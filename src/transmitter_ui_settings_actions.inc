namespace {
enum class TxBatterySetting : uint8_t { Chemistry, CellCount, LowAlarm, VoltageSensing };
enum class DisplaySetting : uint8_t { Timeout, IdleAction, Brightness, Colours, ActivateNow, Orientation };
enum class SoundSetting : uint8_t { Buzzer, Clicks, Pitch, ClickLength, Test };
}

void TransmitterUi::controlTxBattery(uint8_t row, PageControlEvent event)
{
    const int direction = event == PageControlEvent::Decrease ? -1 : 1;
    auto &settings = txBatterySettings_;
    switch (static_cast<TxBatterySetting>(row)) {
        case TxBatterySetting::Chemistry:
            settings.chemistry = static_cast<BatteryChemistry>(
                (static_cast<int>(settings.chemistry) + direction + 4) % 4);
            settings.cellCount = min(settings.cellCount, maximumCellsFor(settings.chemistry));
            settings.lowAlarmDecivolts = defaultLowAlarmDecivolts(settings.chemistry, settings.cellCount);
            break;
        case TxBatterySetting::CellCount: {
            const int cells = constrain(int(settings.cellCount) + direction,
                                        1, int(maximumCellsFor(settings.chemistry)));
            if (cells == settings.cellCount) return;
            settings.cellCount = cells;
            settings.lowAlarmDecivolts = defaultLowAlarmDecivolts(settings.chemistry, settings.cellCount);
            break;
        }
        case TxBatterySetting::LowAlarm: {
            const int voltage = constrain(int(settings.lowAlarmDecivolts) + direction,
                int(kBatteryMinimumAlarmDecivolts), int(kBatteryMaximumAlarmDecivolts));
            if (voltage == settings.lowAlarmDecivolts) return;
            settings.lowAlarmDecivolts = voltage;
            break;
        }
        case TxBatterySetting::VoltageSensing:
            settings.voltageSenseEnabled = !settings.voltageSenseEnabled;
            break;
        default: return;
    }
    saveTxBatterySettings();
    playKeyClick();
    drawTxBatteryPage();
}

void TransmitterUi::controlDisplay(uint8_t row, PageControlEvent event)
{
    const int direction = event == PageControlEvent::Decrease ? -1 : 1;
    switch (static_cast<DisplaySetting>(row)) {
        case DisplaySetting::Timeout: {
            constexpr uint16_t timeouts[] = {0, 30, 60, 300};
            int index = 0;
            while (index < 4 && timeouts[index] != systemSettings_.screenTimeoutSeconds) ++index;
            if (index == 4) index = 2;
            systemSettings_.screenTimeoutSeconds = timeouts[(index + direction + 4) % 4];
            break;
        }
        case DisplaySetting::IdleAction:
            systemSettings_.logoScreensaverEnabled = !systemSettings_.logoScreensaverEnabled;
            break;
        case DisplaySetting::Brightness:
            systemSettings_.displayBrightnessPercent = constrain(
                int(systemSettings_.displayBrightnessPercent) + direction * 10, 20, 100);
            uiBrightnessPercent = systemSettings_.displayBrightnessPercent;
            applyBacklight(uiBrightnessPercent);
            break;
        case DisplaySetting::Colours:
            playKeyClick();
            drawColourSchemePage();
            return;
        case DisplaySetting::ActivateNow:
            playKeyClick();
            if (systemSettings_.logoScreensaverEnabled) {
                logoSaverReturnPage_ = UiPage::DisplaySetup;
                drawLogoSaver();
            } else {
                sleepDisplay();
            }
            return;
        case DisplaySetting::Orientation:
            requestDisplayRotation();
            return;
        default: return;
    }
    saveSystemSettings();
    playKeyClick();
    drawDisplayPage();
}

void TransmitterUi::controlSound(uint8_t item, PageControlEvent event)
{
    playKeyClick();
    const int direction = event == PageControlEvent::Decrease ? -1 : 1;
    switch (static_cast<SoundSetting>(item)) {
        case SoundSetting::Buzzer:
            systemSettings_.buzzerEnabled = !systemSettings_.buzzerEnabled;
            if (!systemSettings_.buzzerEnabled) stopBuzzer();
            break;
        case SoundSetting::Clicks:
            systemSettings_.keyClicksEnabled = !systemSettings_.keyClicksEnabled;
            break;
        case SoundSetting::Pitch:
            if (direction < 0 && systemSettings_.buzzerPitchHz <= kBuzzerMinimumPitchHz) return;
            if (direction > 0 && systemSettings_.buzzerPitchHz >= kBuzzerMaximumPitchHz) return;
            systemSettings_.buzzerPitchHz += direction * kBuzzerPitchStepHz;
            break;
        case SoundSetting::ClickLength:
            if (direction < 0 && systemSettings_.keyClickDurationMs <= kBuzzerMinimumDurationMs) return;
            if (direction > 0 && systemSettings_.keyClickDurationMs >= kBuzzerMaximumDurationMs) return;
            systemSettings_.keyClickDurationMs += direction * kBuzzerDurationStepMs;
            break;
        case SoundSetting::Test:
            playBuzzer(500);
            return;
        default: return;
    }
    saveSystemSettings();
    playKeyClick();
    drawSetupPage();
}
