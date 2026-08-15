#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/uart.h>
#include <hal/uart_ll.h>
#include <soc/gpio_sig_map.h>
#include <esp_system.h>

#include "channel2_ui.h"
#include "binding_resolver.h"
#include "config.h"
#include "crsf_channel_output.h"
#include "crsf_management.h"
#include "crsf_telemetry.h"
#include "mcp23017_input.h"
#include "physical_input_runtime.h"
#include "r1_input_map.h"

namespace {

HardwareSerial rangerSerial(1);
HardwareSerial rangerRxSerial(2);

constexpr uart_port_t RANGER_UART = UART_NUM_1;
constexpr uint32_t CONTROL_TASK_PERIOD_MS = 4;

struct ControlStateSnapshot {
    uint16_t adc = THROTTLE_ADC_CENTER;
    int16_t rawPermille = 0;
    int16_t outputPermille = 0;
    uint16_t requestedCrsf = CRSF_CHANNEL_MID;
    uint16_t transmittedCrsf = CRSF_CHANNEL_MID;
    OutputSafetyStatus safetyStatus = OutputSafetyStatus::Cut;
    bool calibrating = false;
    bool inputValid = true;
};

struct SharedChannelOutputState {
    CrsfChannelOutputModel model;
    bool live = false;
    uint32_t updatedUs = 0;
};

uint16_t filteredAdc = THROTTLE_ADC_CENTER;
uint32_t filteredTxBatteryAdcMv = 0;
uint32_t nextTxBatterySampleMs = 0;
bool throttleArmed = false;
bool wasCalibrating = false;
uint32_t neutralSinceMs = 0;
uint32_t nextFrameUs = 0;
uint32_t nextLogMs = 0;
Channel2Ui channel2Ui;
CrsfTelemetryParser telemetryParser;
BindingResolver bindingResolver;
ResolvedBindingActionBuffer bindingActions;
Mcp23017Input inputExpander(MCP23017_I2C_ADDRESS);
PhysicalInputRuntime physicalInputs;
PhysicalInputEventBuffer physicalInputEvents;
portMUX_TYPE bindingResolverMux = portMUX_INITIALIZER_UNLOCKED;
uint32_t nextExpanderPollMs = 0;
uint32_t nextExpanderProbeMs = 0;
uint16_t expanderPorts = 0xffff;
bool inputExpanderReady = false;
volatile uint32_t crsfBusEdges = 0;
volatile bool sharedControlStale = true;
SharedChannelOutputState sharedChannelOutputs;
portMUX_TYPE channelOutputMux = portMUX_INITIALIZER_UNLOCKED;
CrsfTelemetryState telemetrySnapshot;
portMUX_TYPE telemetryMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE managementMux = portMUX_INITIALIZER_UNLOCKED;
ElrsManagementRequest pendingManagementRequest;
bool managementRequestPending = false;
TaskHandle_t crsfTaskHandle = nullptr;
TaskHandle_t controlTaskHandle = nullptr;
ControlStateSnapshot controlStateSnapshot;
portMUX_TYPE controlStateMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool controlResetRequested = false;
char usbCommand[32];
uint8_t usbCommandLength = 0;

uint64_t readDirectGpioLevels()
{
    uint64_t levels = 0;
    for (size_t index = 0; index < kR1InputMapCount; ++index) {
        const PhysicalInputHardwareBinding &binding = kR1InputMap[index];
        const InputEndpoint endpoints[] = {binding.primary, binding.secondary};
        for (const InputEndpoint &endpoint : endpoints) {
            if (endpoint.source == InputSourceKind::DirectGpio &&
                digitalRead(endpoint.sourceIndex) != LOW)
                levels |= uint64_t{1} << endpoint.sourceIndex;
        }
    }
    return levels;
}

void configureDirectGpioInputs()
{
    uint64_t configured = 0;
    for (size_t index = 0; index < kR1InputMapCount; ++index) {
        const PhysicalInputHardwareBinding &binding = kR1InputMap[index];
        const InputEndpoint endpoints[] = {binding.primary, binding.secondary};
        for (const InputEndpoint &endpoint : endpoints) {
            if (endpoint.source != InputSourceKind::DirectGpio) continue;
            const uint64_t mask = uint64_t{1} << endpoint.sourceIndex;
            if ((configured & mask) != 0) continue;
            pinMode(endpoint.sourceIndex, INPUT_PULLUP);
            configured |= mask;
        }
    }
}

BindingLoadResult installUiBindings()
{
    ControlBinding bindings[kPhysicalControlCount];
    const size_t count = channel2Ui.copyBindings(bindings, kPhysicalControlCount);
    portENTER_CRITICAL(&bindingResolverMux);
    const BindingLoadResult result = bindingResolver.setBindings(bindings, count);
    portEXIT_CRITICAL(&bindingResolverMux);
    return result;
}

void IRAM_ATTR countCrsfBusEdge()
{
    ++crsfBusEdges;
}

void updateNeutralInterlock(uint16_t adc)
{
    if (throttleArmed) {
        return;
    }

    const int delta = static_cast<int>(adc) -
                      static_cast<int>(channel2Ui.throttleCentreAdc());
    if (abs(delta) <= ARM_NEUTRAL_ADC_WINDOW) {
        if (neutralSinceMs == 0) {
            neutralSinceMs = millis();
        } else if (millis() - neutralSinceMs >= ARM_NEUTRAL_TIME_MS) {
            throttleArmed = true;
            Serial.println("ARMED: throttle input is now being transmitted");
        }
    } else {
        neutralSinceMs = 0;
    }
}

OutputSafetyStatus updateTriggerSafety(uint16_t adc, bool calibrationActive,
                                      bool inputValid)
{
    // Calibration is handled by ProportionalInput. This is only the output
    // safety policy: never arm during calibration and require neutral again
    // after calibration finishes.
    if (wasCalibrating && !calibrationActive) {
        throttleArmed = false;
        neutralSinceMs = 0;
    }
    wasCalibrating = calibrationActive;
    if (calibrationActive) {
        throttleArmed = false;
        neutralSinceMs = 0;
        return OutputSafetyStatus::Calibration;
    }
    if (!inputValid) {
        throttleArmed = false;
        neutralSinceMs = 0;
        return OutputSafetyStatus::InputFault;
    }
    if (!channel2Ui.throttleEnabled()) {
        throttleArmed = false;
        neutralSinceMs = 0;
        return OutputSafetyStatus::ChannelDisabled;
    }
    if (!channel2Ui.armRequested()) {
        throttleArmed = false;
        neutralSinceMs = 0;
        return OutputSafetyStatus::Cut;
    }
    updateNeutralInterlock(adc);
    return throttleArmed ? OutputSafetyStatus::Armed
                         : OutputSafetyStatus::NeutralRequired;
}

void setCrsfBusReceive()
{
    // The Saleae capture of the actual Nano-adapter bus decodes as ordinary
    // non-inverted 400 kbaud UART (idle high, falling start bit). Release the
    // output driver and attach that physical polarity directly to UART RX.
    gpio_set_direction(CRSF_SIGNAL_PIN, GPIO_MODE_INPUT);
    gpio_matrix_in(CRSF_SIGNAL_PIN, U2RXD_IN_IDX, true);
    gpio_pullup_en(CRSF_SIGNAL_PIN);
    gpio_pulldown_dis(CRSF_SIGNAL_PIN);
}

void setCrsfBusTransmit()
{
    // UART2 remains attached as a passive listener. It receives a clean echo
    // of our outbound frame as well as the Nano's replies; the parser safely
    // ignores outbound frame types. Keeping it attached avoids losing the
    // beginning of a fast Nano response during a GPIO-matrix handover.
    gpio_set_pull_mode(CRSF_SIGNAL_PIN, GPIO_FLOATING);
    gpio_set_level(CRSF_SIGNAL_PIN, 0);
    gpio_set_direction(CRSF_SIGNAL_PIN, GPIO_MODE_OUTPUT);
    gpio_matrix_out(CRSF_SIGNAL_PIN, U1TXD_OUT_IDX, true, false);
}

void transmitCrsfFrame(const uint8_t *frame, size_t length)
{
    setCrsfBusTransmit();
    rangerSerial.write(frame, length);
    // Poll the hardware directly. Waiting through the UART driver's event path
    // leaves the line driven for too long and can hide the Nano's immediate
    // half-duplex response.
    while (!uart_ll_is_tx_idle(UART_LL_GET_HW(RANGER_UART))) {
    }
    setCrsfBusReceive();
}

void sendChannelsFrame(const CrsfChannelOutputModel &model, bool live)
{
    uint8_t frame[CRSF_FRAME_BYTES];
    buildCrsfChannelsFrame(model, live, frame);
    transmitCrsfFrame(frame, sizeof(frame));
}

bool queueManagementRequest(const ElrsManagementRequest &request)
{
    bool queued = false;
    portENTER_CRITICAL(&managementMux);
    if (!managementRequestPending) {
        pendingManagementRequest = request;
        managementRequestPending = true;
        queued = true;
    }
    portEXIT_CRITICAL(&managementMux);
    return queued;
}

bool takeManagementRequest(ElrsManagementRequest &request)
{
    bool available = false;
    portENTER_CRITICAL(&managementMux);
    if (managementRequestPending) {
        request = pendingManagementRequest;
        managementRequestPending = false;
        available = true;
    }
    portEXIT_CRITICAL(&managementMux);
    return available;
}

void sendManagementRequest(const ElrsManagementRequest &request)
{
    uint8_t frame[64];
    const size_t length = buildElrsManagementFrame(request, frame, sizeof(frame));
    if (length == 0) return;
    transmitCrsfFrame(frame, length);
    Serial.printf("ELRS MGMT: sent %s to %s (%u-byte phrase)\n",
        request.action == ElrsManagementAction::ReadUid ? "UID request" : "bindphrase",
        request.target == ElrsManagementTarget::Transmitter ? "TX module" : "receiver",
        request.phraseLength);
}

void readReturnedTelemetry()
{
    // This runs in the dedicated CRSF task. A generous bound prevents a bad
    // electrical stream from monopolising Core 0 while still draining normal
    // channel echoes and module telemetry faster than they can arrive.
    uint16_t budget = 2048;
    while (budget-- > 0 && rangerRxSerial.available() > 0) {
        const uint8_t value = static_cast<uint8_t>(rangerRxSerial.read());
        telemetryParser.feed(value);
    }
    telemetryParser.updateFreshness();
}

void publishTelemetrySnapshot()
{
    portENTER_CRITICAL(&telemetryMux);
    telemetrySnapshot = telemetryParser.state();
    portEXIT_CRITICAL(&telemetryMux);
}

CrsfTelemetryState readTelemetrySnapshot()
{
    CrsfTelemetryState snapshot;
    portENTER_CRITICAL(&telemetryMux);
    snapshot = telemetrySnapshot;
    portEXIT_CRITICAL(&telemetryMux);
    return snapshot;
}

void publishChannelOutputs(const CrsfChannelOutputModel &model, bool live,
                           uint32_t updatedUs)
{
    portENTER_CRITICAL(&channelOutputMux);
    sharedChannelOutputs.model = model;
    sharedChannelOutputs.live = live;
    sharedChannelOutputs.updatedUs = updatedUs;
    portEXIT_CRITICAL(&channelOutputMux);
}

SharedChannelOutputState readChannelOutputs()
{
    SharedChannelOutputState snapshot;
    portENTER_CRITICAL(&channelOutputMux);
    snapshot = sharedChannelOutputs;
    portEXIT_CRITICAL(&channelOutputMux);
    return snapshot;
}

void forceChannelOutputsSafe()
{
    portENTER_CRITICAL(&channelOutputMux);
    sharedChannelOutputs.live = false;
    sharedChannelOutputs.updatedUs = micros();
    portEXIT_CRITICAL(&channelOutputMux);
}

void requestControlReset()
{
    controlResetRequested = true;
    forceChannelOutputsSafe();
}

void publishControlState(const ControlStateSnapshot &snapshot)
{
    portENTER_CRITICAL(&controlStateMux);
    controlStateSnapshot = snapshot;
    portEXIT_CRITICAL(&controlStateMux);
}

ControlStateSnapshot readControlState()
{
    ControlStateSnapshot snapshot;
    portENTER_CRITICAL(&controlStateMux);
    snapshot = controlStateSnapshot;
    portEXIT_CRITICAL(&controlStateMux);
    return snapshot;
}

void controlTask(void *)
{
    TickType_t nextWake = xTaskGetTickCount();
    for (;;) {
        if (controlResetRequested) {
            controlResetRequested = false;
            throttleArmed = false;
            neutralSinceMs = 0;
            channel2Ui.resetChannelSpeed();
        }

        const uint16_t sample = analogRead(THROTTLE_ADC_PIN);
        filteredAdc = static_cast<uint16_t>((filteredAdc * 7U + sample) / 8U);
        const bool calibrating = channel2Ui.calibrationActive();
        const bool inputValid = filteredAdc >= THROTTLE_ADC_VALID_MIN &&
                                filteredAdc <= THROTTLE_ADC_VALID_MAX;
        const OutputSafetyStatus safetyStatus = updateTriggerSafety(
            filteredAdc, calibrating, inputValid);

        const int16_t rawThrottle = channel2Ui.mapAdcToPermille(filteredAdc);
        const int16_t responseThrottle = channel2Ui.applySettings(rawThrottle);
        const int16_t modeThrottle = channel2Ui.applyDriveMode(
            responseThrottle, rawThrottle);
        const int16_t adjustedThrottle = channel2Ui.applyChannelAdjustments(
            channel2Ui.throttleChannel(), modeThrottle);
        const uint16_t safeNeutralThrottle = crsfValueFromPermille(
            channel2Ui.neutralOutputPermille());
        int16_t configuredThrottle = 0;
        if (safetyStatus == OutputSafetyStatus::Armed)
            configuredThrottle = channel2Ui.applyChannelSpeed(adjustedThrottle, millis());
        else
            channel2Ui.resetChannelSpeed();

        const uint16_t requestedThrottle = crsfValueFromPermille(configuredThrottle);
        const uint16_t transmittedThrottle =
            (throttleArmed && !calibrating) ? requestedThrottle : safeNeutralThrottle;
        CrsfChannelOutputModel outputs;
        bindingActions.clear();
        portENTER_CRITICAL(&bindingResolverMux);
        bindingResolver.update(millis(), bindingActions);
        bindingResolver.applyChannelOutputs(outputs,
            static_cast<uint16_t>(
                (1U << (channel2Ui.throttleChannel() - 1)) |
                (1U << (channel2Ui.steeringChannel() - 1))));
        portEXIT_CRITICAL(&bindingResolverMux);
        // Steering is centred until its 5 kOhm pot is connected, but channel
        // trim/subtrim already operate through the same role-owned pipeline.
        outputs.configure(channel2Ui.steeringChannel(),
            crsfValueFromPermille(channel2Ui.channelNeutralPermille(
                channel2Ui.steeringChannel())),
            crsfValueFromPermille(channel2Ui.channelNeutralPermille(
                channel2Ui.steeringChannel())),
            channel2Ui.steeringEnabled());
        // The dedicated safety-checked throttle path has final ownership of
        // its configured channel, even if a future profile is malformed.
        outputs.configure(channel2Ui.throttleChannel(), requestedThrottle,
                          safeNeutralThrottle, channel2Ui.throttleEnabled());
        const uint32_t controlUpdateUs = micros();
        publishChannelOutputs(outputs,
                              safetyStatus == OutputSafetyStatus::Armed,
                              controlUpdateUs);

        ControlStateSnapshot snapshot;
        snapshot.adc = filteredAdc;
        snapshot.rawPermille = rawThrottle;
        snapshot.outputPermille = throttleArmed
            ? configuredThrottle
            : channel2Ui.neutralOutputPermille();
        snapshot.requestedCrsf = requestedThrottle;
        snapshot.transmittedCrsf = transmittedThrottle;
        snapshot.safetyStatus = safetyStatus;
        snapshot.calibrating = calibrating;
        snapshot.inputValid = inputValid;
        publishControlState(snapshot);

        vTaskDelayUntil(&nextWake, pdMS_TO_TICKS(CONTROL_TASK_PERIOD_MS));
    }
}

void crsfTask(void *)
{
    nextFrameUs = micros();
    uint32_t nextSnapshotMs = millis();
    bool uidFollowupPending = false;
    uint32_t uidFollowupDueMs = 0;
    ElrsManagementTarget uidFollowupTarget = ElrsManagementTarget::Transmitter;
    for (;;) {
        const uint32_t nowUs = micros();
        if (static_cast<int32_t>(nowUs - nextFrameUs) >= 0) {
            // Do not send a burst after a scheduling hiccup; resume at the
            // current 250 Hz phase with the latest requested channel value.
            nextFrameUs = nowUs + CRSF_FRAME_PERIOD_US;
            const SharedChannelOutputState outputs = readChannelOutputs();
            const uint32_t lastControlUs = outputs.updatedUs;
            const bool stale = lastControlUs == 0 ||
                static_cast<uint32_t>(nowUs - lastControlUs) > CONTROL_STALE_TIMEOUT_US;
            sharedControlStale = stale;
            ElrsManagementRequest management;
            const bool requested = takeManagementRequest(management);
            const bool followup = !requested && uidFollowupPending &&
                static_cast<int32_t>(millis() - uidFollowupDueMs) >= 0;
            if (requested || followup) {
                if (followup) {
                    management = ElrsManagementRequest{};
                    management.action = ElrsManagementAction::ReadUid;
                    management.target = uidFollowupTarget;
                    uidFollowupPending = false;
                }
                sendManagementRequest(management);
                if (management.action == ElrsManagementAction::SetBindPhrase) {
                    // ExpressLRS schedules a reboot 200 ms after changing its
                    // UID. Give the endpoint ample time, then verify the UID.
                    uidFollowupTarget = management.target;
                    uidFollowupDueMs = millis() + 1200;
                    uidFollowupPending = true;
                }
            } else
                sendChannelsFrame(outputs.model, outputs.live && !stale);
        }
        readReturnedTelemetry();
        const uint32_t nowMs = millis();
        if (static_cast<int32_t>(nowMs - nextSnapshotMs) >= 0) {
            nextSnapshotMs = nowMs + 20;
            publishTelemetrySnapshot();
        }
        vTaskDelay(1);
    }
}

bool consumeScreenshotCommand()
{
    while (Serial.available() > 0) {
        const char character = static_cast<char>(Serial.read());
        if (character == '\r') continue;
        if (character == '\n') {
            usbCommand[usbCommandLength] = '\0';
            const bool matched = strcmp(usbCommand, "SCREENSHOT") == 0;
            usbCommandLength = 0;
            if (matched) return true;
            continue;
        }
        if (usbCommandLength + 1 < sizeof(usbCommand))
            usbCommand[usbCommandLength++] = character;
        else
            usbCommandLength = 0;
    }
    return false;
}

} // namespace

void setup()
{
    Serial.begin(115200);
    delay(700);
    Serial.println();
    Serial.println("JR R1 native CRSF throttle test");
    Serial.printf("BOOT: reset_reason=%u free_heap=%u bytes\n",
                  static_cast<unsigned>(esp_reset_reason()),
                  static_cast<unsigned>(ESP.getFreeHeap()));
    Serial.printf("MEMORY: heap=%u free=%u min_free=%u largest=%u "
                  "psram=%u free_psram=%u\n",
                  static_cast<unsigned>(ESP.getHeapSize()),
                  static_cast<unsigned>(ESP.getFreeHeap()),
                  static_cast<unsigned>(ESP.getMinFreeHeap()),
                  static_cast<unsigned>(ESP.getMaxAllocHeap()),
                  static_cast<unsigned>(ESP.getPsramSize()),
                  static_cast<unsigned>(ESP.getFreePsram()));
    Serial.println("WARNING: disconnect the motor / raise the wheels for first test");

    analogReadResolution(12);
    analogSetPinAttenuation(THROTTLE_ADC_PIN, ADC_11db);
    analogSetPinAttenuation(TX_BATTERY_ADC_PIN, ADC_11db);

    // UART1 drives the one-wire CRSF bus. UART2 passively samples the same pin;
    // separating TX and RX avoids remapping a live UART input at every 250 Hz
    // direction change.
    rangerSerial.begin(CRSF_BAUD, SERIAL_8N1, -1, CRSF_SIGNAL_PIN, false);
    rangerSerial.setTimeout(0);
    rangerRxSerial.setRxBufferSize(2048);
    rangerRxSerial.begin(CRSF_BAUD, SERIAL_8N1, CRSF_SIGNAL_PIN, -1, false);
    rangerRxSerial.setTimeout(0);
    setCrsfBusReceive();
    attachInterrupt(digitalPinToInterrupt(CRSF_SIGNAL_PIN), countCrsfBusEdge, CHANGE);

    xTaskCreatePinnedToCore(crsfTask, "crsf-link", 4096, nullptr, 4,
                            &crsfTaskHandle, 0);

    const uint32_t heapBeforeUi = ESP.getFreeHeap();
    channel2Ui.begin();
    Serial.printf("MEMORY: UI init used=%u heap_free=%u min_free=%u "
                  "largest=%u free_psram=%u\n",
                  static_cast<unsigned>(heapBeforeUi - ESP.getFreeHeap()),
                  static_cast<unsigned>(ESP.getFreeHeap()),
                  static_cast<unsigned>(ESP.getMinFreeHeap()),
                  static_cast<unsigned>(ESP.getMaxAllocHeap()),
                  static_cast<unsigned>(ESP.getFreePsram()));
    const bool inputMapReady = physicalInputs.begin(kR1InputMap, kR1InputMapCount);
    configureDirectGpioInputs();
    Serial.printf("INPUT MAP: %u controls, one MCP23017 + six direct GPIO: %s\n",
                  static_cast<unsigned>(kR1InputMapCount),
                  inputMapReady ? "ready" : "INVALID");
    const BindingLoadResult bindingResult = installUiBindings();
    Serial.printf("BINDINGS: installed %u assignments: %s\n",
                  static_cast<unsigned>(bindingResolver.bindingCount()),
                  bindingResult.ok() ? "ready" : bindingResult.reason);
    inputExpanderReady = inputExpander.begin();
    Serial.printf("MCP23017: address=0x%02X %s; PA0 is Timer button\n",
                  MCP23017_I2C_ADDRESS,
                  inputExpanderReady ? "ONLINE" : "NOT FOUND");
    filteredAdc = analogRead(THROTTLE_ADC_PIN);
    publishControlState(ControlStateSnapshot());
    xTaskCreatePinnedToCore(controlTask, "control-input", 4096, nullptr, 5,
                            &controlTaskHandle, 1);
    nextLogMs = millis();
}

void loop()
{
    if (consumeScreenshotCommand()) {
        // Force the whole channel model safe immediately; the CRSF task must
        // never retain live outputs while the UI task streams a screenshot.
        requestControlReset();
        channel2Ui.captureScreenshot(Serial);
        nextLogMs = millis() + USB_LOG_PERIOD_MS;
        return;
    }

    const ControlStateSnapshot control = readControlState();
    const CrsfTelemetryState telemetry = readTelemetrySnapshot();

    uint16_t txVoltageDecivolts = 0;
    bool txVoltageValid = false;
    const uint32_t nowMs = millis();
    if (!inputExpanderReady &&
        static_cast<int32_t>(nowMs - nextExpanderProbeMs) >= 0) {
        nextExpanderProbeMs = nowMs + 1000;
        inputExpanderReady = inputExpander.begin();
        if (inputExpanderReady)
            Serial.printf("MCP23017: reconnected at 0x%02X\n",
                          MCP23017_I2C_ADDRESS);
    }
    if (static_cast<int32_t>(nowMs - nextExpanderPollMs) >= 0) {
        nextExpanderPollMs = nowMs + MCP23017_POLL_PERIOD_MS;
        uint16_t ports = 0xffff; // fail-safe released state for active-low inputs
        if (inputExpanderReady && !inputExpander.read(ports)) {
            inputExpanderReady = false;
            nextExpanderProbeMs = nowMs + 1000;
            Serial.printf("MCP23017: read failed at 0x%02X; inputs disabled\n",
                          MCP23017_I2C_ADDRESS);
        }
        expanderPorts = ports;
        physicalInputEvents.clear();
        physicalInputs.update(ports, readDirectGpioLevels(), nowMs,
                              physicalInputEvents, inputExpanderReady);
        physicalInputs.setProportionalValue(PhysicalControlId::ThrottleTrigger,
                                            control.rawPermille, nowMs);
        if (physicalInputEvents.size() != 0) {
            ResolvedBindingActionBuffer actions;
            portENTER_CRITICAL(&bindingResolverMux);
            bindingResolver.processEvents(physicalInputEvents,
                static_cast<uint8_t>(ContextAlways | channel2Ui.activeControlContext() |
                    (throttleArmed ? ContextArmed : ContextCut)), actions);
            portEXIT_CRITICAL(&bindingResolverMux);
            for (size_t index = 0; index < actions.size(); ++index) {
                const ResolvedBindingAction &action = actions[index];
                if (action.target == AssignmentTarget::ModelParameter)
                    channel2Ui.applyChannelParameterAction(action.targetId,
                                                            action.value);
                else if (action.target == AssignmentTarget::TransmitterFunction)
                    channel2Ui.applyTransmitterFunctionAction(action.targetId,
                                                               action.value);
                else if (action.target == AssignmentTarget::DriveMode)
                    channel2Ui.applyDriveModeAction(action.targetId,
                                                    action.value);
            }
            for (size_t index = 0; index < physicalInputEvents.size(); ++index) {
                const PhysicalInputEvent &inputEvent = physicalInputEvents[index];
                Serial.printf("INPUT: %s event=%s value=%d source=%s\n",
                              physicalControlDescriptor(
                                  static_cast<size_t>(inputEvent.input)).name,
                              inputEventName(inputEvent.event), inputEvent.value,
                              r1InputEndpointLabel(inputEvent.input));
            }
            if (physicalInputEvents.overflowed())
                Serial.println("INPUT: event buffer overflow");
        }
    }
    channel2Ui.setPhysicalInputStates(physicalInputs.states());
    if (channel2Ui.txVoltageSenseEnabled()) {
        if (static_cast<int32_t>(nowMs - nextTxBatterySampleMs) >= 0) {
            nextTxBatterySampleMs = nowMs + 20;
            const uint32_t sampleMv = analogReadMilliVolts(TX_BATTERY_ADC_PIN);
            filteredTxBatteryAdcMv = filteredTxBatteryAdcMv == 0
                ? sampleMv
                : (filteredTxBatteryAdcMv * 7U + sampleMv) / 8U;
        }
        const uint32_t packMv = filteredTxBatteryAdcMv *
            (TX_BATTERY_DIVIDER_TOP_OHMS + TX_BATTERY_DIVIDER_BOTTOM_OHMS) /
            TX_BATTERY_DIVIDER_BOTTOM_OHMS;
        txVoltageValid = packMv >= 2500;
        txVoltageDecivolts = static_cast<uint16_t>((packMv + 50U) / 100U);
    } else {
        filteredTxBatteryAdcMv = 0;
    }
    const SharedChannelOutputState channelOutputs = readChannelOutputs();
    channel2Ui.update(control.adc, control.rawPermille,
                      control.outputPermille,
                      control.transmittedCrsf, control.safetyStatus,
                      telemetry, txVoltageDecivolts, txVoltageValid,
                      channelOutputs.model,
                      channelOutputs.live && !sharedControlStale);
    ElrsManagementRequest management;
    if (channel2Ui.consumeElrsManagementRequest(management)) {
        requestControlReset();
        if (!queueManagementRequest(management))
            Serial.println("ELRS MGMT: queue busy; request discarded");
    }
    if (channel2Ui.consumeBindingsChanged()) {
        // CUT the complete output model before replacing resolver state. The
        // new mapping cannot reach the RF task until the neutral interlock is
        // satisfied again.
        requestControlReset();
        const BindingLoadResult result = installUiBindings();
        Serial.printf("BINDINGS: TFT profile reload %s (%u assignments)\n",
                      result.ok() ? "ready" :
                          (result.reason == nullptr ? "FAILED" : result.reason),
                      static_cast<unsigned>(bindingResolver.bindingCount()));
    }
    if (channel2Ui.consumeChannelRolesChanged()) {
        // Disable all live outputs before the next atomic model snapshot moves
        // throttle to its newly selected channel.
        requestControlReset();
        Serial.printf("SAFE: throttle role changed to CH%u; release trigger to re-arm\n",
                      channel2Ui.throttleChannel());
    }
    if (channel2Ui.consumeSafetyResetRequested()) {
        requestControlReset();
        Serial.println("SAFE: mode/assignment/arm state changed; output reset to neutral");
    }

    if (static_cast<int32_t>(nowMs - nextLogMs) >= 0) {
        nextLogMs += USB_LOG_PERIOD_MS;
        const int neutralDelta = static_cast<int>(control.adc) -
                                 static_cast<int>(channel2Ui.throttleCentreAdc());
        Serial.printf("adc=%4u ctr=%4u delta=%+4d cal=%s neutralFor=%lums THR=CH%u requested=%4u sent=%4u armed=%s stale=%s drive=%s TIMER=%s MCP=%s link=%s LQ=%u RSSI=%d TX=%s%u.%uV RX=%s%u.%uV edges=%lu bytes=%lu frames=%lu crc=%lu\n",
                      control.adc,
                      channel2Ui.throttleCentreAdc(),
                      neutralDelta,
                      control.calibrating ? "yes" : "no",
                      static_cast<unsigned long>(neutralSinceMs == 0 ? 0 : nowMs - neutralSinceMs),
                      channel2Ui.throttleChannel(),
                      control.requestedCrsf,
                      control.transmittedCrsf,
                      throttleArmed ? "yes" : "NO",
                      sharedControlStale ? "YES" : "no",
                      channel2Ui.driveMode() == DriveMode::Walk ? "WALK" : "NORMAL",
                      physicalInputs.state(PhysicalControlId::TimerButton).stablePrimary
                          ? "ON" : "off",
                      inputExpanderReady ? "ok" : "OFFLINE",
                      telemetry.linkValid ? "yes" : "no",
                      telemetry.uplinkLinkQuality,
                      telemetry.uplinkRssiDbm,
                      txVoltageValid ? "" : "--/",
                      txVoltageDecivolts / 10,
                      txVoltageDecivolts % 10,
                      telemetry.receiverVoltageValid ? "" : "--/",
                      telemetry.receiverVoltageDecivolts / 10,
                      telemetry.receiverVoltageDecivolts % 10,
                      static_cast<unsigned long>(crsfBusEdges),
                      static_cast<unsigned long>(telemetry.rawBytes),
                      static_cast<unsigned long>(telemetry.validFrames),
                      static_cast<unsigned long>(telemetry.crcErrors));
    }
}
