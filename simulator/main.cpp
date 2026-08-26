#include "transmitter_ui.h"
#include "crsf_channel_output.h"
#include "crsf_management.h"
#include "simulator_platform.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
uint16_t adcForPermille(const TransmitterUi &ui, int16_t permille)
{
    const ProportionalCalibration &calibration = ui.calibration();
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
    std::string screenshotPath;
    std::string initialPage;
    uint8_t initialChannel = 2;
    int initialModel = -1;
    bool wakeSaverControl = false;
    int receiverVoltageOverride = -1;
    uint32_t runForMs = 0;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--screenshot") == 0 && index + 1 < argc)
            screenshotPath = argv[++index];
        else if (std::strcmp(argv[index], "--page") == 0 && index + 1 < argc)
            initialPage = argv[++index];
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

    TransmitterUi ui;
    ChannelSlewRuntime channelSlew;
    if (!ui.begin()) return 1;
    ui.setStartupPreflight(true, 0);
    ui.selectSimulatorChannel(initialChannel);
    if (initialPage == "boot")
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
    else if (initialPage == "esc")
        ui.showSimulatorPage(UiPage::EscProgramming);

    CrsfTelemetryState telemetry;
    uint32_t previousMs = millis();
    while (!simulatorQuitRequested()) {
        simulatorPollEvents();
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

        const uint16_t rawAdc = adcForPermille(ui, controls.throttlePermille);
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
        const OutputSafetyStatus safety = ui.throttleEnabled()
            ? (ui.armRequested() ? OutputSafetyStatus::Armed : OutputSafetyStatus::Cut)
            : OutputSafetyStatus::ChannelDisabled;

        CrsfChannelOutputModel channelOutputs;
        channelOutputs.configure(ui.steeringChannel(), CRSF_CHANNEL_MID,
                                 CRSF_CHANNEL_MID, ui.steeringEnabled());
        channelOutputs.configure(ui.throttleChannel(), crsf, CRSF_CHANNEL_MID,
                                 ui.throttleEnabled());
        ui.update(rawAdc, rawAdc, rawPermille, output, crsf, safety, telemetry,
                  controls.transmitterVoltageDecivolts, true,
                  channelOutputs, safety == OutputSafetyStatus::Armed);
        if (ui.consumeSafetyResetRequested()) channelSlew.reset(now);
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

        if (runForMs && now >= runForMs) break;
        delay(5);
    }

    if (!screenshotPath.empty()) simulatorSaveScreenshot(screenshotPath.c_str());
    simulatorShutdown();
    return 0;
}
