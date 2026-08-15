#include "channel2_ui.h"
#include "crsf_channel_output.h"
#include "crsf_management.h"
#include "simulator_platform.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
uint16_t adcForPermille(const Channel2Ui &ui, int16_t permille)
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
    uint32_t runForMs = 0;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--screenshot") == 0 && index + 1 < argc)
            screenshotPath = argv[++index];
        else if (std::strcmp(argv[index], "--run-ms") == 0 && index + 1 < argc)
            runForMs = static_cast<uint32_t>(std::strtoul(argv[++index], nullptr, 10));
    }

    Channel2Ui ui;
    if (!ui.begin()) return 1;

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
        telemetry.receiverVoltageDecivolts = controls.receiverVoltageDecivolts;
        telemetry.linkUpdatedMs = now;
        telemetry.batteryUpdatedMs = now;

        const uint16_t rawAdc = adcForPermille(ui, controls.throttlePermille);
        const int16_t rawPermille = ui.mapAdcToPermille(rawAdc);
        const int16_t configured = ui.applySettings(rawPermille);
        const int16_t driveLimited = ui.applyDriveMode(configured, rawPermille);
        const int16_t output = ui.applyChannelSpeed(driveLimited, now);
        const uint16_t crsf = crsfValueFromPermille(output);
        const OutputSafetyStatus safety = ui.throttleEnabled()
            ? (ui.armRequested() ? OutputSafetyStatus::Armed : OutputSafetyStatus::Cut)
            : OutputSafetyStatus::ChannelDisabled;

        CrsfChannelOutputModel channelOutputs;
        channelOutputs.configure(ui.steeringChannel(), CRSF_CHANNEL_MID,
                                 CRSF_CHANNEL_MID, ui.steeringEnabled());
        channelOutputs.configure(ui.throttleChannel(), crsf, CRSF_CHANNEL_MID,
                                 ui.throttleEnabled());
        ui.update(rawAdc, rawPermille, output, crsf, safety, telemetry,
                  controls.transmitterVoltageDecivolts, true,
                  channelOutputs, safety == OutputSafetyStatus::Armed);

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
