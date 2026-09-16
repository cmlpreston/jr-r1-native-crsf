#include "transmitter_ui.h"
#include "crsf_channel_output.h"
#include "crsf_management.h"
#include "simulator_platform.h"
#include "tx_config_module.h"
#include "handset_inputs.h"
#include "binding_resolver.h"

#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace {
uint16_t adcForPermille(const ProportionalCalibration &calibration, int16_t permille)
{
    if (permille < 0) {
        return static_cast<uint16_t>(calibration.centre +
            static_cast<int32_t>(permille) *
            (calibration.centre - calibration.minimum) / 1000);
    }
    return static_cast<uint16_t>(calibration.centre +
        static_cast<int32_t>(permille) *
        (calibration.maximum - calibration.centre) / 1000);
}

void emulateManagementRequest(const ElrsManagementRequest &request,
                              CrsfTelemetryState &telemetry)
{
    if (request.action == ElrsManagementAction::TxParameter) {
        emulateTxConfig(request.parameter, telemetry); return;
    }
    uint8_t uid[kElrsUidLength] = {};
    if (request.action == ElrsManagementAction::SetBindPhrase)
        calculateElrsBindPhraseUid(request.phrase, uid);

    uint8_t *targetUid = request.target == ElrsManagementTarget::Transmitter
        ? telemetry.transmitterUid : telemetry.receiverUid;
    bool &targetValid = request.target == ElrsManagementTarget::Transmitter
        ? telemetry.transmitterUidValid : telemetry.receiverUidValid;
    uint32_t &updated = request.target == ElrsManagementTarget::Transmitter
        ? telemetry.transmitterUidUpdatedMs : telemetry.receiverUidUpdatedMs;

    if (request.action == ElrsManagementAction::SetBindPhrase)
        std::memcpy(targetUid, uid, sizeof(uid));
    targetValid = true;
    updated = millis() == 0 ? 1 : millis();
    std::printf("SIM: %s %s\n",
        request.target == ElrsManagementTarget::Transmitter ? "TX" : "RX",
        request.action == ElrsManagementAction::SetBindPhrase
            ? "bind phrase accepted" : "UID read");
}
}

int main(int argc, char **argv)
{
    bool auditUi=false;
    std::string screenshotPath;
    std::string initialPage;
#ifdef __EMSCRIPTEN__
    initialPage = "dashboard";
#endif
    uint8_t initialChannel = 2;
    static int initialModel = -1;
    static int initialTxParameter = 0;
    static bool wakeSaverControl = false;
    static int receiverVoltageOverride = -1;
    [[maybe_unused]] uint32_t runForMs = 0;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--ui-audit") == 0) auditUi=true;
        else if (std::strcmp(argv[index], "--screenshot") == 0 && index + 1 < argc)
            screenshotPath = argv[++index];
        else if (std::strcmp(argv[index], "--page") == 0 && index + 1 < argc)
            initialPage = argv[++index];
        else if (std::strcmp(argv[index], "--tx-parameter") == 0 && index + 1 < argc)
            initialTxParameter = std::atoi(argv[++index]);
        else if (std::strcmp(argv[index], "--channel") == 0 && index + 1 < argc)
            initialChannel = static_cast<uint8_t>(std::strtoul(argv[++index], nullptr, 10));
        else if (std::strcmp(argv[index], "--select-model") == 0 && index + 1 < argc)
            initialModel = static_cast<int>(std::strtol(argv[++index], nullptr, 10)) - 1;
        else if (std::strcmp(argv[index], "--wake-saver-control") == 0)
            wakeSaverControl = true;
        else if (std::strcmp(argv[index], "--rx-voltage") == 0 && index + 1 < argc)
            receiverVoltageOverride = static_cast<int>(
                std::strtol(argv[++index], nullptr, 10));
        else if (std::strcmp(argv[index], "--run-ms") == 0 && index + 1 < argc)
            runForMs = static_cast<uint32_t>(std::strtoul(argv[++index], nullptr, 10));
    }

    static TransmitterUi ui;
    static ChannelSlewRuntime channelSlew;
    static SimulatorHandsetInputs handset;
    static BindingResolver bindings;
    static bool bindingsLoaded = false;
    if (!ui.begin()) return 1;
    ui.setStartupPreflight(true, 0);
    if(auditUi)return ui.runSimulatorUiAudit()?0:1;
    ui.selectSimulatorChannel(initialChannel);
    if (initialPage == "dashboard")
        ui.showSimulatorPage(UiPage::Dashboard);
    else if (initialPage == "tune") ui.showSimulatorPage(UiPage::Tune);
    else if (initialPage == "telemetry") ui.showSimulatorPage(UiPage::Telemetry);
    else if (initialPage == "gps") ui.showSimulatorPage(UiPage::Gps);
    else if (initialPage == "monitor") ui.showSimulatorPage(UiPage::ChannelMonitor);
    else if (initialPage == "widgets") ui.showSimulatorPage(UiPage::WidgetSelect);
    else if (initialPage == "radio") ui.showSimulatorPage(UiPage::RadioDiagnostics);
    else if (initialPage == "channels") ui.showSimulatorPage(UiPage::Channels);
    else if (initialPage == "roles") ui.showSimulatorPage(UiPage::ChannelRoles);
    else if (initialPage == "channel") ui.showSimulatorPage(UiPage::ChannelPlaceholder);
    else if (initialPage == "throttle") ui.showSimulatorPage(UiPage::Channel2);
    else if (initialPage == "assignments") ui.showSimulatorPage(UiPage::ControlAssignments);
    else if (initialPage == "binding-editor") ui.showSimulatorPage(UiPage::BindingEditor);
    else if (initialPage == "inputs") ui.showSimulatorPage(UiPage::InputMonitor);
    else if (initialPage == "modes") ui.showSimulatorPage(UiPage::DriveModes);
    else if (initialPage == "tx-battery") ui.showSimulatorPage(UiPage::TxBatterySetup);
    else if (initialPage == "rx-battery") ui.showSimulatorPage(UiPage::RxBatterySetup);
    else if (initialPage == "binding") ui.showSimulatorPage(UiPage::ElrsBinding);
    else if (initialPage == "menu")
        ui.showSimulatorPage(UiPage::MainMenu);
    else if (initialPage == "calibration" || initialPage == "calibrate")
        ui.showSimulatorPage(UiPage::Calibration);
    else if (initialPage == "boot")
        ui.showSimulatorPage(UiPage::BootSplash);
    else if (initialPage == "saver" || initialPage == "screensaver")
        ui.showSimulatorPage(UiPage::LogoSaver);
    else if (initialPage == "setup")
        ui.showSimulatorPage(UiPage::Setup);
    else if (initialPage == "display")
        ui.showSimulatorPage(UiPage::DisplaySetup);
    else if (initialPage == "display2") {
        ui.selectSimulatorDisplayBank(1);
        ui.showSimulatorPage(UiPage::DisplaySetup);
    }
    else if (initialPage == "info" || initialPage == "firmware")
        ui.showSimulatorPage(UiPage::FirmwareInfo);
    else if (initialPage == "models")
        ui.showSimulatorPage(UiPage::ModelSelect);
    else if (initialPage == "response")
        ui.showSimulatorPage(UiPage::ThrottleResponse);
    else if (initialPage == "ramp")
        ui.showSimulatorPage(UiPage::ChannelSpeed);
    else if (initialPage == "tx-config")
        ui.showSimulatorPage(UiPage::ElrsTxConfig);
    else if (initialPage == "esc")
        ui.showSimulatorPage(UiPage::EscProgramming);

    static CrsfTelemetryState telemetry;
    static uint32_t previousMs = millis();
    auto frame = []() {
        simulatorPollEvents();
        uint16_t action; int16_t actionValue;
        while (simulatorConsumeAction(action, actionValue))
            ui.applyTransmitterFunctionAction(action, actionValue);
        const uint32_t now = millis();
        const uint32_t elapsed = now - previousMs;
        previousMs = now;
        if (elapsed) lv_tick_inc(elapsed);

        const SimulatorControls &controls = simulatorControls();
        telemetry.linkValid = controls.linkValid;
        telemetry.uplinkRssiDbm = controls.uplinkRssiDbm;
        telemetry.uplinkLinkQuality = controls.linkQualityPercent;
        telemetry.receiverVoltageValid = controls.linkValid;
        telemetry.receiverVoltageDecivolts = receiverVoltageOverride >= 0
            ? static_cast<uint16_t>(receiverVoltageOverride)
            : controls.receiverVoltageDecivolts;
        telemetry.linkUpdatedMs = now;
        telemetry.batteryUpdatedMs = now;
        // Optional simulator-only fixture. Never compiled into the ESP32 target.
        const bool sensors = controls.vehicleTelemetry && controls.linkValid;
        telemetry.gpsValid = telemetry.temperatureValid = telemetry.rpmValid = sensors;
        if (sensors) {
            const double phase = (now / 1000) * 0.12;
            telemetry.gpsUpdatedMs = telemetry.temperatureUpdatedMs = telemetry.rpmUpdatedMs = (now/1000)*1000;
            telemetry.latitudeE7 = GpsMap::DemoLat + static_cast<int32_t>(sin(phase)*4500);
            telemetry.longitudeE7 = GpsMap::DemoLon + static_cast<int32_t>(cos(phase)*6000);
            telemetry.groundSpeedDeciKmh = 220 + static_cast<int>(sin(phase)*50);
            telemetry.headingCentiDegrees = static_cast<uint16_t>(fmod(atan2(-6000.0*GpsMap::longitudeScale(GpsMap::DemoLat)*sin(phase),4500.0*cos(phase))*180.0/3.141592653589793+360.0,360.0)*100.0);
            telemetry.satellites = 12;
            telemetry.temperatureDeciC = 550 + static_cast<int>(sin(phase/4)*80);
            telemetry.rpm = 12000 + static_cast<int>(sin(phase)*3000);
        }

        const uint16_t rawAdc = adcForPermille(ui.calibration(), controls.throttlePermille);
        const auto steeringProfile = ui.steeringInputProfile();
        const uint16_t steeringAdc = adcForPermille(steeringProfile.calibration,
                                                   controls.steeringPermille);
        if (ui.consumeBindingsChanged() || !bindingsLoaded) {
            ControlBinding profile[kPhysicalControlCount];
            const size_t count = ui.copyBindings(profile, kPhysicalControlCount);
            if (!bindings.setBindings(profile, count).ok()) bindings.clear();
            bindingsLoaded = true;
            ui.applyTransmitterFunctionAction(static_cast<uint16_t>(TransmitterFunction::ArmCut), 0);
            channelSlew.reset(now);
        }
        PhysicalInputEventBuffer inputEvents;
        handset.update(controls.handsetContacts, controls.steeringPermille,
                       controls.throttlePermille, now, inputEvents);
        ui.setPhysicalInputStates(handset.states());
        ResolvedBindingActionBuffer resolved;
        bindings.processEvents(inputEvents,
            static_cast<uint8_t>(ContextAlways | ui.activeControlContext() |
                (ui.armRequested() && controls.linkValid && ui.throttleEnabled() &&
                 !ui.calibrationActive() ? ContextArmed : ContextCut)), resolved);
        bindings.update(now, resolved);
        for (size_t i = 0; i < resolved.size(); ++i) {
            const auto &item = resolved[i];
            if (item.target == AssignmentTarget::TransmitterFunction)
                ui.applyTransmitterFunctionAction(item.targetId, item.value);
            else if (item.target == AssignmentTarget::ModelParameter)
                ui.applyChannelParameterAction(item.targetId, item.value);
            else if (item.target == AssignmentTarget::DriveMode)
                ui.applyDriveModeAction(item.targetId, item.value);
        }
        const int16_t rawPermille = ui.mapAdcToPermille(rawAdc);
        const ControlConfigurationSnapshot configuration =
            ui.controlConfigurationSnapshot();
        const int16_t configured = applyConfiguredResponse(
            configuration, configuration.throttleChannel, rawPermille);
        const int16_t driveLimited = applyConfiguredDriveMode(
            configuration, configured, rawPermille);
        const int16_t adjusted = applyConfiguredChannelAdjustments(
            configuration, configuration.throttleChannel, driveLimited);
        const int16_t output = channelSlew.apply(
            configuration, configuration.throttleChannel, adjusted, now);
        const uint16_t crsf = crsfValueFromPermille(output);
        const OutputSafetyStatus safety = configuration.calibrationActive
            ? OutputSafetyStatus::Calibration : ui.throttleEnabled()
            ? (ui.armRequested() ? (controls.linkValid ? OutputSafetyStatus::Armed
                : OutputSafetyStatus::LinkRequired) : OutputSafetyStatus::Cut)
            : OutputSafetyStatus::ChannelDisabled;

        CrsfChannelOutputModel channelOutputs;
        const int16_t steeringResponse = applyConfiguredChannelAdjustments(configuration,
            configuration.steeringChannel, applyConfiguredResponse(configuration,
                configuration.steeringChannel,
                mapProportionalInputToPermille(steeringProfile, steeringAdc)));
        channelOutputs.configure(ui.steeringChannel(), crsfValueFromPermille(steeringResponse),
                                 CRSF_CHANNEL_MID, ui.steeringEnabled());
        channelOutputs.configure(ui.throttleChannel(), crsf, CRSF_CHANNEL_MID,
                                 ui.throttleEnabled());
        const uint16_t reserved = (uint16_t{1} << (ui.steeringChannel() - 1)) |
                                  (uint16_t{1} << (ui.throttleChannel() - 1));
        bindings.applyChannelOutputs(channelOutputs, reserved);
        ui.update(rawAdc, steeringAdc, rawPermille, output, crsf, safety, telemetry,
                  controls.transmitterVoltageDecivolts, true,
                  channelOutputs, safety == OutputSafetyStatus::Armed);
        if (ui.consumeSafetyResetRequested()) channelSlew.reset(now);
        if (initialTxParameter > 0 && ui.selectSimulatorTxParameter(initialTxParameter))
            initialTxParameter = 0;
        if (initialModel >= 0) {
            ui.selectSimulatorModel(static_cast<uint8_t>(initialModel));
            initialModel = -1;
        }
        if (wakeSaverControl) {
            PhysicalInputStateStore states;
            states.setMapped(PhysicalControlId::SteeringWheel, true);
            states.setValue(PhysicalControlId::SteeringWheel, 20);
            ui.setPhysicalInputStates(states); // below the noise threshold
            states.setValue(PhysicalControlId::SteeringWheel, 50);
            ui.setPhysicalInputStates(states); // deliberate movement wakes
            wakeSaverControl = false;
        }

        ElrsManagementRequest request;
        while (ui.consumeElrsManagementRequest(request))
            emulateManagementRequest(request, telemetry);

        simulatorServiceBridge();
    };
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(frame, 0, true);
#else
    while (!simulatorQuitRequested()) {
        frame();
        if (runForMs && millis() >= runForMs) break;
        delay(5);
    }
#endif

    if (!screenshotPath.empty()) simulatorSaveScreenshot(screenshotPath.c_str());
    simulatorShutdown();
    return 0;
}
