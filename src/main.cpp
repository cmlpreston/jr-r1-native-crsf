#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/uart.h>
#include <hal/uart_ll.h>
#include <soc/gpio_pins.h>
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

constexpr uart_port_t RANGER_UART = UART_NUM_1;
constexpr uint32_t CONTROL_TASK_PERIOD_MS = 4;
constexpr uint32_t STARTUP_INPUT_SETTLE_MS = 400;

struct ControlStateSnapshot {
    uint16_t rawAdc = THROTTLE_ADC_CENTER;
    uint16_t adc = THROTTLE_ADC_CENTER;
    int16_t rawPermille = 0;
    int16_t outputPermille = 0;
    uint16_t requestedCrsf = CRSF_CHANNEL_MID;
    uint16_t transmittedCrsf = CRSF_CHANNEL_MID;
    OutputSafetyStatus safetyStatus = OutputSafetyStatus::Cut;
    bool calibrating = false;
    bool inputValid = true;
    uint32_t maximumTaskGapUs = 0;
};

struct SharedChannelOutputState {
    CrsfChannelOutputModel model;
    bool live = false;
    uint32_t updatedUs = 0;
};

ProportionalInputRuntime throttleInputRuntime;
uint32_t filteredTxBatteryAdcMv = 0;
uint32_t nextTxBatterySampleMs = 0;
bool throttleArmed = false;
bool wasCalibrating = false;
uint32_t neutralSinceMs = 0;
uint32_t linkReadySinceMs = 0;
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
volatile uint32_t crsfChannelFramesSent = 0;
volatile uint32_t crsfTransmitTimeouts = 0;
volatile uint32_t crsfMaximumTransmitWaitUs = 0;
volatile uint32_t crsfMaximumTaskGapUs = 0;
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
volatile bool startupPreflightComplete = false;
uint32_t startupInputSettledAtMs = 0;
uint32_t startupWarningMask = 0;
char usbCommand[32];
uint8_t usbCommandLength = 0;

const char *safetyStatusName(OutputSafetyStatus status)
{
    switch (status) {
        case OutputSafetyStatus::Cut: return "CUT";
        case OutputSafetyStatus::StartupWarning: return "STARTUP";
        case OutputSafetyStatus::LinkRequired: return "LINK_WAIT";
        case OutputSafetyStatus::NeutralRequired: return "NEUTRAL";
        case OutputSafetyStatus::Armed: return "LIVE";
        case OutputSafetyStatus::Calibration: return "CAL";
        case OutputSafetyStatus::InputFault: return "INPUT_FAULT";
        case OutputSafetyStatus::ControlStale: return "STALE";
        case OutputSafetyStatus::ChannelDisabled: return "CH_OFF";
    }
    return "UNKNOWN";
}

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

bool updateLinkInterlock(const CrsfTelemetryState &telemetry)
{
    // Once armed, link telemetry is advisory only. Dropping the transmitted
    // command merely because a return packet was delayed would itself create
    // a hazardous control transient; RF-loss behaviour belongs in the RX
    // failsafe configuration.
    if (throttleArmed) return true;

    const uint32_t now = millis();
    const bool acceptable = telemetry.linkValid &&
                            telemetry.uplinkLinkQuality >= ARM_MIN_LINK_QUALITY;
    if (!acceptable) {
        linkReadySinceMs = 0;
        return false;
    }
    if (linkReadySinceMs == 0) linkReadySinceMs = now;
    return now - linkReadySinceMs >= ARM_LINK_STABLE_TIME_MS;
}

OutputSafetyStatus updateTriggerSafety(uint16_t adc, bool calibrationActive,
                                      bool inputValid,
                                      const CrsfTelemetryState &telemetry)
{
    // Calibration is handled by ProportionalInput. This is only the output
    // safety policy: never arm during calibration and require neutral again
    // after calibration finishes.
    if (wasCalibrating && !calibrationActive) {
        throttleArmed = false;
        neutralSinceMs = 0;
        linkReadySinceMs = 0;
    }
    wasCalibrating = calibrationActive;
    if (calibrationActive) {
        throttleArmed = false;
        neutralSinceMs = 0;
        linkReadySinceMs = 0;
        return OutputSafetyStatus::Calibration;
    }
    if (!inputValid) {
        throttleArmed = false;
        neutralSinceMs = 0;
        linkReadySinceMs = 0;
        return OutputSafetyStatus::InputFault;
    }
    if (!channel2Ui.throttleEnabled()) {
        throttleArmed = false;
        neutralSinceMs = 0;
        linkReadySinceMs = 0;
        return OutputSafetyStatus::ChannelDisabled;
    }
    if (!startupPreflightComplete) {
        throttleArmed = false;
        neutralSinceMs = 0;
        linkReadySinceMs = 0;
        return OutputSafetyStatus::StartupWarning;
    }
    if (!channel2Ui.armRequested()) {
        throttleArmed = false;
        neutralSinceMs = 0;
        linkReadySinceMs = 0;
        return OutputSafetyStatus::Cut;
    }
    if (!updateLinkInterlock(telemetry)) {
        // Neutral qualification starts only after the returned RF link has
        // remained acceptable for its complete dwell period.
        neutralSinceMs = 0;
        return OutputSafetyStatus::LinkRequired;
    }
    updateNeutralInterlock(adc);
    return throttleArmed ? OutputSafetyStatus::Armed
                         : OutputSafetyStatus::NeutralRequired;
}

void setCrsfBusReceive()
{
    // One UART owns both directions of the Ranger Nano's single-wire CRSF
    // connection. Disconnect its TX output before releasing the GPIO, then
    // attach UART1 RX. This changes only the GPIO matrix routing: the UART
    // driver and its RX ring buffer remain installed continuously.
    gpio_matrix_out(CRSF_SIGNAL_PIN, SIG_GPIO_OUT_IDX, false, false);
    gpio_set_direction(CRSF_SIGNAL_PIN, GPIO_MODE_INPUT);
    // The Ranger adapter path is empirically decoded through the GPIO-matrix
    // inversion. Disabling this produces no valid return frames, even though
    // the waveform observed outside the adapter is conventional idle-high
    // UART; retain the proven setting here.
    gpio_matrix_in(CRSF_SIGNAL_PIN, U1RXD_IN_IDX, true);
    gpio_pullup_en(CRSF_SIGNAL_PIN);
    gpio_pulldown_dis(CRSF_SIGNAL_PIN);
}

void setCrsfBusTransmit()
{
    // Stop UART1 receiving before its transmitter owns the half-duplex wire.
    // Constant zero plus input inversion presents the detached receiver with
    // idle-high, preventing self-echo and framing-error traffic in its FIFO.
    gpio_matrix_in(GPIO_MATRIX_CONST_ZERO_INPUT, U1RXD_IN_IDX, true);
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
    const uint32_t waitStartedUs = micros();
    while (!uart_ll_is_tx_idle(UART_LL_GET_HW(RANGER_UART)) &&
           static_cast<uint32_t>(micros() - waitStartedUs) < 2000) {
    }
    const uint32_t waitedUs = micros() - waitStartedUs;
    if (waitedUs > crsfMaximumTransmitWaitUs)
        crsfMaximumTransmitWaitUs = waitedUs;
    if (!uart_ll_is_tx_idle(UART_LL_GET_HW(RANGER_UART)))
        ++crsfTransmitTimeouts;
    setCrsfBusReceive();
}

void sendChannelsFrame(const CrsfChannelOutputModel &model, bool live)
{
    uint8_t frame[CRSF_FRAME_BYTES];
    buildCrsfChannelsFrame(model, live, frame);
    transmitCrsfFrame(frame, sizeof(frame));
    ++crsfChannelFramesSent;
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
    // Keep returned telemetry subordinate to the next channel deadline. A
    // noisy one-wire bus can otherwise keep available() permanently non-zero
    // and make one invocation spend milliseconds draining the UART FIFO.
    // At 400 kbaud only about 40 bytes arrive per millisecond, so 128 bytes per
    // pass still drains a healthy stream faster than real time.
    // Do not call HardwareSerial::read() once per byte here. On a noisy or
    // self-echoing half-duplex line each call enters the UART driver and takes
    // its lock; draining 128 bytes that way has produced measured ~129 ms
    // CRSF task stalls. One non-blocking driver read keeps this work bounded.
    uint8_t bytes[128];
    size_t buffered = 0;
    if (uart_get_buffered_data_len(RANGER_UART, &buffered) == ESP_OK &&
        buffered != 0) {
        const size_t wanted = buffered < sizeof(bytes) ? buffered : sizeof(bytes);
        const int received = uart_read_bytes(RANGER_UART, bytes, wanted, 0);
        for (int index = 0; index < received; ++index)
            telemetryParser.feed(bytes[index]);
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
    uint32_t previousCycleUs = micros();
    uint32_t maximumTaskGapUs = 0;
    OutputSafetyStatus previousSafetyStatus = OutputSafetyStatus::ControlStale;
    for (;;) {
        const uint32_t cycleUs = micros();
        const uint32_t taskGapUs = cycleUs - previousCycleUs;
        previousCycleUs = cycleUs;
        if (taskGapUs > maximumTaskGapUs) maximumTaskGapUs = taskGapUs;
        if (controlResetRequested) {
            controlResetRequested = false;
            throttleArmed = false;
            neutralSinceMs = 0;
            linkReadySinceMs = 0;
            channel2Ui.resetChannelSpeed();
        }

        // Refresh persistent policy without disturbing live filter state. A
        // calibration accepted by the UI therefore takes effect on the next
        // control sample; coherent model snapshots will replace this read in
        // the next architecture stage.
        throttleInputRuntime.setProfile(channel2Ui.throttleInputProfile());
        const ProportionalInputState throttleInput =
            throttleInputRuntime.update(analogRead(THROTTLE_ADC_PIN));
        const uint16_t filteredAdc = throttleInput.filteredAdc;
        const bool calibrating = channel2Ui.calibrationActive();
        const bool inputValid = throttleInput.valid;
        const CrsfTelemetryState telemetry = readTelemetrySnapshot();
        const OutputSafetyStatus safetyStatus = updateTriggerSafety(
            filteredAdc, calibrating, inputValid, telemetry);
        if (safetyStatus != previousSafetyStatus) {
            Serial.printf("SAFETY t=%lums %s -> %s arm_request=%s raw=%u filtered=%u centre=%u delta=%+d\n",
                          static_cast<unsigned long>(millis()),
                          safetyStatusName(previousSafetyStatus),
                          safetyStatusName(safetyStatus),
                          channel2Ui.armRequested() ? "yes" : "no",
                          throttleInput.rawAdc, filteredAdc,
                          channel2Ui.throttleCentreAdc(),
                          static_cast<int>(filteredAdc) -
                              static_cast<int>(channel2Ui.throttleCentreAdc()));
            previousSafetyStatus = safetyStatus;
        }

        const int16_t rawThrottle = throttleInput.normalisedPermille;
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
        const uint16_t roleOwnedChannels = static_cast<uint16_t>(
            (1U << (channel2Ui.throttleChannel() - 1)) |
            (1U << (channel2Ui.steeringChannel() - 1)));
        bindingResolver.applyChannelOutputs(outputs, roleOwnedChannels);
        portEXIT_CRITICAL(&bindingResolverMux);
        // Proportional AUX assignments pass through the same per-channel
        // response profile as steering and throttle. Role-owned channels are
        // completed by their dedicated input/safety paths below.
        channel2Ui.applyProportionalProfiles(outputs, millis(),
                                             roleOwnedChannels);
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
        snapshot.rawAdc = throttleInput.rawAdc;
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
        snapshot.maximumTaskGapUs = maximumTaskGapUs;
        publishControlState(snapshot);

        vTaskDelayUntil(&nextWake, pdMS_TO_TICKS(CONTROL_TASK_PERIOD_MS));
    }
}

void crsfTask(void *)
{
    nextFrameUs = micros();
    uint32_t previousTaskCycleUs = nextFrameUs;
    uint32_t nextSnapshotMs = millis();
    bool uidFollowupPending = false;
    uint32_t uidFollowupDueMs = 0;
    ElrsManagementTarget uidFollowupTarget = ElrsManagementTarget::Transmitter;
    for (;;) {
        const uint32_t nowUs = micros();
        const uint32_t taskGapUs = nowUs - previousTaskCycleUs;
        previousTaskCycleUs = nowUs;
        if (taskGapUs > crsfMaximumTaskGapUs)
            crsfMaximumTaskGapUs = taskGapUs;
        if (static_cast<int32_t>(nowUs - nextFrameUs) >= 0) {
            // Preserve the 250 Hz phase when a normal task iteration reaches
            // the deadline slightly late. Reset only after a genuinely missed
            // complete period, preventing both long-term drift toward ~220 Hz
            // and catch-up bursts after an exceptional scheduling hiccup.
            const uint32_t latenessUs = nowUs - nextFrameUs;
            nextFrameUs = latenessUs >= CRSF_FRAME_PERIOD_US
                ? nowUs + CRSF_FRAME_PERIOD_US
                : nextFrameUs + CRSF_FRAME_PERIOD_US;
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

    // UART1 remains installed with both RX and TX assigned to the one-wire
    // CRSF pin. The GPIO matrix functions above alternate ownership without
    // stopping/restarting the driver, matching the half-duplex strategy used
    // by ExpressLRS itself. Allocate the larger RX buffer before begin().
    rangerSerial.setRxBufferSize(2048);
    rangerSerial.begin(CRSF_BAUD, SERIAL_8N1,
                       CRSF_SIGNAL_PIN, CRSF_SIGNAL_PIN, false);
    rangerSerial.setTimeout(0);
    setCrsfBusReceive();
    Serial.printf("CRSF TRANSPORT: UART1 single-wire half-duplex on GPIO%u\n",
                  static_cast<unsigned>(CRSF_SIGNAL_PIN));
    // Do not attach a CHANGE interrupt to the CRSF signal. At 400 kbaud that
    // diagnostic generated hundreds of thousands of interrupts per second and
    // competed with the UART/CRSF task for no operational benefit.

    // HardwareSerial installs its UART drivers from setup() on Core 1. Keep
    // the CRSF owner on that core too, above control sampling and far above
    // the Arduino/LVGL loop, so display/system work cannot create radio gaps.
    xTaskCreatePinnedToCore(crsfTask, "crsf-link", 4096, nullptr, 6,
                            &crsfTaskHandle, 1);

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
    channel2Ui.setStartupPreflight(false, 0);
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
    throttleInputRuntime.setProfile(channel2Ui.throttleInputProfile());
    throttleInputRuntime.reset(analogRead(THROTTLE_ADC_PIN));
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
        if (!startupPreflightComplete) {
            if (startupInputSettledAtMs == 0)
                startupInputSettledAtMs = nowMs;
            if (nowMs - startupInputSettledAtMs >= STARTUP_INPUT_SETTLE_MS) {
                const uint32_t mask = physicalInputs.startupUnsafeMask();
                if (mask != startupWarningMask) {
                    startupWarningMask = mask;
                    channel2Ui.setStartupPreflight(false, mask);
                    Serial.printf("STARTUP CHECK: unsafe mask=0x%08lX\n",
                                  static_cast<unsigned long>(mask));
                }
                if (mask == 0 && inputExpanderReady) {
                    startupPreflightComplete = true;
                    channel2Ui.setStartupPreflight(true, 0);
                    requestControlReset();
                    Serial.println("STARTUP CHECK: controls safe; arm available");
                }
            }
        }
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

    if (ENABLE_PERIODIC_USB_DIAGNOSTICS &&
        static_cast<int32_t>(nowMs - nextLogMs) >= 0) {
        nextLogMs += USB_LOG_PERIOD_MS;
        const int neutralDelta = static_cast<int>(control.adc) -
                                 static_cast<int>(channel2Ui.throttleCentreAdc());
        Serial.printf("raw=%4u adc=%4u ctr=%4u delta=%+4d safety=%s armReq=%s cal=%s neutralFor=%lums THR=CH%u requested=%4u sent=%4u armed=%s stale=%s ctrlGap=%luus txFrames=%lu crsfGap=%luus txWait=%luus txTO=%lu drive=%s TIMER=%s MCP=%s link=%s LQ=%u RSSI=%d TX=%s%u.%uV RX=%s%u.%uV bytes=%lu frames=%lu crc=%lu\n",
                      control.rawAdc,
                      control.adc,
                      channel2Ui.throttleCentreAdc(),
                      neutralDelta,
                      safetyStatusName(control.safetyStatus),
                      channel2Ui.armRequested() ? "yes" : "no",
                      control.calibrating ? "yes" : "no",
                      static_cast<unsigned long>(neutralSinceMs == 0 ? 0 : nowMs - neutralSinceMs),
                      channel2Ui.throttleChannel(),
                      control.requestedCrsf,
                      control.transmittedCrsf,
                      throttleArmed ? "yes" : "NO",
                      sharedControlStale ? "YES" : "no",
                      static_cast<unsigned long>(control.maximumTaskGapUs),
                      static_cast<unsigned long>(crsfChannelFramesSent),
                      static_cast<unsigned long>(crsfMaximumTaskGapUs),
                      static_cast<unsigned long>(crsfMaximumTransmitWaitUs),
                      static_cast<unsigned long>(crsfTransmitTimeouts),
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
                      static_cast<unsigned long>(telemetry.rawBytes),
                      static_cast<unsigned long>(telemetry.validFrames),
                      static_cast<unsigned long>(telemetry.crcErrors));
    }
}
