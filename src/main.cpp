#include "remote_control_policy.h"
#include "rf_bench_session.h"
#include <Arduino.h>
#include <atomic>
#include <Wire.h>
#include "input_bench_mode.h"
#include <driver/gpio.h>
#include <driver/uart.h>
#include <hal/uart_ll.h>
#include <soc/gpio_pins.h>
#include <soc/gpio_sig_map.h>
#include <esp_system.h>

#include "transmitter_ui.h"
#include "setup_portal.h"
#include "binding_resolver.h"
#include "boot_diagnostics.h"
#include "memory_diagnostics.h"
#include "config.h"
#include "control_configuration.h"
#include "control_engine.h"
#include "control_output_mailbox.h"
#include "bounded_event_queue.h"
#include "input_bus_guard.h"
#include "input_pipeline_policy.h"
#include "crsf_channel_output.h"
#include "crsf_management.h"
#include "esc_rf_policy.h"
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
// Keep debug console traffic on USB rather than the CRSF connection.
HardwareSerial rangerSerial(0);
// Defaults off before either task starts. No channel OR management frame may
// revive the Ranger's handset watchdog without a physical confirmation.
std::atomic<bool> moduleLinkEnabled{false};
std::atomic<uint32_t> inputBindingEpoch{0},inputSafetyEpoch{0},inputLastSampleMs{0};

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
    uint32_t sampledMs = 0;
    uint32_t maximumTaskGapUs = 0;
};

struct CrsfRuntimeSnapshot {
    uint32_t channelFramesSent = 0;
    uint32_t transmitTimeouts = 0;
    uint32_t maximumTransmitWaitUs = 0;
    uint32_t maximumTaskGapUs = 0;
    bool controlStale = true;
    OutputSafetyStatus outputSafetyStatus = OutputSafetyStatus::ControlStale;
};

ControlEngine controlEngine({ARM_NEUTRAL_ADC_WINDOW, ARM_NEUTRAL_TIME_MS,
                             ARM_LINK_STABLE_TIME_MS, ARM_MIN_LINK_QUALITY});
uint32_t filteredTxBatteryAdcMv = 0;
uint32_t nextTxBatterySampleMs = 0;
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
bool inputExpanderReady = false;
CrsfRuntimeSnapshot crsfRuntimeOwned;
CrsfRuntimeSnapshot sharedCrsfRuntime;
portMUX_TYPE crsfRuntimeMux = portMUX_INITIALIZER_UNLOCKED;
ControlOutputMailbox controlOutputMailbox;
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
bool startupPreflightComplete = false; // Arduino-loop-owned startup state.
uint32_t startupInputSettledAtMs = 0;
uint32_t startupWarningMask = 0;
char usbCommand[32];
uint8_t usbCommandLength = 0;
bool inputMonitorHalted = false;
RfBenchSession rfBenchSession;
portMUX_TYPE rfBenchMux = portMUX_INITIALIZER_UNLOCKED;
bool rfBenchEnabled()
{
    portENTER_CRITICAL(&rfBenchMux);
    const bool enabled = rfBenchSession.enabled();
    portEXIT_CRITICAL(&rfBenchMux);
    return enabled;
}
bool rfBenchArmRequested()
{
    portENTER_CRITICAL(&rfBenchMux);
    const bool armed = rfBenchSession.armRequested(millis());
    portEXIT_CRITICAL(&rfBenchMux);
    return armed;
}
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
        case OutputSafetyStatus::ConfigurationFault: return "CONFIG_FAULT";
    }
    return "UNKNOWN";
}

#include "main_diagnostics.inc"

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

extern std::atomic<uint32_t> inputBindingEpoch;

BindingLoadResult installUiBindings()
{
    ControlBinding bindings[kPhysicalControlCount];
    const size_t count = transmitterUi.copyBindings(bindings, kPhysicalControlCount);
    portENTER_CRITICAL(&bindingResolverMux);
    const BindingLoadResult result = bindingResolver.setBindings(bindings, count);
    inputBindingEpoch.fetch_add(1);
    portEXIT_CRITICAL(&bindingResolverMux);
    return result;
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

bool transmitCrsfFrame(const uint8_t *frame, size_t length)
{
    if(!moduleLinkEnabled.load())return false;
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
    return true;
}

void sendChannelsFrame(const CrsfChannelOutputModel &model, bool live)
{
    uint8_t frame[CRSF_FRAME_BYTES];
    buildCrsfChannelsFrame(model, live, frame);
    if(transmitCrsfFrame(frame, sizeof(frame)))++crsfRuntimeOwned.channelFramesSent;
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
    if(!transmitCrsfFrame(frame, length))return;
    if(request.action==ElrsManagementAction::EscProgrammer){
        recordDiagnostic(DiagnosticKind::EscTx,request.escReturnProbe,unsigned(request.esc.op),request.esc.sequence);return;
    }
    if (request.action == ElrsManagementAction::TxParameter) return;
    if(request.action==ElrsManagementAction::ErsSensor){
        recordDiagnostic(DiagnosticKind::ErsTx,request.ers.type,request.ers.id,request.ers.command,unsigned(length));return;
    }
    recordDiagnostic(DiagnosticKind::ManagementTx,unsigned(request.action),unsigned(request.target),request.phraseLength);
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
                           uint32_t updatedUs, uint32_t acknowledgedReset, uint32_t revision)
{
    portENTER_CRITICAL(&channelOutputMux);
    controlOutputMailbox.publish(model, controlSafetyStatus, updatedUs, acknowledgedReset, revision);
    portEXIT_CRITICAL(&channelOutputMux);
}

SharedChannelOutputState readChannelOutputs()
{
    SharedChannelOutputState snapshot;
    portENTER_CRITICAL(&channelOutputMux);
    snapshot = controlOutputMailbox.outputs();
    portEXIT_CRITICAL(&channelOutputMux);
    return snapshot;
}

void requestControlReset()
{
    portENTER_CRITICAL(&rfBenchMux);
    rfBenchSession.cut();
    portEXIT_CRITICAL(&rfBenchMux);
    portENTER_CRITICAL(&channelOutputMux);
    controlOutputMailbox.requestReset();
    portEXIT_CRITICAL(&channelOutputMux);
}

void completeStartupPreflight()
{
    portENTER_CRITICAL(&channelOutputMux);
    controlOutputMailbox.completeStartup();
    portEXIT_CRITICAL(&channelOutputMux);
}

void requestUiConfigurationCut()
{
    portENTER_CRITICAL(&rfBenchMux);
    rfBenchSession.cut();
    portEXIT_CRITICAL(&rfBenchMux);
    portENTER_CRITICAL(&channelOutputMux);
    inputSafetyEpoch.fetch_add(1);
    controlOutputMailbox.requireConfiguration();
    portEXIT_CRITICAL(&channelOutputMux);
}

void requestPhysicalControlCut()
{
    inputSafetyEpoch.fetch_add(1);
    portENTER_CRITICAL(&rfBenchMux);rfBenchSession.cut();portEXIT_CRITICAL(&rfBenchMux);
    portENTER_CRITICAL(&channelOutputMux);controlOutputMailbox.requestPhysicalCut();portEXIT_CRITICAL(&channelOutputMux);
}

ControlCommandSnapshot readControlCommands()
{
    portENTER_CRITICAL(&channelOutputMux);
    const auto snapshot = controlOutputMailbox.commands();
    portEXIT_CRITICAL(&channelOutputMux);
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

void publishControlConfiguration(ControlConfigurationSnapshot configuration,uint32_t observedPhysicalCut)
{
    configuration.revision = nextControlConfigurationRevision++;
    portENTER_CRITICAL(&controlConfigurationMux);
    sharedControlConfiguration = configuration;
    portEXIT_CRITICAL(&controlConfigurationMux);
    portENTER_CRITICAL(&channelOutputMux);
    const bool accepted=controlOutputMailbox.acceptConfiguration(configuration.revision,
        configuration.armRequested,observedPhysicalCut);
    portEXIT_CRITICAL(&channelOutputMux);
    if(!accepted)return;
    lastPublishedControlConfiguration = configuration;
    controlConfigurationPublished = true;
}

void publishUiControlConfigurationIfChanged()
{
    const uint32_t physicalCut=readControlCommands().physicalCutSequence;
    const ControlConfigurationSnapshot candidate =
        transmitterUi.controlConfigurationSnapshot();
    if (!controlConfigurationPublished || readControlCommands().configurationPending ||
        !equivalentControlConfiguration(candidate,
                                        lastPublishedControlConfiguration))
        publishControlConfiguration(candidate,physicalCut);
}

ControlConfigurationSnapshot readControlConfiguration()
{
    ControlConfigurationSnapshot snapshot;
    portENTER_CRITICAL(&controlConfigurationMux);
    snapshot = sharedControlConfiguration;
    portEXIT_CRITICAL(&controlConfigurationMux);
    return snapshot;
}

// Loop-owned debounced digital inputs cross into the control task as one snapshot.
portMUX_TYPE mixInputMux = portMUX_INITIALIZER_UNLOCKED;
MixInputSnapshot sharedMixInputs;
uint32_t sharedMixInputsMs = 0;
void publishMixInputs()
{
    MixInputSnapshot next;
    for(uint8_t id=2;id<kPhysicalControlCount;++id) {
        const auto &state=physicalInputs.state(static_cast<PhysicalControlId>(id));
        if(state.mapped&&!state.faulted)next.valid|=uint32_t{1}<<id;
        const auto kind=physicalControlDescriptor(id).kind;
        next.values[id]=kind==PhysicalControlKind::SpringCentredTrim?
            (state.stableSecondary?1000:state.stablePrimary?-1000:0):(state.stablePrimary?1000:-1000);
    }
    portENTER_CRITICAL(&mixInputMux);
    sharedMixInputs=next;sharedMixInputsMs=millis();
    portEXIT_CRITICAL(&mixInputMux);
}
MixInputSnapshot readMixInputs(uint32_t now)
{
    portENTER_CRITICAL(&mixInputMux);
    MixInputSnapshot result=sharedMixInputs;
    const uint32_t updated=sharedMixInputsMs;
    portEXIT_CRITICAL(&mixInputMux);
    if(!updated||now-updated>500)result.valid=0;
    return result;
}

#include "main_input_pipeline.inc"

void controlTask(void *)
{
    TickType_t nextWake = xTaskGetTickCount();
    uint32_t previousCycleUs = micros();
    uint32_t maximumTaskGapUs = 0;
    OutputSafetyStatus previousSafetyStatus = OutputSafetyStatus::ControlStale;
    bool samplerFaultLatched=false;
    for (;;) {
        const uint32_t cycleUs = micros();
        const uint32_t taskGapUs = cycleUs - previousCycleUs;
        previousCycleUs = cycleUs;
        if (taskGapUs > maximumTaskGapUs) maximumTaskGapUs = taskGapUs;
        const ControlCommandSnapshot commands = readControlCommands();
        auto configuration = readControlConfiguration();
        if (rfBenchEnabled()) configuration.armRequested = rfBenchArmRequested();
        ControlCycleInput input;
        input.nowMs = millis();
        input.physicalInputsHealthy=physicalInputSampleFresh(input.nowMs,inputLastSampleMs.load());
        // An enable attempt made while the sampler is unhealthy must also be
        // cancelled, not retained as an automatic arm on later recovery.
        if(commands.startupPreflightComplete&&!input.physicalInputsHealthy&&(!samplerFaultLatched||configuration.armRequested)){
            immediatePhysicalCut();samplerFaultLatched=true;
        }
        if(input.physicalInputsHealthy&&!configuration.armRequested)samplerFaultLatched=false;
        input.resetSequence = commands.resetSequence;
        input.startupComplete = commands.startupPreflightComplete;
        input.throttleAdc = analogRead(THROTTLE_ADC_PIN);
        input.steeringAdc = analogRead(STEERING_ADC_PIN);
        input.digitalInputs = readMixInputs(input.nowMs);
        bindingActions.clear();
        portENTER_CRITICAL(&bindingResolverMux);
        bindingResolver.update(input.nowMs, bindingActions);
        bindingResolver.applyChannelOutputs(input.bindings, configuredRoleOwnedChannels(configuration));
        portEXIT_CRITICAL(&bindingResolverMux);
        const auto telemetry = readTelemetrySnapshot();
        input.linkValid = telemetry.linkValid;
        input.linkQuality = telemetry.uplinkLinkQuality;
        const auto result = controlEngine.update(configuration, input);
        const auto &throttleInput = result.throttle;
        const auto &steeringInput = result.steering;
        const auto &outputs = result.outputs;
        const uint16_t filteredAdc = throttleInput.filteredAdc;
        const bool inputValid = result.inputsValid;
        const bool calibrating = configuration.calibrationActive;
        const auto safetyStatus = result.safety;
        if (rfBenchEnabled() && (!inputValid || calibrating || !configuration.throttleEnabled)) {
            portENTER_CRITICAL(&rfBenchMux);
            rfBenchSession.cut();
            portEXIT_CRITICAL(&rfBenchMux);
        }


        const bool outputsLive = safetyStatus == OutputSafetyStatus::Armed;
        const int16_t rawThrottle = throttleInput.normalisedPermille;
        const int16_t rawSteering = steeringInput.normalisedPermille;
        const uint16_t requestedThrottle=outputs.requested(configuration.throttleChannel);
        const uint16_t safeNeutralThrottle=outputs.safe(configuration.throttleChannel);
        const uint16_t transmittedThrottle=outputsLive?requestedThrottle:safeNeutralThrottle;
        const uint16_t requestedSteering=outputs.requested(configuration.steeringChannel);
        const uint16_t safeNeutralSteering=outputs.safe(configuration.steeringChannel);
        const uint16_t transmittedSteering=outputsLive&&configuration.steeringEnabled?requestedSteering:safeNeutralSteering;
        const uint32_t controlUpdateUs = micros();
        publishChannelOutputs(outputs, safetyStatus, controlUpdateUs, input.resetSequence, configuration.revision);
        // Publish the new safe model before even bounded diagnostic bookkeeping.
        if(safetyStatus!=previousSafetyStatus){
            recordDiagnostic(DiagnosticKind::Safety,unsigned(previousSafetyStatus),unsigned(safetyStatus),
                configuration.armRequested,throttleInput.rawAdc,filteredAdc,
                configuration.throttleInput.calibration.centre);
            previousSafetyStatus=safetyStatus;
        }

        ControlStateSnapshot snapshot;
        snapshot.rawAdc = throttleInput.rawAdc;
        snapshot.adc = filteredAdc;
        snapshot.rawPermille = rawThrottle;
        snapshot.outputPermille = permilleFromCrsfValue(transmittedThrottle);
        snapshot.requestedCrsf = requestedThrottle;
        snapshot.transmittedCrsf = transmittedThrottle;
        snapshot.steeringAdc = steeringInput.filteredAdc;
        snapshot.steeringRawPermille = rawSteering;
        snapshot.steeringOutputPermille = permilleFromCrsfValue(transmittedSteering);
        snapshot.steeringTransmittedCrsf = transmittedSteering;
        snapshot.safetyStatus = safetyStatus;
        snapshot.calibrating = calibrating;
        snapshot.inputValid = inputValid;
        snapshot.armed = outputsLive;
        snapshot.neutralSinceMs = result.neutralSinceMs;
        snapshot.sampledMs = input.nowMs;
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
            OutputSafetyDecision safetyDecision = arbitrateOutputSafety(
                outputs.controlSafetyStatus, !stale);
            // Independent final veto, even if the UI/serial loop has stalled.
            if (rfBenchEnabled() && !rfBenchArmRequested() && safetyDecision.transmitRequested) {
                safetyDecision.status = OutputSafetyStatus::Cut;
                safetyDecision.transmitRequested = false;
            }
            const bool moduleEnabled=moduleLinkEnabled.load();
            if(!moduleEnabled){safetyDecision.status=OutputSafetyStatus::Cut;safetyDecision.transmitRequested=false;}
            crsfRuntimeOwned.outputSafetyStatus = safetyDecision.status;
            ElrsManagementRequest management;
            const bool requested = takeManagementRequest(management);
            const bool followup = !requested && uidFollowupPending &&
                static_cast<int32_t>(millis() - uidFollowupDueMs) >= 0;
            if(!moduleEnabled){
                uidFollowupPending=false; // Discard queued requests while paused.
            } else if (requested || followup) {
                if (followup) {
                    management = ElrsManagementRequest{};
                    management.action = ElrsManagementAction::ReadUid;
                    management.target = uidFollowupTarget;
                    uidFollowupPending = false;
                }
                if(management.action!=ElrsManagementAction::EscProgrammer ||
                   escRfRequestAllowed(stale,safetyDecision))
                    sendManagementRequest(management);
                else recordDiagnostic(DiagnosticKind::EscBlocked,stale,unsigned(safetyDecision.status));
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

// Read-only snapshots for tools/input_monitor.py. Use the existing input cache
// and control-task snapshot; never perform extra I2C reads or alter outputs.
void printInputMonitorSnapshot()
{
    auto control = readControlState();
    if (inputMonitorHalted) {
        // No control task exists in this mode. Report measured ADCs, never the
        // initial placeholder snapshot. Percentages are unavailable here.
        control.rawAdc = control.adc = analogRead(THROTTLE_ADC_PIN);
        control.steeringAdc = analogRead(STEERING_ADC_PIN);
        control.inputValid = false;
        if (transmitterUi.txVoltageSenseEnabled())
            filteredTxBatteryAdcMv = analogReadMilliVolts(TX_BATTERY_ADC_PIN);
    }
    Serial.printf("@INPUT_BEGIN\t1\t%lu\t%s\t%u\t%u\t%d\t%u\t%d\t%u\t%lu\t%u\n",
        static_cast<unsigned long>(millis()), inputMonitorHalted ? "HALTED_SAFE" : safetyStatusName(readCrsfRuntime().outputSafetyStatus),
        control.rawAdc, control.adc, control.rawPermille, control.steeringAdc,
        control.steeringRawPermille, control.inputValid,
        static_cast<unsigned long>(filteredTxBatteryAdcMv), transmitterUi.txVoltageSenseEnabled());
    const auto rf = readCrsfRuntime();
    const auto telemetry = readTelemetrySnapshot();
    portENTER_CRITICAL(&rfBenchMux);
    const bool benchAuthorized = rfBenchSession.authorized();
    portEXIT_CRITICAL(&rfBenchMux);
    Serial.printf("@INPUT_RF\t%u\t%u\t%u\t%u\t%u\t%u\t%lu\n",
        benchAuthorized, rfBenchArmRequested(), control.transmittedCrsf,
        control.steeringTransmittedCrsf, telemetry.linkValid,
        telemetry.uplinkLinkQuality, static_cast<unsigned long>(rf.channelFramesSent));
#ifdef JR_R1_XIAO_PCB
    for (unsigned i = 0; i < 2; ++i) {
        const auto &sample = latestPcbInputs[i];
        Serial.printf("@INPUT_MCP\t%u\t%u\t%u\t%lu\n", i + 1,
            sample.available, sample.levels,
            static_cast<unsigned long>(i ? pcbControls.readErrors() : pcbKeypad.readErrors()));
    }
#endif
    const auto inputStates=currentPhysicalInputStates();
    for (size_t i = 0; i < kPhysicalControlCount; ++i) {
        const auto id = static_cast<PhysicalControlId>(i);
        const auto &state = inputStates.state(id);
        const auto &descriptor = physicalControlDescriptor(i);
        Serial.printf("@INPUT_ROW\t%u\t%s\t%s\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%s\t%lu\n",
            static_cast<unsigned>(i), descriptor.name, r1InputEndpointLabel(id),
            static_cast<unsigned>(descriptor.kind), state.mapped, state.faulted,
            state.rawPrimary, state.rawSecondary, state.stablePrimary, state.stableSecondary,
            inputEventName(state.lastEvent), static_cast<unsigned long>(state.lastEventMs));
    }
    Serial.printf("@INPUT_END\t%u\n", static_cast<unsigned>(kPhysicalControlCount));
}

// Read-only UTC diagnostics remain independent of position/fix availability.
void printGpsTimeTelemetrySnapshot(const CrsfTelemetryState &t,uint32_t now)
{
    const char *status=!t.gpsTimeFrames?"no-packets":!t.gpsUtcYear?"no-valid-time":
        gpsTimeAvailable(t,now)?"utc-available":"stale";
    Serial.printf("@GPSTIME status=%s frames=%lu rejected=%lu age_ms=%ld utc=%04u-%02u-%02uT%02u:%02u:%02u.%03uZ\n",
        status,static_cast<unsigned long>(t.gpsTimeFrames),
        static_cast<unsigned long>(t.gpsTimeRejectedFrames),
        t.gpsUtcYear?static_cast<long>(uint32_t(now-t.gpsTimeUpdatedMs)):-1L,
        t.gpsUtcYear,t.gpsUtcMonth,t.gpsUtcDay,t.gpsUtcHour,t.gpsUtcMinute,
        t.gpsUtcSecond,t.gpsUtcMillisecond);
}

// Read-only telemetry snapshot for RF bench verification over USB.
void printGpsTelemetrySnapshot()
{
    const auto t=readTelemetrySnapshot();
    const uint32_t now=millis();
    const char *status=!t.gpsWireFrames?"no-packets":!t.gpsDecodedFrames?"no-decodable-packets":
        uint32_t(now-t.gpsDecodedUpdatedMs)>3000?"stale":
        gpsPositionAvailable(t,now)?"position-available":gpsSpeedAvailable(t,now)?"speed-only":"no-usable-position";
    // Packet ages are -1 before the first packet. Coordinates stay private.
    Serial.printf("@GPS status=%s packets=%lu decoded=%lu rejected=%lu packet_age_ms=%ld decoded_age_ms=%ld payload_bytes=%u position_available=%u fix_confirmed=%u satellites=%u course_valid=%u speed_dkmh=%u speed_available=%u speed_age_ms=%ld speed_packets=%lu\n",
        status,static_cast<unsigned long>(t.gpsWireFrames),static_cast<unsigned long>(t.gpsDecodedFrames),
        static_cast<unsigned long>(t.gpsRejectedFrames),
        t.gpsWireFrames?static_cast<long>(uint32_t(now-t.gpsWireUpdatedMs)):-1L,
        t.gpsDecodedFrames?static_cast<long>(uint32_t(now-t.gpsDecodedUpdatedMs)):-1L,
        t.gpsLastPayloadLength,gpsPositionAvailable(t,now),gpsFixConfirmed(t,now),
        t.satellites,t.gpsCourseValid,t.groundSpeedDeciKmh,gpsSpeedAvailable(t,now),
        t.gpsSpeedFrames?static_cast<long>(uint32_t(now-t.gpsSpeedUpdatedMs)):-1L,
        static_cast<unsigned long>(t.gpsSpeedFrames));
    printGpsTimeTelemetrySnapshot(t,now);
}

void printCellTelemetrySnapshot()
{
    const auto t = readTelemetrySnapshot();
    Serial.printf("@CELLS valid=%u age_ms=%lu link=%u lq=%u rx_valid=%u rx_dv=%u frames=%lu crc_errors=%lu\n",
        t.vehicleCellsValid, static_cast<unsigned long>(millis()-t.vehicleCellsUpdatedMs),
        t.linkValid, t.uplinkLinkQuality, t.receiverVoltageValid, t.receiverVoltageDecivolts,
        static_cast<unsigned long>(t.validFrames), static_cast<unsigned long>(t.crcErrors));
    Serial.printf("@FAS voltage_valid=%u voltage_mv=%u voltage_raw_cv=%u current_valid=%u current_raw_da=%u within_300a=%u temperature_valid=%u temperature_dc=%d\n",
        t.fasVoltageValid,t.fasVoltageMv,t.fasVoltageMv/10,t.fasCurrentValid,t.fasCurrentDeciA,
        t.fasCurrentDeciA<=3000,t.temperatureValid,t.temperatureDeciC);
    Serial.printf("@ERS battery_role=%s valid=%u voltage_dv=%u current_da=%u used_mah=%lu remaining_valid=%u remaining_pct=%u age_ms=%lu rpm_valid=%u rpm=%ld\n",
        transmitterUi.portalStandardBatteryVehicle()?"vehicle":"receiver",t.ersBatteryValid,
        t.ersVoltageDeciV,t.ersCurrentDeciA,static_cast<unsigned long>(t.ersCapacityMah),
        t.ersRemainingValid,t.ersRemainingPercent,static_cast<unsigned long>(millis()-t.ersBatteryUpdatedMs),
        t.rpmValid,static_cast<long>(t.rpm));
    if (t.vehicleCellsValid)
        Serial.printf("@CELLS_MV %u %u %u %u total=%u minimum=%u\n",
            t.vehicleCellsMv[0],t.vehicleCellsMv[1],t.vehicleCellsMv[2],t.vehicleCellsMv[3],
            t.vehiclePackMv,t.vehicleMinimumCellMv);
}

void printWifiSetupStatus()
{
    const auto &wifi = SetupPortal::state();
    Serial.printf("WIFI: %s; %s; recovery=%s headless=%s\n", wifi.message,
        wifi.running ? "web server ready" : "web server not ready",
        transmitterUi.hardwareRecovery() ? "yes" : "no",
        transmitterUi.headlessTesting() ? "yes" : "no");
    if (wifi.hotspot)
        Serial.printf("WIFI: join %s then open http://192.168.4.1\n", wifi.apName);
    if (wifi.connected)
        Serial.printf("WIFI: home address http://%s or http://%s.local\n", wifi.ip, wifi.activeHostname);
    // Only an explicit USB command prints credentials; never periodic logs.
    if (wifi.running && wifi.apPassword[0])
        Serial.printf("WIFI: username=r1 setup-password=%s (boot PIN is separate)\n", wifi.apPassword);
    else Serial.println("WIFI: wait for setup, then send WIFISTATUS again");
}

bool consumeScreenshotCommand()
{
    if(!Serial.available())return false;
    DiagnosticConsoleGuard console;
    while (Serial.available() > 0) {
        const char character = static_cast<char>(Serial.read());
        if (character == '\r') continue;
        if (character == '\n') {
            usbCommand[usbCommandLength] = '\0';
            const bool matched = strcmp(usbCommand, "SCREENSHOT") == 0;
            if (strncmp(usbCommand, "PINRESET ", 9) == 0) {
                const bool cut = readChannelOutputs().controlSafetyStatus != OutputSafetyStatus::Armed &&
                    readCrsfRuntime().outputSafetyStatus != OutputSafetyStatus::Armed;
                const bool ok = cut && transmitterUi.recoverPinOverUsb(usbCommand + 9);
                Serial.println(ok ? "PIN reset saved; outputs CUT; Walk mode unchanged." :
                    "PIN reset rejected: outputs must be CUT and PIN must be four digits.");
            }
            if (strncmp(usbCommand, "RFTEST ", 7) == 0) {
                const bool allowed = (inputMonitorHalted || controlTaskHandle != nullptr) &&
                    transmitterUi.verifyPin(usbCommand + 7);
                portENTER_CRITICAL(&rfBenchMux);
                const bool accepted = rfBenchSession.enable(allowed, millis());
                portEXIT_CRITICAL(&rfBenchMux);
                if (accepted) {
                    transmitterUi.setRfBenchMode(true);
                    requestControlReset();
                }
                Serial.println(accepted ? "@RF_REPLY Bench mode enabled; outputs CUT. A requests arm." :
                    "@RF_REPLY Bench mode rejected: PIN incorrect or startup not ready.");
            }
            if (strcmp(usbCommand, "RFKEEP") == 0) {
                bool healthy = true;
#ifdef JR_R1_XIAO_PCB
                healthy = latestPcbInputs[0].available && latestPcbInputs[1].available;
#endif
                for (size_t i=0; i<kPhysicalControlCount; ++i)
                    if (currentPhysicalInputStates().state(static_cast<PhysicalControlId>(i)).faulted) healthy = false;
                portENTER_CRITICAL(&rfBenchMux);
                rfBenchSession.heartbeat(millis(), healthy);
                portEXIT_CRITICAL(&rfBenchMux);
            }
            if (strcmp(usbCommand, "RFCUT") == 0) {
                requestControlReset();
                Serial.println("@RF_REPLY Outputs CUT. Press A to request arm again.");
            }
            if (strcmp(usbCommand, "RFARM") == 0) {
                const auto state = readControlState();
                const bool ready = startupPreflightComplete && state.inputValid &&
                    !state.calibrating && !transmitterUi.calibrationActive() &&
                    !SetupPortal::active() &&
                    abs(int(state.adc) - int(transmitterUi.throttleCentreAdc())) <= ARM_NEUTRAL_ADC_WINDOW &&
                    currentStartupUnsafeMask() == 0 && inputExpanderReady;
                requestControlReset(); // Every explicit arm requires a fresh neutral/link dwell.
                portENTER_CRITICAL(&rfBenchMux);
                const bool accepted = rfBenchSession.requestArm(millis(), ready);
                portEXIT_CRITICAL(&rfBenchMux);
                Serial.println(accepted ? "@RF_REPLY Arm requested; waiting for receiver link and neutral checks." :
                    "@RF_REPLY Arm rejected: centre controls, close Wi-Fi setup/calibration, check inputs and bench session.");
            }
            if (strcmp(usbCommand, "HEADLESS ON") == 0) {
                if (transmitterUi.setHeadlessTesting(true)) {
                    SetupPortal::start();
                    Serial.println("HEADLESS: saved ON; LCD/touch disabled; normal arming inhibited. Send WIFISTATUS for access details.");
                } else Serial.println("HEADLESS: setting save failed; outputs requested CUT; retry before reboot");
            }
            if (strcmp(usbCommand, "HEADLESS OFF") == 0) {
                Serial.println(transmitterUi.setHeadlessTesting(false) ?
                    "HEADLESS: saved OFF for next boot; restart to retest LCD/touch. Current recovery latch unchanged." :
                    "HEADLESS: setting save failed; outputs requested CUT");
            }
            if (strcmp(usbCommand, "WIFISETUP") == 0) {
                transmitterUi.requestConfigurationCut();
                SetupPortal::start();
                Serial.println("WIFI: setup requested; outputs CUT. Send WIFISTATUS for access details.");
            }
            if (strcmp(usbCommand, "WIFICLOSE") == 0) {
                transmitterUi.requestConfigurationCut();
                SetupPortal::stop();
                Serial.println("WIFI: closing hotspot setup; recovery/arming latch unchanged; home web access may remain available.");
            }
            if(strncmp(usbCommand,"UITAP ",6)==0){
                int x=-1,y=-1;char extra=0;
                const bool valid=sscanf(usbCommand+6,"%d %d %c",&x,&y,&extra)==2&&
                    x>=0&&y>=0&&x<transmitterUi.screenWidth()&&y<transmitterUi.screenHeight();
                const bool ok=valid&&transmitterUi.portalRemoteInput(x,y,0);
                Serial.println(ok?"@UI_REPLY Tap applied":"@UI_REPLY Tap rejected; requires CUT, web setup and normal PIN guards");
            }
            if (strcmp(usbCommand, "WIFISTATUS") == 0) printWifiSetupStatus();
            if (strcmp(usbCommand, "INPUTMON") == 0) printInputMonitorSnapshot();
            if(strcmp(usbCommand,"INPUTSTATS")==0){
                const auto sample=readInputSample();uint32_t dropped,logs;size_t queued;
                portENTER_CRITICAL(&inputDispatchMux);dropped=inputDispatch.dropped();queued=inputDispatch.size();portEXIT_CRITICAL(&inputDispatchMux);
                portENTER_CRITICAL(&diagnosticMux);logs=diagnosticEvents.dropped();portEXIT_CRITICAL(&diagnosticMux);
                Serial.printf("INPUTSTATS: fresh=%u ageMs=%lu maxGapUs=%lu queued=%u dropped=%lu diagnosticDrops=%lu\n",
                    physicalInputSampleFresh(millis(),sample.sampledMs),(unsigned long)(millis()-sample.sampledMs),
                    (unsigned long)sample.maximumGapUs,unsigned(queued),(unsigned long)dropped,(unsigned long)logs);
            }
            if(strcmp(usbCommand,"ESCPROGSTATUS")==0){
                const auto &e=transmitterUi.escProgrammerStatus();
                const auto o=readChannelOutputs();const auto r=readCrsfRuntime();const auto t=readTelemetrySnapshot();
                Serial.printf("ESC RF STATUS: state=%u selected=%u interlock=%u flags=%u pending=%u safety=%s CH5=%u CH12=%u ageUs=%lu rxSeq=%lu rxType=%02x rxLength=%u status=%s\n",
                    unsigned(e.state()),e.selected(),e.blocksArm(),e.flags(),readControlCommands().configurationPending,safetyStatusName(r.outputSafetyStatus),o.model.safe(5),o.model.safe(EscWire::SelectorChannel),
                    static_cast<unsigned long>(micros()-o.updatedUs),static_cast<unsigned long>(t.rxParameterSequence),t.rxParameterType,t.rxParameterLength,e.status());
                EscWire::Message m;if(EscWire::parse(t.rxParameterType,t.rxParameterPayload,t.rxParameterLength,true,m))
                    Serial.printf("ESC RF RX: op=%u seq=%u status=%u flags=%u\n",unsigned(m.op),m.sequence,unsigned(m.status),m.flags);
            }
            if (strcmp(usbCommand, "TELEMETRY") == 0) {printCellTelemetrySnapshot();printGpsTelemetrySnapshot();}
            if (strcmp(usbCommand, "GPSSTATUS") == 0) printGpsTelemetrySnapshot();
            if (strcmp(usbCommand, "GPSTIME") == 0) printGpsTimeTelemetrySnapshot(readTelemetrySnapshot(),millis());
            if(strcmp(usbCommand,"ERSSTATUS")==0){
                const auto t=readTelemetrySnapshot();
                Serial.printf("ERS RX: configFrames=%lu accepted=%lu lastType=%02x header=%02x %02x %02x %02x %02x\n",
                    static_cast<unsigned long>(t.configWireFrames),static_cast<unsigned long>(t.ersSequence),t.lastConfigType,
                    t.lastConfigHeader[0],t.lastConfigHeader[1],t.lastConfigHeader[2],t.lastConfigHeader[3],t.lastConfigHeader[4]);
            }
            if(strcmp(usbCommand,"MEMORY")==0||strcmp(usbCommand,"memory")==0)MemoryDiagnostics::print(Serial);
            if (strcmp(usbCommand, "BOOTLOG") == 0 || strcmp(usbCommand, "bootlog") == 0)
                bootDiagnosticsDump();
#ifdef JR_R1_XIAO_PCB
            if (strcmp(usbCommand, "INPUTS") == 0 || strcmp(usbCommand, "inputs") == 0)
                printPcbInputs();
#endif
            if (strcmp(usbCommand, "UICACHE ON") == 0)
                transmitterUi.setDisplayCacheEnabled(true);
            if (strcmp(usbCommand, "UICACHE OFF") == 0)
                transmitterUi.setDisplayCacheEnabled(false);
            memset(usbCommand, 0, sizeof(usbCommand));
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

#include "input_bench.inc"
} // namespace

void setup()
{
    transmitterUi.setSafetyCutHandler(requestUiConfigurationCut);
    // Establish the inverted CRSF bus idle level immediately, before USB/UI
    // startup work. The Ranger can remain powered across an S3 reset, so a
    // deliberate quiet interval gives its handset UART watchdog a clean
    // opportunity to resynchronise before our first channel frame.
    gpio_reset_pin(CRSF_SIGNAL_PIN);
    gpio_set_direction(CRSF_SIGNAL_PIN, GPIO_MODE_INPUT);
    gpio_pulldown_en(CRSF_SIGNAL_PIN);
    gpio_pullup_dis(CRSF_SIGNAL_PIN);

    Serial.begin(115200);
    InputBus::begin(); // Before any MCP/touch access or input task starts.
    DiagnosticConsoleGuard::mutex(); // Initialise the static console mutex before its worker.
    if(xTaskCreatePinnedToCore(diagnosticTask,"diagnostics",4096,nullptr,1,&diagnosticTaskHandle,0)!=pdPASS)
        Serial.println("DIAGNOSTICS: worker unavailable; events may be dropped");
    delay(CRSF_STARTUP_BUS_QUIET_MS);
#if JR_R1_INPUT_BENCH
    benchBegin();return;
#endif
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
    MemoryDiagnostics::configure(crsfTaskHandle,nullptr,false);

    const uint32_t heapBeforeUi = ESP.getFreeHeap();
#ifdef JR_R1_XIAO_PCB
    if (!transmitterUi.begin()) {
        Serial.println("PCB RECOVERY: peripheral startup incomplete or headless testing enabled. Remote UI/input tests available; outputs remain safe.");
        // No control task or arming path starts after an incomplete hardware
        // sequence. The already-running CRSF task keeps sending safe frames.
        bootDiagnosticsStage("halted-safe");
        inputMonitorHalted = true;
        physicalInputs.begin(kR1InputMap, kR1InputMapCount);
        configureDirectGpioInputs();
        transmitterUi.enterHardwareRecovery();
        SetupPortal::begin(&transmitterUi);
        SetupPortal::start();
        Serial.println("RECOVERY: Wi-Fi setup requested automatically. USB: WIFISTATUS for credentials, BOOTLOG, INPUTMON, MEMORY; HEADLESS ON to skip LCD/touch on future boots.");
        MemoryDiagnostics::configure(crsfTaskHandle, nullptr, true);
        for (;;) {
            MemoryDiagnostics::sample();
            // Sample only; never feed events to bindings or start live control.
            latestPcbInputs[0].available = pcbKeypad.online() && pcbKeypad.read(latestPcbInputs[0].levels);
            latestPcbInputs[1].available = pcbControls.online() && pcbControls.read(latestPcbInputs[1].levels);
            physicalInputEvents.clear();
            physicalInputs.update(latestPcbInputs, 2, readDirectGpioLevels(), millis(), physicalInputEvents);
            // No binding actions, configuration publication, management requests
            // or control task: CRSF continues sending its existing safe frames.
            consumeScreenshotCommand();
            if (rfBenchEnabled()) {
                inputMonitorHalted = false;
                transmitterUi.setRfBenchMode(true);
                Serial.println("RF BENCH: starting normal input/control tasks; USB lease and explicit ARM required");
                break;
            }
            transmitterUi.setPhysicalInputStates(physicalInputs.states());
            const uint16_t throttle = analogRead(THROTTLE_ADC_PIN);
            const uint16_t steering = analogRead(STEERING_ADC_PIN);
            uint16_t battery = 0;
            if (transmitterUi.txVoltageSenseEnabled()) {
                filteredTxBatteryAdcMv = analogReadMilliVolts(TX_BATTERY_ADC_PIN);
                battery = (filteredTxBatteryAdcMv *
                    (TX_BATTERY_DIVIDER_TOP_OHMS + TX_BATTERY_DIVIDER_BOTTOM_OHMS) /
                    TX_BATTERY_DIVIDER_BOTTOM_OHMS + 50) / 100;
            }
            transmitterUi.update(throttle, steering, 0, 0, CRSF_CHANNEL_MID,
                OutputSafetyStatus::ControlStale, readTelemetrySnapshot(), battery,
                battery >= 25, CrsfChannelOutputModel{}, false);
            // Safe until the separately authorized RF bench path above starts control.
            SetupPortal::tick(true, true);
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
    publishControlState(ControlStateSnapshot());
    // Last flash checkpoint before live control starts; no periodic flash writes.
    bootDiagnosticsStage("control-starting");
    xTaskCreatePinnedToCore(controlTask, "control-input", 6144, nullptr, 5,
                            &controlTaskHandle, 1);
    inputUiContext.store(transmitterUi.activeControlContext());
    if(xTaskCreatePinnedToCore(inputTask,"physical-input",4096,nullptr,4,&inputTaskHandle,1)!=pdPASS){
        requestUiConfigurationCut();Serial.println("INPUT: sampler unavailable; startup remains inhibited");
    }
    MemoryDiagnostics::configure(crsfTaskHandle,controlTaskHandle,true);
    MemoryDiagnostics::configureWorkers(inputTaskHandle,diagnosticTaskHandle);
    if (!rfBenchEnabled()) SetupPortal::begin(&transmitterUi);
    nextLogMs = millis();
}

void loop()
{
#if JR_R1_INPUT_BENCH
    benchLoop();return;
#endif
    MemoryDiagnostics::sample();
    if (consumeScreenshotCommand()) {
        // Force the whole channel model safe immediately; the CRSF task must
        // never retain live outputs while the UI task streams a screenshot.
        requestControlReset();
        DiagnosticConsoleGuard console;
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
    const auto physical=readInputSample();
    inputExpanderReady=physical.available&&physicalInputSampleFresh(nowMs,physical.sampledMs);
#ifdef JR_R1_XIAO_PCB
    latestPcbInputs[0]=physical.chips[0];latestPcbInputs[1]=physical.chips[1];
#endif
    if(physical.ready&&physicalInputSampleFresh(nowMs,physical.sampledMs)&&!startupPreflightComplete){
        if(!startupInputSettledAtMs)startupInputSettledAtMs=physical.sampledMs;
        if(nowMs-startupInputSettledAtMs>=STARTUP_INPUT_SETTLE_MS){
            startupWarningMask=physical.unsafeMask;
            transmitterUi.setStartupPreflight(false,startupWarningMask);
            if(!startupWarningMask&&physical.available){
                startupPreflightComplete=true;transmitterUi.setStartupPreflight(true,0);completeStartupPreflight();
            }
        }
    }
    transmitterUi.setPhysicalInputStates(physical.states);
    dispatchQueuedInputs(nowMs);
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
    transmitterUi.updateModulePower(txVoltageValid,nowMs);
    moduleLinkEnabled.store(transmitterUi.moduleLinkEnabled());
    const SharedChannelOutputState channelOutputs = readChannelOutputs();
    auto monitorInputs=readMixInputs(nowMs);
    monitorInputs.values[0]=control.steeringRawPermille;monitorInputs.values[1]=control.rawPermille;
    // Do not relabel cached calibrated values as a fresh input sample. The UI
    // history can then leave gaps across stalls instead of inventing a trace.
    const uint32_t controlSampleAgeMs=millis()-control.sampledMs;
    monitorInputs.valid&=~uint64_t{3};
    if(control.inputValid&&controlSampleAgeMs<=100)monitorInputs.valid|=3;
    CrsfChannelOutputModel monitorBindings;
    portENTER_CRITICAL(&bindingResolverMux);
    bindingResolver.applyChannelOutputs(monitorBindings,configuredRoleOwnedChannels(lastPublishedControlConfiguration));
    portEXIT_CRITICAL(&bindingResolverMux);
    transmitterUi.setMixMonitorInputs(monitorInputs,monitorBindings,control.sampledMs);
    transmitterUi.update(control.adc, control.steeringAdc, control.rawPermille,
                      control.outputPermille,
                      control.transmittedCrsf,
                      crsfRuntime.outputSafetyStatus,
                      telemetry, txVoltageDecivolts, txVoltageValid,
                      channelOutputs.model,
                      crsfRuntime.outputSafetyStatus ==
                          OutputSafetyStatus::Armed);
    inputUiContext.store(transmitterUi.activeControlContext());
    moduleLinkEnabled.store(transmitterUi.moduleLinkEnabled());
    telemetryParser.setStandardBatteryVehicle(transmitterUi.portalStandardBatteryVehicle());
    ElrsManagementRequest management;
    if (transmitterUi.consumeElrsManagementRequest(management) && moduleLinkEnabled.load()) {
        requestControlReset();
        if (!queueManagementRequest(management))
            Serial.println("ELRS MGMT: queue busy; request discarded");
    }
    synchronizeUiBindings();
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
    const bool wifiArmRequested=readControlConfiguration().armRequested;
    const auto wifiSafety=readCrsfRuntime().outputSafetyStatus;
    if(SetupPortal::state().running){
        SetupPortal::RuntimeTiming timing;timing.ready=true;
        timing.controlMaximumGapUs=readControlState().maximumTaskGapUs;
        timing.radioMaximumGapUs=readCrsfRuntime().maximumTaskGapUs;
        const auto sample=readInputSample();
        timing.inputMaximumGapUs=sample.maximumGapUs;
        timing.inputAgeMs=millis()-sample.sampledMs;
        timing.inputHealthy=sample.available&&physicalInputSampleFresh(millis(),sample.sampledMs);
        portENTER_CRITICAL(&inputDispatchMux);timing.inputDropped=inputDispatch.dropped();portEXIT_CRITICAL(&inputDispatchMux);
        portENTER_CRITICAL(&diagnosticMux);timing.diagnosticDropped=diagnosticEvents.dropped();portEXIT_CRITICAL(&diagnosticMux);
        SetupPortal::setRuntimeTiming(timing);
    }
    SetupPortal::tick(remoteControlAllowed(wifiArmRequested,wifiSafety),
        wifiConnectionAllowed(wifiArmRequested,wifiSafety));
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
                      physical.states.state(PhysicalControlId::TimerButton).stablePrimary
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
