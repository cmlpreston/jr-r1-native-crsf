#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/uart.h>
#include <hal/uart_ll.h>
#include <soc/gpio_pins.h>
#include <soc/gpio_sig_map.h>
#include <esp_system.h>

#include "transmitter_ui.h"
#include "binding_resolver.h"
#include "boot_diagnostics.h"
#include "config.h"
#include "control_configuration.h"
#include "crsf_channel_output.h"
#include "crsf_management.h"
#include "crsf_telemetry.h"
#include "mcp23017_input.h"
#ifdef JR_R1_XIAO_PCB
#include "pcb_io.h"
#endif
#include "output_safety.h"
#include "physical_input_runtime.h"
#include "proportional_input.h"
#include "r1_input_map.h"

namespace {

// Native USB CDC owns the debug console, leaving UART0 available for the
// latency-sensitive single-wire CRSF handset connection. ExpressLRS uses the
// same UART on ESP32 targets for its proven half-duplex implementation.
HardwareSerial rangerSerial(0);

constexpr uart_port_t RANGER_UART = UART_NUM_0;
constexpr uint32_t CONTROL_TASK_PERIOD_MS = 4;
constexpr uint32_t STARTUP_INPUT_SETTLE_MS = 400;

struct ControlStateSnapshot {
    uint16_t rawAdc = THROTTLE_ADC_CENTER;
    uint16_t adc = THROTTLE_ADC_CENTER;
    int16_t rawPermille = 0;
    int16_t outputPermille = 0;
    uint16_t requestedCrsf = CRSF_CHANNEL_MID;
    uint16_t transmittedCrsf = CRSF_CHANNEL_MID;
    uint16_t steeringAdc = STEERING_ADC_CENTER;
    int16_t steeringRawPermille = 0;
    int16_t steeringOutputPermille = 0;
    uint16_t steeringTransmittedCrsf = CRSF_CHANNEL_MID;
    OutputSafetyStatus safetyStatus = OutputSafetyStatus::Cut;
    bool calibrating = false;
    bool inputValid = true;
    bool armed = false;
    uint32_t neutralSinceMs = 0;
    uint32_t maximumTaskGapUs = 0;
};

struct ControlCommandSnapshot {
    uint32_t resetSequence = 0;
    bool startupPreflightComplete = false;
};

struct CrsfRuntimeSnapshot {
    uint32_t channelFramesSent = 0;
    uint32_t transmitTimeouts = 0;
    uint32_t maximumTransmitWaitUs = 0;
    uint32_t maximumTaskGapUs = 0;
    bool controlStale = true;
    OutputSafetyStatus outputSafetyStatus = OutputSafetyStatus::ControlStale;
};

struct SharedChannelOutputState {
    CrsfChannelOutputModel model;
    OutputSafetyStatus controlSafetyStatus = OutputSafetyStatus::Cut;
    uint32_t updatedUs = 0;
};

ProportionalInputRuntime throttleInputRuntime;
ProportionalInputRuntime steeringInputRuntime;
ChannelSlewRuntime channelSlewRuntime;
uint32_t filteredTxBatteryAdcMv = 0;
uint32_t nextTxBatterySampleMs = 0;
// Owned exclusively by the control task. Other tasks observe these values only
// through ControlStateSnapshot.
bool controlThrottleArmed = false;
bool controlWasCalibrating = false;
uint32_t controlNeutralSinceMs = 0;
uint32_t controlLinkReadySinceMs = 0;
uint32_t nextFrameUs = 0;
uint32_t nextLogMs = 0;
TransmitterUi transmitterUi;
CrsfTelemetryParser telemetryParser;
BindingResolver bindingResolver;
ResolvedBindingActionBuffer bindingActions;
#ifndef JR_R1_XIAO_PCB
Mcp23017Input inputExpander(MCP23017_I2C_ADDRESS);
#endif
PhysicalInputRuntime physicalInputs;
PhysicalInputEventBuffer physicalInputEvents;
portMUX_TYPE bindingResolverMux = portMUX_INITIALIZER_UNLOCKED;
uint32_t nextExpanderPollMs = 0;
uint32_t nextExpanderProbeMs = 0;
uint16_t expanderPorts = 0xffff;
bool inputExpanderReady = false;
CrsfRuntimeSnapshot crsfRuntimeOwned;
CrsfRuntimeSnapshot sharedCrsfRuntime;
portMUX_TYPE crsfRuntimeMux = portMUX_INITIALIZER_UNLOCKED;
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
ControlConfigurationSnapshot sharedControlConfiguration;
ControlConfigurationSnapshot lastPublishedControlConfiguration;
portMUX_TYPE controlConfigurationMux = portMUX_INITIALIZER_UNLOCKED;
uint32_t nextControlConfigurationRevision = 1;
bool controlConfigurationPublished = false;
ControlCommandSnapshot sharedControlCommands;
portMUX_TYPE controlCommandMux = portMUX_INITIALIZER_UNLOCKED;
bool startupPreflightComplete = false; // Arduino-loop-owned startup state.
uint32_t startupInputSettledAtMs = 0;
uint32_t startupWarningMask = 0;
char usbCommand[32];
uint8_t usbCommandLength = 0;
#ifdef JR_R1_XIAO_PCB
McpInputSnapshot latestPcbInputs[] = {{0x20, 0xffff, false}, {0x21, 0xffff, false}};
uint32_t nextInputDiagnosticMs = 0;
void printPcbInputs()
{
    Serial.println("INPUTS: cached GPIO (bit 0=PA0, bit 8=PB0); LOW masks exclude output/unused/touch pins");
    for (unsigned i = 0; i < 2; ++i) {
        const auto &sample = latestPcbInputs[i];
        const uint16_t mask = i == 0 ? 0x7f1f : 0x661f;
        const uint32_t errors = i == 0 ? pcbKeypad.readErrors() : pcbControls.readErrors();
        if (sample.available)
            Serial.printf("INPUTS K%u 0x%02X GPIO=0x%04X controls_LOW=0x%04X errors=%lu\n",
                          i+1, sample.address, sample.levels,
                          static_cast<unsigned>((~sample.levels) & mask),
                          static_cast<unsigned long>(errors));
        else
            Serial.printf("INPUTS K%u 0x%02X UNAVAILABLE errors=%lu\n", i+1, sample.address,
                          static_cast<unsigned long>(errors));
    }
}
#endif

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
    const size_t count = transmitterUi.copyBindings(bindings, kPhysicalControlCount);
    portENTER_CRITICAL(&bindingResolverMux);
    const BindingLoadResult result = bindingResolver.setBindings(bindings, count);
    portEXIT_CRITICAL(&bindingResolverMux);
    return result;
}

void updateNeutralInterlock(uint16_t adc,
                            const ControlConfigurationSnapshot &configuration)
{
    if (controlThrottleArmed) {
        return;
    }

    const int delta = static_cast<int>(adc) -
                      static_cast<int>(configuration.throttleInput.calibration.centre);
    if (abs(delta) <= ARM_NEUTRAL_ADC_WINDOW) {
        if (controlNeutralSinceMs == 0) {
            controlNeutralSinceMs = millis();
        } else if (millis() - controlNeutralSinceMs >= ARM_NEUTRAL_TIME_MS) {
            controlThrottleArmed = true;
            Serial.println("ARMED: throttle input is now being transmitted");
        }
    } else {
        controlNeutralSinceMs = 0;
    }
}

bool updateLinkInterlock(const CrsfTelemetryState &telemetry)
{
    // Once armed, link telemetry is advisory only. Dropping the transmitted
    // command merely because a return packet was delayed would itself create
    // a hazardous control transient; RF-loss behaviour belongs in the RX
    // failsafe configuration.
    if (controlThrottleArmed) return true;

    const uint32_t now = millis();
    const bool acceptable = telemetry.linkValid &&
                            telemetry.uplinkLinkQuality >= ARM_MIN_LINK_QUALITY;
    if (!acceptable) {
        controlLinkReadySinceMs = 0;
        return false;
    }
    if (controlLinkReadySinceMs == 0) controlLinkReadySinceMs = now;
    return now - controlLinkReadySinceMs >= ARM_LINK_STABLE_TIME_MS;
}

OutputSafetyStatus updateTriggerSafety(uint16_t adc, bool calibrationActive,
                                      bool inputValid,
                                      const CrsfTelemetryState &telemetry,
                                      const ControlConfigurationSnapshot &configuration,
                                      bool startupComplete)
{
    // Calibration is handled by ProportionalInput. This is only the output
    // safety policy: never arm during calibration and require neutral again
    // after calibration finishes.
    if (controlWasCalibrating && !calibrationActive) {
        controlThrottleArmed = false;
        controlNeutralSinceMs = 0;
        controlLinkReadySinceMs = 0;
    }
    controlWasCalibrating = calibrationActive;
    if (calibrationActive) {
        controlThrottleArmed = false;
        controlNeutralSinceMs = 0;
        controlLinkReadySinceMs = 0;
        return OutputSafetyStatus::Calibration;
    }
    if (!inputValid) {
        controlThrottleArmed = false;
        controlNeutralSinceMs = 0;
        controlLinkReadySinceMs = 0;
        return OutputSafetyStatus::InputFault;
    }
    if (!configuration.throttleEnabled) {
        controlThrottleArmed = false;
        controlNeutralSinceMs = 0;
        controlLinkReadySinceMs = 0;
        return OutputSafetyStatus::ChannelDisabled;
    }
    if (!startupComplete) {
        controlThrottleArmed = false;
        controlNeutralSinceMs = 0;
        controlLinkReadySinceMs = 0;
        return OutputSafetyStatus::StartupWarning;
    }
    if (!configuration.armRequested) {
        controlThrottleArmed = false;
        controlNeutralSinceMs = 0;
        controlLinkReadySinceMs = 0;
        return OutputSafetyStatus::Cut;
    }
    if (!updateLinkInterlock(telemetry)) {
        // Neutral qualification starts only after the returned RF link has
        // remained acceptable for its complete dwell period.
        controlNeutralSinceMs = 0;
        return OutputSafetyStatus::LinkRequired;
    }
    updateNeutralInterlock(adc, configuration);
    return controlThrottleArmed ? OutputSafetyStatus::Armed
                         : OutputSafetyStatus::NeutralRequired;
}

void setCrsfBusReceive()
{
    // One UART owns both directions of the Ranger Nano's single-wire CRSF
    // connection. As in ExpressLRS, leave the UART TX matrix mapping in place
    // and release the physical driver by changing the shared GPIO to input.
    // The UART driver and its RX ring buffer remain installed continuously.
    gpio_set_direction(CRSF_SIGNAL_PIN, GPIO_MODE_INPUT);
    // Match ExpressLRS CRSFHandset::duplex_set_RX() for an inverted
    // single-wire handset bus: matrix inversion plus a physical pull-down.
    gpio_matrix_in(CRSF_SIGNAL_PIN, U0RXD_IN_IDX, true);
    gpio_pulldown_en(CRSF_SIGNAL_PIN);
    gpio_pullup_dis(CRSF_SIGNAL_PIN);
}

void setCrsfBusTransmit()
{
    // Stop UART0 receiving before its transmitter owns the half-duplex wire.
    // Match ExpressLRS CRSFHandset::duplex_set_TX(): disconnect RX by routing
    // a constant physical low without matrix inversion while TX owns the bus.
    gpio_matrix_in(GPIO_MATRIX_CONST_ZERO_INPUT, U0RXD_IN_IDX, false);
    gpio_set_pull_mode(CRSF_SIGNAL_PIN, GPIO_FLOATING);
    gpio_set_level(CRSF_SIGNAL_PIN, 0);
    gpio_set_direction(CRSF_SIGNAL_PIN, GPIO_MODE_OUTPUT);
    gpio_matrix_out(CRSF_SIGNAL_PIN, U0TXD_OUT_IDX, true, false);
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
    if (waitedUs > crsfRuntimeOwned.maximumTransmitWaitUs)
        crsfRuntimeOwned.maximumTransmitWaitUs = waitedUs;
    if (!uart_ll_is_tx_idle(UART_LL_GET_HW(RANGER_UART)))
        ++crsfRuntimeOwned.transmitTimeouts;
    setCrsfBusReceive();
}

void sendChannelsFrame(const CrsfChannelOutputModel &model, bool live)
{
    uint8_t frame[CRSF_FRAME_BYTES];
    buildCrsfChannelsFrame(model, live, frame);
    transmitCrsfFrame(frame, sizeof(frame));
    ++crsfRuntimeOwned.channelFramesSent;
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
    if (request.action == ElrsManagementAction::TxParameter) return;
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

void publishChannelOutputs(const CrsfChannelOutputModel &model,
                           OutputSafetyStatus controlSafetyStatus,
                           uint32_t updatedUs)
{
    portENTER_CRITICAL(&channelOutputMux);
    sharedChannelOutputs.model = model;
    sharedChannelOutputs.controlSafetyStatus = controlSafetyStatus;
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
    sharedChannelOutputs.controlSafetyStatus = OutputSafetyStatus::Cut;
    sharedChannelOutputs.updatedUs = micros();
    portEXIT_CRITICAL(&channelOutputMux);
}

void requestControlReset()
{
    portENTER_CRITICAL(&controlCommandMux);
    ++sharedControlCommands.resetSequence;
    portEXIT_CRITICAL(&controlCommandMux);
    forceChannelOutputsSafe();
}

void completeStartupPreflight()
{
    portENTER_CRITICAL(&controlCommandMux);
    sharedControlCommands.startupPreflightComplete = true;
    ++sharedControlCommands.resetSequence;
    portEXIT_CRITICAL(&controlCommandMux);
    forceChannelOutputsSafe();
}

ControlCommandSnapshot readControlCommands()
{
    ControlCommandSnapshot snapshot;
    portENTER_CRITICAL(&controlCommandMux);
    snapshot = sharedControlCommands;
    portEXIT_CRITICAL(&controlCommandMux);
    return snapshot;
}

void publishCrsfRuntime()
{
    portENTER_CRITICAL(&crsfRuntimeMux);
    sharedCrsfRuntime = crsfRuntimeOwned;
    portEXIT_CRITICAL(&crsfRuntimeMux);
}

CrsfRuntimeSnapshot readCrsfRuntime()
{
    CrsfRuntimeSnapshot snapshot;
    portENTER_CRITICAL(&crsfRuntimeMux);
    snapshot = sharedCrsfRuntime;
    portEXIT_CRITICAL(&crsfRuntimeMux);
    return snapshot;
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

void publishControlConfiguration(ControlConfigurationSnapshot configuration)
{
    configuration.revision = nextControlConfigurationRevision++;
    portENTER_CRITICAL(&controlConfigurationMux);
    sharedControlConfiguration = configuration;
    portEXIT_CRITICAL(&controlConfigurationMux);
    lastPublishedControlConfiguration = configuration;
    controlConfigurationPublished = true;
}

void publishUiControlConfigurationIfChanged()
{
    const ControlConfigurationSnapshot candidate =
        transmitterUi.controlConfigurationSnapshot();
    if (!controlConfigurationPublished ||
        !equivalentControlConfiguration(candidate,
                                        lastPublishedControlConfiguration))
        publishControlConfiguration(candidate);
}

ControlConfigurationSnapshot readControlConfiguration()
{
    ControlConfigurationSnapshot snapshot;
    portENTER_CRITICAL(&controlConfigurationMux);
    snapshot = sharedControlConfiguration;
    portEXIT_CRITICAL(&controlConfigurationMux);
    return snapshot;
}

void controlTask(void *)
{
    TickType_t nextWake = xTaskGetTickCount();
    uint32_t previousCycleUs = micros();
    uint32_t maximumTaskGapUs = 0;
    uint32_t appliedConfigurationRevision = 0;
    uint32_t appliedResetSequence = 0;
    OutputSafetyStatus previousSafetyStatus = OutputSafetyStatus::ControlStale;
    for (;;) {
        const uint32_t cycleUs = micros();
        const uint32_t taskGapUs = cycleUs - previousCycleUs;
        previousCycleUs = cycleUs;
        if (taskGapUs > maximumTaskGapUs) maximumTaskGapUs = taskGapUs;
        const ControlCommandSnapshot commands = readControlCommands();
        if (commands.resetSequence != appliedResetSequence) {
            appliedResetSequence = commands.resetSequence;
            controlThrottleArmed = false;
            controlNeutralSinceMs = 0;
            controlLinkReadySinceMs = 0;
            channelSlewRuntime.reset(millis());
        }

        // Capture one coherent configuration for the entire cycle. UI/model
        // edits published during this cycle become visible at the next one.
        const ControlConfigurationSnapshot configuration =
            readControlConfiguration();
        if (configuration.revision != appliedConfigurationRevision) {
            throttleInputRuntime.setProfile(configuration.throttleInput);
            steeringInputRuntime.setProfile(configuration.steeringInput);
            appliedConfigurationRevision = configuration.revision;
        }
        const ProportionalInputState throttleInput =
            throttleInputRuntime.update(analogRead(THROTTLE_ADC_PIN));
        const uint16_t filteredAdc = throttleInput.filteredAdc;
        const ProportionalInputState steeringInput =
            steeringInputRuntime.update(analogRead(STEERING_ADC_PIN));
        const bool calibrating = configuration.calibrationActive;
        const bool inputValid = throttleInput.valid;
        const CrsfTelemetryState telemetry = readTelemetrySnapshot();
        const OutputSafetyStatus safetyStatus = updateTriggerSafety(
            filteredAdc, calibrating, inputValid, telemetry, configuration,
            commands.startupPreflightComplete);
        if (safetyStatus != previousSafetyStatus) {
            Serial.printf("SAFETY t=%lums %s -> %s arm_request=%s raw=%u filtered=%u centre=%u delta=%+d\n",
                          static_cast<unsigned long>(millis()),
                          safetyStatusName(previousSafetyStatus),
                          safetyStatusName(safetyStatus),
                          configuration.armRequested ? "yes" : "no",
                          throttleInput.rawAdc, filteredAdc,
                          configuration.throttleInput.calibration.centre,
                          static_cast<int>(filteredAdc) -
                              static_cast<int>(
                                  configuration.throttleInput.calibration.centre));
            previousSafetyStatus = safetyStatus;
        }

        const uint32_t processingNowMs = millis();
        const bool outputsLive = safetyStatus == OutputSafetyStatus::Armed;
        if (!outputsLive) channelSlewRuntime.reset(processingNowMs);

        const int16_t rawThrottle = throttleInput.normalisedPermille;
        const ProcessedProportionalChannel throttleChannel =
            processConfiguredProportionalChannel(
                configuration, configuration.throttleChannel, rawThrottle, 0,
                channelSlewRuntime, processingNowMs, outputsLive);
        const uint16_t requestedThrottle = crsfValueFromPermille(
            throttleChannel.requestedPermille);
        const uint16_t safeNeutralThrottle = crsfValueFromPermille(
            throttleChannel.safePermille);
        const uint16_t transmittedThrottle = outputsLive
            ? requestedThrottle : safeNeutralThrottle;
        const int16_t rawSteering = steeringInput.normalisedPermille;
        const ProcessedProportionalChannel steeringChannel =
            processConfiguredProportionalChannel(
                configuration, configuration.steeringChannel, rawSteering, 0,
                channelSlewRuntime, processingNowMs, outputsLive);
        const uint16_t requestedSteering = crsfValueFromPermille(
            steeringChannel.requestedPermille);
        const uint16_t safeNeutralSteering = crsfValueFromPermille(
            steeringChannel.safePermille);
        const uint16_t transmittedSteering = outputsLive
            ? requestedSteering : safeNeutralSteering;
        CrsfChannelOutputModel outputs;
        bindingActions.clear();
        portENTER_CRITICAL(&bindingResolverMux);
        bindingResolver.update(millis(), bindingActions);
        const uint16_t roleOwnedChannels =
            configuredRoleOwnedChannels(configuration);
        bindingResolver.applyChannelOutputs(outputs, roleOwnedChannels);
        portEXIT_CRITICAL(&bindingResolverMux);
        // Apply the same response/adjustment/slew pipeline to proportional AUX
        // bindings. Role-owned channels use that processor above with their
        // dedicated ADC inputs and are installed below.
        processConfiguredProportionalOutputs(
            configuration, outputs, channelSlewRuntime, processingNowMs,
            outputsLive, roleOwnedChannels);
        outputs.configure(configuration.steeringChannel,
            requestedSteering, safeNeutralSteering,
            configuration.steeringEnabled);
        outputs.configure(configuration.throttleChannel, requestedThrottle,
                          safeNeutralThrottle, configuration.throttleEnabled);
        const uint32_t controlUpdateUs = micros();
        publishChannelOutputs(outputs, safetyStatus, controlUpdateUs);

        ControlStateSnapshot snapshot;
        snapshot.rawAdc = throttleInput.rawAdc;
        snapshot.adc = filteredAdc;
        snapshot.rawPermille = rawThrottle;
        snapshot.outputPermille = outputsLive
            ? throttleChannel.requestedPermille
            : throttleChannel.safePermille;
        snapshot.requestedCrsf = requestedThrottle;
        snapshot.transmittedCrsf = transmittedThrottle;
        snapshot.steeringAdc = steeringInput.filteredAdc;
        snapshot.steeringRawPermille = rawSteering;
        snapshot.steeringOutputPermille = steeringChannel.requestedPermille;
        snapshot.steeringTransmittedCrsf = transmittedSteering;
        snapshot.safetyStatus = safetyStatus;
        snapshot.calibrating = calibrating;
        snapshot.inputValid = inputValid;
        snapshot.armed = controlThrottleArmed;
        snapshot.neutralSinceMs = controlNeutralSinceMs;
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
        if (taskGapUs > crsfRuntimeOwned.maximumTaskGapUs)
            crsfRuntimeOwned.maximumTaskGapUs = taskGapUs;
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
            crsfRuntimeOwned.controlStale = stale;
            const OutputSafetyDecision safetyDecision = arbitrateOutputSafety(
                outputs.controlSafetyStatus, !stale);
            crsfRuntimeOwned.outputSafetyStatus = safetyDecision.status;
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
                sendChannelsFrame(outputs.model,
                                  safetyDecision.transmitRequested);
        }
        readReturnedTelemetry();
        const uint32_t nowMs = millis();
        if (static_cast<int32_t>(nowMs - nextSnapshotMs) >= 0) {
            nextSnapshotMs = nowMs + 20;
            publishTelemetrySnapshot();
            publishCrsfRuntime();
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
            if (strcmp(usbCommand, "BOOTLOG") == 0 || strcmp(usbCommand, "bootlog") == 0)
                bootDiagnosticsDump();
#ifdef JR_R1_XIAO_PCB
            if (strcmp(usbCommand, "INPUTS") == 0 || strcmp(usbCommand, "inputs") == 0)
                printPcbInputs();
#endif
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
    // Establish the inverted CRSF bus idle level immediately, before USB/UI
    // startup work. The Ranger can remain powered across an S3 reset, so a
    // deliberate quiet interval gives its handset UART watchdog a clean
    // opportunity to resynchronise before our first channel frame.
    gpio_reset_pin(CRSF_SIGNAL_PIN);
    gpio_set_direction(CRSF_SIGNAL_PIN, GPIO_MODE_INPUT);
    gpio_pulldown_en(CRSF_SIGNAL_PIN);
    gpio_pullup_dis(CRSF_SIGNAL_PIN);

    Serial.begin(115200);
    delay(CRSF_STARTUP_BUS_QUIET_MS);
    Serial.println();
    Serial.println("JR R1 native CRSF throttle test");
    Serial.printf("BOOT: reset_reason=%u free_heap=%u bytes\n",
                  static_cast<unsigned>(esp_reset_reason()),
                  static_cast<unsigned>(ESP.getFreeHeap()));
    Serial.printf("MEMORY: heap=%u free=%u min_free=%u largest=%u "
                  "psram=%u free_psram=%u flash=%u\n",
                  static_cast<unsigned>(ESP.getHeapSize()),
                  static_cast<unsigned>(ESP.getFreeHeap()),
                  static_cast<unsigned>(ESP.getMinFreeHeap()),
                  static_cast<unsigned>(ESP.getMaxAllocHeap()),
                  static_cast<unsigned>(ESP.getPsramSize()),
                  static_cast<unsigned>(ESP.getFreePsram()),
                  static_cast<unsigned>(ESP.getFlashChipSize()));
    Serial.println("WARNING: disconnect the motor / raise the wheels for first test");

    bootDiagnosticsBegin(static_cast<uint32_t>(esp_reset_reason()));

    analogReadResolution(12);
    analogSetPinAttenuation(THROTTLE_ADC_PIN, ADC_11db);
    analogSetPinAttenuation(STEERING_ADC_PIN, ADC_11db);
    analogSetPinAttenuation(TX_BATTERY_ADC_PIN, ADC_11db);

    // UART0 remains installed with both RX and TX assigned to the one-wire
    // CRSF pin. The GPIO matrix functions above alternate ownership without
    // stopping/restarting the driver, matching the half-duplex strategy used
    // by ExpressLRS itself. Allocate the larger RX buffer before begin().
    rangerSerial.setRxBufferSize(2048);
    rangerSerial.begin(CRSF_BAUD, SERIAL_8N1,
                       CRSF_SIGNAL_PIN, CRSF_SIGNAL_PIN, false);
    rangerSerial.setTimeout(0);
    setCrsfBusReceive();
    uart_flush_input(RANGER_UART);
    Serial.printf("CRSF TRANSPORT: UART0 single-wire half-duplex on GPIO%u\n",
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
#ifdef JR_R1_XIAO_PCB
    if (!transmitterUi.begin()) {
        Serial.println("PCB STARTUP FAILED: check K1/K2, LCD and touch; reset to retry. Outputs remain safe.");
        // No control task or arming path starts after an incomplete hardware
        // sequence. The already-running CRSF task keeps sending safe frames.
        bootDiagnosticsStage("halted-safe");
        for (;;) {
            // Diagnostics only: never invoke screenshots, UI or control startup here.
            consumeScreenshotCommand();
            delay(20);
        }
    }
#else
    transmitterUi.begin();
#endif
    Serial.printf("MEMORY: UI init used=%u heap_free=%u min_free=%u "
                  "largest=%u free_psram=%u\n",
                  static_cast<unsigned>(heapBeforeUi - ESP.getFreeHeap()),
                  static_cast<unsigned>(ESP.getFreeHeap()),
                  static_cast<unsigned>(ESP.getMinFreeHeap()),
                  static_cast<unsigned>(ESP.getMaxAllocHeap()),
                  static_cast<unsigned>(ESP.getFreePsram()));
    const bool inputMapReady = physicalInputs.begin(kR1InputMap, kR1InputMapCount);
    transmitterUi.setStartupPreflight(false, 0);
    configureDirectGpioInputs();
    Serial.printf("INPUT MAP: %u controls: %s\n",
                  static_cast<unsigned>(kR1InputMapCount),
                  inputMapReady ? "ready" : "INVALID");
    const BindingLoadResult bindingResult = installUiBindings();
    Serial.printf("BINDINGS: installed %u assignments: %s\n",
                  static_cast<unsigned>(bindingResolver.bindingCount()),
                  bindingResult.ok() ? "ready" : bindingResult.reason);
#ifdef JR_R1_XIAO_PCB
    // Already initialised by UI startup; do not reset the shared K2 outputs.
    inputExpanderReady = pcbKeypad.online() && pcbControls.online();
#else
    inputExpanderReady = inputExpander.begin();
    Serial.printf("MCP23017: address=0x%02X %s; PA0 is Timer button\n",
                  MCP23017_I2C_ADDRESS,
                  inputExpanderReady ? "ONLINE" : "NOT FOUND");
#endif
    publishUiControlConfigurationIfChanged();
    const ControlConfigurationSnapshot initialConfiguration =
        readControlConfiguration();
    throttleInputRuntime.setProfile(initialConfiguration.throttleInput);
    throttleInputRuntime.reset(analogRead(THROTTLE_ADC_PIN));
    steeringInputRuntime.setProfile(initialConfiguration.steeringInput);
    steeringInputRuntime.reset(analogRead(STEERING_ADC_PIN));
    channelSlewRuntime.reset(millis());
    publishControlState(ControlStateSnapshot());
    // Last flash checkpoint before live control starts; no periodic flash writes.
    bootDiagnosticsStage("control-starting");
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
        transmitterUi.captureScreenshot(Serial);
        nextLogMs = millis() + USB_LOG_PERIOD_MS;
        return;
    }

    const ControlStateSnapshot control = readControlState();
    const CrsfTelemetryState telemetry = readTelemetrySnapshot();
    const CrsfRuntimeSnapshot crsfRuntime = readCrsfRuntime();

    uint16_t txVoltageDecivolts = 0;
    bool txVoltageValid = false;
    const uint32_t nowMs = millis();
#ifdef JR_R1_XIAO_PCB
    inputExpanderReady = pcbKeypad.online() && pcbControls.online();
    if (!inputExpanderReady &&
        static_cast<int32_t>(nowMs - nextExpanderProbeMs) >= 0) {
        nextExpanderProbeMs = nowMs + 1000;
        if (!pcbKeypad.online()) pcbKeypad.begin();
        if (!pcbControls.online()) pcbControls.begin();
        inputExpanderReady = pcbKeypad.online() && pcbControls.online();
    }
#else
    if (!inputExpanderReady &&
        static_cast<int32_t>(nowMs - nextExpanderProbeMs) >= 0) {
        nextExpanderProbeMs = nowMs + 1000;
        inputExpanderReady = inputExpander.begin();
        if (inputExpanderReady)
            Serial.printf("MCP23017: reconnected at 0x%02X\n",
                          MCP23017_I2C_ADDRESS);
    }
#endif
    if (static_cast<int32_t>(nowMs - nextExpanderPollMs) >= 0) {
        nextExpanderPollMs = nowMs + MCP23017_POLL_PERIOD_MS;
#ifdef JR_R1_XIAO_PCB
        McpInputSnapshot snapshots[] = {{0x20, 0xffff, false}, {0x21, 0xffff, false}};
        snapshots[0].available = pcbKeypad.online() && pcbKeypad.read(snapshots[0].levels);
        snapshots[1].available = pcbControls.online() && pcbControls.read(snapshots[1].levels);
        latestPcbInputs[0] = snapshots[0];
        latestPcbInputs[1] = snapshots[1];
        inputExpanderReady = snapshots[0].available && snapshots[1].available;
        if (!startupPreflightComplete && static_cast<int32_t>(nowMs - nextInputDiagnosticMs) >= 0) {
            printPcbInputs();
            nextInputDiagnosticMs = nowMs + 1000;
        }
        physicalInputEvents.clear();
        physicalInputs.update(snapshots, 2, readDirectGpioLevels(), nowMs,
                              physicalInputEvents);
#else
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
#endif
        physicalInputs.setProportionalValue(PhysicalControlId::ThrottleTrigger,
                                            control.rawPermille, nowMs);
        physicalInputs.setProportionalValue(PhysicalControlId::SteeringWheel,
                                            control.steeringRawPermille, nowMs);
        transmitterUi.setPhysicalInputStates(physicalInputs.states());
        if (!startupPreflightComplete) {
            if (startupInputSettledAtMs == 0)
                startupInputSettledAtMs = nowMs;
            if (nowMs - startupInputSettledAtMs >= STARTUP_INPUT_SETTLE_MS) {
                const uint32_t mask = physicalInputs.startupUnsafeMask();
                if (mask != startupWarningMask) {
                    startupWarningMask = mask;
                    Serial.printf("STARTUP CHECK: unsafe mask=0x%08lX\n",
                                  static_cast<unsigned long>(mask));
                }
                transmitterUi.setStartupPreflight(false, mask);
                if (mask == 0 && inputExpanderReady) {
                    startupPreflightComplete = true;
                    transmitterUi.setStartupPreflight(true, 0);
                    completeStartupPreflight();
                    Serial.println("STARTUP CHECK: controls safe; arm available");
                }
            }
        }
        if (physicalInputEvents.size() != 0) {
            ResolvedBindingActionBuffer actions;
            portENTER_CRITICAL(&bindingResolverMux);
            bindingResolver.processEvents(physicalInputEvents,
                static_cast<uint8_t>(ContextAlways | transmitterUi.activeControlContext() |
                    (control.armed ? ContextArmed : ContextCut)), actions);
            portEXIT_CRITICAL(&bindingResolverMux);
            for (size_t index = 0; index < actions.size(); ++index) {
                const ResolvedBindingAction &action = actions[index];
                if (action.target == AssignmentTarget::ModelParameter)
                    transmitterUi.applyChannelParameterAction(action.targetId,
                                                            action.value);
                else if (action.target == AssignmentTarget::TransmitterFunction)
                    transmitterUi.applyTransmitterFunctionAction(action.targetId,
                                                               action.value);
                else if (action.target == AssignmentTarget::DriveMode)
                    transmitterUi.applyDriveModeAction(action.targetId,
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
    transmitterUi.setPhysicalInputStates(physicalInputs.states());
    if (transmitterUi.txVoltageSenseEnabled()) {
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
    transmitterUi.update(control.adc, control.steeringAdc, control.rawPermille,
                      control.outputPermille,
                      control.transmittedCrsf,
                      crsfRuntime.outputSafetyStatus,
                      telemetry, txVoltageDecivolts, txVoltageValid,
                      channelOutputs.model,
                      crsfRuntime.outputSafetyStatus ==
                          OutputSafetyStatus::Armed);
    ElrsManagementRequest management;
    if (transmitterUi.consumeElrsManagementRequest(management)) {
        requestControlReset();
        if (!queueManagementRequest(management))
            Serial.println("ELRS MGMT: queue busy; request discarded");
    }
    if (transmitterUi.consumeBindingsChanged()) {
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
    if (transmitterUi.consumeChannelRolesChanged()) {
        // Disable all live outputs before the next atomic model snapshot moves
        // throttle to its newly selected channel.
        requestControlReset();
        Serial.printf("SAFE: throttle role changed to CH%u; release trigger to re-arm\n",
                      transmitterUi.throttleChannel());
    }
    if (transmitterUi.consumeSafetyResetRequested()) {
        requestControlReset();
        Serial.println("SAFE: mode/assignment/arm state changed; output reset to neutral");
    }

    // Publish only after all UI and binding actions for this Arduino-loop
    // iteration have completed. The control task sees either the entire old
    // model or the entire new one, never a mixture of fields.
    publishUiControlConfigurationIfChanged();
    const ControlConfigurationSnapshot uiConfiguration =
        lastPublishedControlConfiguration;

    if (ENABLE_PERIODIC_USB_DIAGNOSTICS &&
        static_cast<int32_t>(nowMs - nextLogMs) >= 0) {
        nextLogMs += USB_LOG_PERIOD_MS;
        const int neutralDelta = static_cast<int>(control.adc) -
            static_cast<int>(uiConfiguration.throttleInput.calibration.centre);
        Serial.printf("raw=%4u adc=%4u ctr=%4u delta=%+4d safety=%s armReq=%s cal=%s neutralFor=%lums THR=CH%u requested=%4u sent=%4u armed=%s stale=%s ctrlGap=%luus txFrames=%lu crsfGap=%luus txWait=%luus txTO=%lu drive=%s TIMER=%s MCP=%s link=%s LQ=%u RSSI=%d TX=%s%u.%uV RX=%s%u.%uV bytes=%lu frames=%lu crc=%lu\n",
                      control.rawAdc,
                      control.adc,
                      uiConfiguration.throttleInput.calibration.centre,
                      neutralDelta,
                      safetyStatusName(crsfRuntime.outputSafetyStatus),
                      uiConfiguration.armRequested ? "yes" : "no",
                      control.calibrating ? "yes" : "no",
                      static_cast<unsigned long>(control.neutralSinceMs == 0
                          ? 0 : nowMs - control.neutralSinceMs),
                      uiConfiguration.throttleChannel,
                      control.requestedCrsf,
                      control.transmittedCrsf,
                      control.armed ? "yes" : "NO",
                      crsfRuntime.controlStale ? "YES" : "no",
                      static_cast<unsigned long>(control.maximumTaskGapUs),
                      static_cast<unsigned long>(crsfRuntime.channelFramesSent),
                      static_cast<unsigned long>(crsfRuntime.maximumTaskGapUs),
                      static_cast<unsigned long>(crsfRuntime.maximumTransmitWaitUs),
                      static_cast<unsigned long>(crsfRuntime.transmitTimeouts),
                      uiConfiguration.driveMode == DriveMode::Walk ? "WALK" : "NORMAL",
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
