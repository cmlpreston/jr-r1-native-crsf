#include "channel2_ui.h"

#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include <Wire.h>

#include "config.h"

namespace {

constexpr int16_t kScreenWidth = 480;
constexpr int16_t kScreenHeight = 320;
constexpr int16_t kTrackLeft = 76;
constexpr int16_t kTrackRight = 452;
constexpr int16_t kTrackY = 294;
constexpr uint32_t kUiUpdateMs = 75;

constexpr uint16_t kBlack = RGB565_BLACK;
constexpr uint16_t kWhite = RGB565_WHITE;
constexpr uint16_t kCyan = 0x07FF;
constexpr uint16_t kGreen = 0x07E0;
constexpr uint16_t kAmber = 0xFD20;
constexpr uint16_t kRed = 0xF800;
constexpr uint16_t kPanel = 0x10A2;
constexpr uint16_t kLine = 0x03EF;
constexpr uint16_t kDim = 0x8410;

constexpr uint8_t kNsReadX = 0xC0;
constexpr uint8_t kNsReadY = 0xD0;
constexpr uint8_t kNsReadZ = 0xE0;
constexpr uint16_t kTouchPressureThreshold = 150;
constexpr uint16_t kCalibrationMovementThreshold = 12;
constexpr uint32_t kCalibrationStableMs = 1500;
constexpr int16_t kExtendedTravelPermille = 1200;
constexpr uint16_t kCrsfAbsoluteMinimum = 0;
constexpr uint16_t kCrsfAbsoluteMaximum = 2047;
constexpr uint16_t kBuzzerMinimumPitchHz = 500;
constexpr uint16_t kBuzzerMaximumPitchHz = 5000;
constexpr uint16_t kBuzzerPitchStepHz = 100;
constexpr uint16_t kBuzzerMinimumDurationMs = 10;
constexpr uint16_t kBuzzerMaximumDurationMs = 200;
constexpr uint16_t kBuzzerDurationStepMs = 10;

Arduino_DataBus *displayBus = new Arduino_ESP32SPI(
    TFT_DC_PIN, TFT_CS_PIN, TFT_SCK_PIN, TFT_MOSI_PIN, TFT_MISO_PIN);
Arduino_GFX *display = new Arduino_ILI9486_18bit(
    displayBus, GFX_NOT_DEFINED, 1 /* landscape */, false /* IPS */);

int16_t clampExtendedTravel(int32_t value)
{
    return static_cast<int16_t>(constrain(value,
                                          -kExtendedTravelPermille,
                                          kExtendedTravelPermille));
}

int16_t markerX(uint16_t crsfValue)
{
    return static_cast<int16_t>(map(constrain(crsfValue,
                                              kCrsfAbsoluteMinimum,
                                              kCrsfAbsoluteMaximum),
                                    kCrsfAbsoluteMinimum,
                                    kCrsfAbsoluteMaximum,
                                    kTrackLeft, kTrackRight));
}

void drawButton(int16_t x, int16_t y, int16_t w, int16_t h, const char *text)
{
    display->fillRoundRect(x, y, w, h, 5, kPanel);
    display->drawRoundRect(x, y, w, h, 5, kLine);
    display->setTextColor(kWhite);
    display->setTextSize(3);
    display->setCursor(x + (w - 18) / 2, y + 8);
    display->print(text);
}

void drawCentredButton(int16_t x, int16_t y, int16_t w, int16_t h,
                       const char *text, uint8_t textSize,
                       uint16_t fill = kPanel, uint16_t textColour = kWhite)
{
    display->fillRoundRect(x, y, w, h, 7, fill);
    display->drawRoundRect(x, y, w, h, 7, kLine);
    display->setTextColor(textColour);
    display->setTextSize(textSize);
    const int16_t textWidth = static_cast<int16_t>(strlen(text) * 6 * textSize);
    const int16_t textHeight = static_cast<int16_t>(8 * textSize);
    display->setCursor(x + max<int16_t>(4, (w - textWidth) / 2),
                       y + max<int16_t>(3, (h - textHeight) / 2));
    display->print(text);
}

} // namespace

bool Channel2Ui::begin()
{
    loadSettings();
    loadSystemSettings();
    // Arduino-ESP32's tone task owns this LEDC channel and stops timed tones
    // independently of slow/blocking TFT redraws.
    setToneChannel(BUZZER_LEDC_CHANNEL);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    pinMode(TOUCH_IRQ_PIN, INPUT_PULLUP);
    Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);
    Wire.setClock(400000);

    displayReady_ = display->begin(TFT_SPI_FREQUENCY);
    if (!displayReady_) {
        Serial.println("TFT35: display initialisation failed");
        return false;
    }

    display->setTextWrap(false);
    display->fillScreen(kBlack);
    drawDashboard();
    Serial.printf("TFT35: %dx%d, SPI=%lu Hz, NS2009=0x%02X\n",
                  display->width(), display->height(),
                  static_cast<unsigned long>(TFT_SPI_FREQUENCY),
                  NS2009_I2C_ADDRESS);
    playBuzzer(80);
    return true;
}

void Channel2Ui::loadSettings()
{
    Preferences preferences;
    preferences.begin("jr-r1-ch2", true);
    settings_.brakeEndpointPercent = preferences.getUChar("brake_ep", 100);
    settings_.throttleEndpointPercent = preferences.getUChar("throt_ep", 100);
    // Keep the original NVS key so existing settings migrate seamlessly.
    settings_.subtrimPercent = preferences.getChar("trim", 0);
    settings_.reversed = preferences.getBool("reverse", false);
    calibration_.minimum = preferences.getUShort("adc_min", THROTTLE_ADC_MIN);
    calibration_.centre = preferences.getUShort("adc_ctr", THROTTLE_ADC_CENTER);
    calibration_.maximum = preferences.getUShort("adc_max", THROTTLE_ADC_MAX);
    preferences.end();

    settings_.brakeEndpointPercent = constrain(settings_.brakeEndpointPercent, 50, 100);
    settings_.throttleEndpointPercent = constrain(settings_.throttleEndpointPercent, 50, 100);
    settings_.subtrimPercent = constrain(settings_.subtrimPercent, -20, 20);
    if (calibration_.minimum + 100 >= calibration_.centre ||
        calibration_.centre + 100 >= calibration_.maximum) {
        calibration_.minimum = THROTTLE_ADC_MIN;
        calibration_.centre = THROTTLE_ADC_CENTER;
        calibration_.maximum = THROTTLE_ADC_MAX;
    }
}

void Channel2Ui::saveSettings()
{
    Preferences preferences;
    preferences.begin("jr-r1-ch2", false);
    preferences.putUChar("brake_ep", settings_.brakeEndpointPercent);
    preferences.putUChar("throt_ep", settings_.throttleEndpointPercent);
    preferences.putChar("trim", settings_.subtrimPercent);
    preferences.putBool("reverse", settings_.reversed);
    preferences.end();
}

void Channel2Ui::saveCalibration()
{
    Preferences preferences;
    preferences.begin("jr-r1-ch2", false);
    preferences.putUShort("adc_min", calibration_.minimum);
    preferences.putUShort("adc_ctr", calibration_.centre);
    preferences.putUShort("adc_max", calibration_.maximum);
    preferences.end();
}

void Channel2Ui::loadSystemSettings()
{
    Preferences preferences;
    preferences.begin("jr-r1-system", true);
    systemSettings_.buzzerEnabled = preferences.getBool("buzz_en", true);
    systemSettings_.keyClicksEnabled = preferences.getBool("key_beep", true);
    systemSettings_.buzzerPitchHz = preferences.getUShort("pitch", 2400);
    systemSettings_.keyClickDurationMs = preferences.getUShort("click_ms", 40);
    preferences.end();

    systemSettings_.buzzerPitchHz = constrain(systemSettings_.buzzerPitchHz,
                                               kBuzzerMinimumPitchHz,
                                               kBuzzerMaximumPitchHz);
    systemSettings_.keyClickDurationMs = constrain(systemSettings_.keyClickDurationMs,
                                                    kBuzzerMinimumDurationMs,
                                                    kBuzzerMaximumDurationMs);
}

void Channel2Ui::saveSystemSettings()
{
    Preferences preferences;
    preferences.begin("jr-r1-system", false);
    preferences.putBool("buzz_en", systemSettings_.buzzerEnabled);
    preferences.putBool("key_beep", systemSettings_.keyClicksEnabled);
    preferences.putUShort("pitch", systemSettings_.buzzerPitchHz);
    preferences.putUShort("click_ms", systemSettings_.keyClickDurationMs);
    preferences.end();
}

void Channel2Ui::playBuzzer(uint16_t durationMs)
{
    if (!systemSettings_.buzzerEnabled) return;
    tone(BUZZER_PIN, systemSettings_.buzzerPitchHz, durationMs);
}

void Channel2Ui::playKeyClick()
{
    if (systemSettings_.keyClicksEnabled) {
        playBuzzer(systemSettings_.keyClickDurationMs);
    }
}

void Channel2Ui::stopBuzzer()
{
    noTone(BUZZER_PIN);
}

int16_t Channel2Ui::mapAdcToPermille(uint16_t adc) const
{
    adc = constrain(adc, calibration_.minimum, calibration_.maximum);
    if (adc <= calibration_.centre) {
        return static_cast<int16_t>(map(adc, calibration_.minimum,
                                        calibration_.centre, -1000, 0));
    }
    return static_cast<int16_t>(map(adc, calibration_.centre,
                                    calibration_.maximum, 0, 1000));
}

int16_t Channel2Ui::applySettings(int16_t rawInputPermille) const
{
    int32_t value = settings_.reversed ? -rawInputPermille : rawInputPermille;
    const uint8_t endpoint = value < 0 ? settings_.brakeEndpointPercent
                                       : settings_.throttleEndpointPercent;
    value = value * endpoint / 100;
    // Sub-trim is an output offset: centre and both endpoints move together.
    // Do not clamp back to +/-1000 here, or travel would be lost at one end.
    value += static_cast<int32_t>(settings_.subtrimPercent) * 10;
    return clampExtendedTravel(value);
}

void Channel2Ui::drawSettingRow(int row)
{
    static const char *labels[] = {"BRAKE END", "SUB-TRIM", "THROTTLE END", "DIRECTION"};
    const int16_t y = 56 + row * 50;
    display->fillRect(10, y, 460, 44, kBlack);
    display->drawRoundRect(10, y, 460, 44, 5, kLine);
    display->setTextColor(kCyan);
    display->setTextSize(2);
    display->setCursor(22, y + 14);
    display->print(labels[row]);

    char value[20];
    if (row == 0) {
        snprintf(value, sizeof(value), "%u%%", settings_.brakeEndpointPercent);
    } else if (row == 1) {
        snprintf(value, sizeof(value), "%+d%%", settings_.subtrimPercent);
    } else if (row == 2) {
        snprintf(value, sizeof(value), "%u%%", settings_.throttleEndpointPercent);
    } else {
        snprintf(value, sizeof(value), "%s", settings_.reversed ? "REVERSE" : "NORMAL");
    }

    display->setTextColor(row == 3 && settings_.reversed ? kAmber : kWhite);
    display->setTextSize(2);
    display->setCursor(row == 3 ? 280 : 222, y + 14);
    display->print(value);

    if (row < 3) {
        drawButton(354, y + 3, 48, 38, "-");
        drawButton(412, y + 3, 48, 38, "+");
    } else {
        display->fillRoundRect(354, y + 3, 106, 38, 5,
                               settings_.reversed ? kAmber : kPanel);
        display->drawRoundRect(354, y + 3, 106, 38, 5, kLine);
        display->setTextColor(settings_.reversed ? kBlack : kWhite);
        display->setTextSize(2);
        display->setCursor(370, y + 13);
        display->print("TOGGLE");
    }
}

void Channel2Ui::drawPage()
{
    if (!displayReady_) return;
    activePage_ = UiPage::Channel2;
    display->fillScreen(kBlack);
    display->fillRect(0, 0, kScreenWidth, 44, 0x0015);
    display->setTextColor(kWhite);
    display->setTextSize(3);
    display->setCursor(14, 10);
    display->print("CH2  THROTTLE");
    display->fillRoundRect(264, 5, 72, 34, 5, kPanel);
    display->drawRoundRect(264, 5, 72, 34, 5, kLine);
    display->setTextSize(2);
    display->setTextColor(kCyan);
    display->setCursor(276, 14);
    display->print("BACK");
    display->fillRoundRect(344, 5, 58, 34, 5, kPanel);
    display->drawRoundRect(344, 5, 58, 34, 5, kLine);
    display->setTextSize(2);
    display->setTextColor(kCyan);
    display->setCursor(356, 14);
    display->print("CAL");

    for (int row = 0; row < 4; ++row) drawSettingRow(row);

    display->setTextColor(kCyan);
    display->setTextSize(2);
    display->setCursor(12, 284);
    display->print("CH2");
    display->drawFastHLine(kTrackLeft, kTrackY, kTrackRight - kTrackLeft + 1, kDim);
    display->drawFastVLine((kTrackLeft + kTrackRight) / 2, kTrackY - 9, 19, kWhite);
    display->setTextSize(1);
    display->setTextColor(kDim);
    display->setCursor(kTrackLeft, 306);
    display->print("BRAKE");
    display->setCursor(kTrackRight - 48, 306);
    display->print("THROTTLE");
    lastMarkerX_ = -1;
    lastDisplayedCrsf_ = UINT16_MAX;
}

void Channel2Ui::drawMainMenu()
{
    if (!displayReady_) return;
    activePage_ = UiPage::MainMenu;
    display->fillScreen(kBlack);
    display->fillRect(0, 0, kScreenWidth, 44, 0x0015);
    display->setTextColor(kWhite);
    display->setTextSize(3);
    display->setCursor(82, 10);
    display->print("JR R1 CONTROLLER");
    drawCentredButton(404, 5, 66, 34, "HOME", 1);

    static const char *labels[] = {
        "MODELS", "CHANNELS",
        "RESPONSE", "MIXES",
        "DRIVE AIDS", "TIMERS",
        "LINK/TELEM", "SETUP",
    };
    for (uint8_t index = 0; index < 8; ++index) {
        const bool enabled = index == 1 || index == 7;
        const int16_t col = index % 2;
        const int16_t row = index / 2;
        drawCentredButton(10 + col * 235, 52 + row * 54, 225, 46,
                          labels[index], 2,
                          enabled ? (index == 1 ? 0x0841 : kPanel) : kBlack,
                          enabled ? (index == 1 ? kCyan : kWhite) : kDim);
    }

    display->setTextSize(1);
    display->setTextColor(kDim);
    display->setCursor(158, 285);
    display->print("DIMMED ITEMS: COMING SOON");
}

void Channel2Ui::drawDashboard()
{
    if (!displayReady_) return;
    activePage_ = UiPage::Dashboard;
    display->fillScreen(kBlack);

    // Persistent status strip.
    display->fillRect(0, 0, kScreenWidth, 40, 0x0015);
    display->setTextColor(kWhite);
    display->setTextSize(2);
    display->setCursor(12, 12);
    display->print("MODEL 01  HYPER 7");
    display->setTextColor(kGreen);
    display->setCursor(294, 12);
    display->print("CRSF TX");
    drawCentredButton(430, 4, 42, 32, "*", 2);

    // Model, power and link cards. Unknown telemetry is deliberately shown as
    // unavailable until the Ranger return stream is decoded.
    display->drawRoundRect(8, 47, 144, 87, 6, kLine);
    display->setTextColor(kCyan);
    display->setTextSize(1);
    display->setCursor(18, 57);
    display->print("ACTIVE MODEL");
    display->setTextColor(kWhite);
    display->setTextSize(3);
    display->setCursor(18, 78);
    display->print("01");
    display->setTextSize(1);
    display->setTextColor(kDim);
    display->setCursor(18, 114);
    display->print("HYPER 7");

    display->drawRoundRect(160, 47, 144, 87, 6, kLine);
    display->setTextColor(kCyan);
    display->setTextSize(1);
    display->setCursor(170, 57);
    display->print("POWER");
    display->setTextColor(kDim);
    display->setTextSize(2);
    display->setCursor(170, 78);
    display->print("TX  --.-V");
    display->setCursor(170, 105);
    display->print("RX  --.-V");

    display->drawRoundRect(312, 47, 160, 87, 6, kLine);
    display->setTextColor(kCyan);
    display->setTextSize(1);
    display->setCursor(322, 57);
    display->print("LINK / SAFETY");
    display->setTextColor(kDim);
    display->setTextSize(2);
    display->setCursor(322, 78);
    display->print("TELEM --");

    // Sixteen-channel overview. CH1 and CH2 are enabled in the initial model;
    // unassigned channels remain grey and centred.
    display->drawRoundRect(8, 141, 464, 99, 6, kLine);
    display->setTextColor(kCyan);
    display->setTextSize(1);
    display->setCursor(18, 149);
    display->print("CHANNEL MONITOR");
    for (uint8_t channel = 0; channel < 16; ++channel) {
        const int16_t x = 18 + channel * 28;
        const bool enabled = channel < 2;
        display->setTextColor(enabled ? kWhite : kDim);
        display->setCursor(x + (channel < 9 ? 5 : 2), 163);
        display->printf("%u", channel + 1);
        display->drawRect(x + 3, 177, 16, 45, enabled ? kLine : kDim);
        display->fillRect(x + 5, 198, 12, 3, enabled ? kCyan : kDim);
    }

    drawCentredButton(8, 249, 108, 62, "MENU", 2, kPanel, kWhite);
    drawCentredButton(124, 249, 108, 62, "CHANNELS", 1, 0x0841, kCyan);
    drawCentredButton(240, 249, 108, 62, "TRIM", 2, kBlack, kDim);
    drawCentredButton(356, 249, 116, 62, "TELEMETRY", 1, kBlack, kDim);

    lastDisplayedCrsf_ = UINT16_MAX;
    lastDashboardArmed_ = true; // force first status repaint
    CrsfTelemetryState emptyTelemetry;
    drawDashboardLive(992, false, emptyTelemetry);
}

void Channel2Ui::drawDashboardLive(uint16_t crsfValue, bool throttleArmed,
                                   const CrsfTelemetryState &telemetry)
{
    if (!displayReady_ || activePage_ != UiPage::Dashboard) return;

    if (crsfValue != lastDisplayedCrsf_) {
        // Restore CH2's bar interior then draw its signed live position.
        constexpr int16_t x = 18 + 28;
        display->fillRect(x + 4, 178, 14, 43, kBlack);
        display->drawRect(x + 3, 177, 16, 45, kLine);
        const int16_t y = static_cast<int16_t>(map(constrain(crsfValue, 0, 2047),
                                                   0, 2047, 219, 179));
        display->fillRect(x + 5, min<int16_t>(y, 199), 12,
                          max<int16_t>(3, abs(y - 199)), kGreen);
        display->drawFastHLine(x + 4, 199, 14, kWhite);
        lastDisplayedCrsf_ = crsfValue;
    }

    if (throttleArmed != lastDashboardArmed_) {
        display->fillRect(320, 116, 146, 14, kBlack);
        display->setTextSize(1);
        display->setTextColor(throttleArmed ? kGreen : kAmber);
        display->setCursor(322, 119);
        display->print(throttleArmed ? "OUTPUT LIVE" : "THROTTLE SAFE");
        lastDashboardArmed_ = throttleArmed;
    }

    if (telemetry.linkValid != lastLinkValid_ ||
        telemetry.uplinkLinkQuality != lastLinkQuality_ ||
        telemetry.uplinkRssiDbm != lastRssiDbm_) {
        display->fillRect(320, 74, 146, 40, kBlack);
        display->setTextSize(2);
        if (telemetry.linkValid) {
            display->setTextColor(telemetry.uplinkLinkQuality >= 80 ? kGreen : kAmber);
            display->setCursor(322, 76);
            display->printf("LQ %3u%%", telemetry.uplinkLinkQuality);
            display->setTextColor(kWhite);
            display->setCursor(322, 98);
            display->printf("RSSI %d", telemetry.uplinkRssiDbm);
        } else {
            display->setTextColor(kRed);
            display->setCursor(322, 87);
            display->print("NO LINK");
        }
        lastLinkValid_ = telemetry.linkValid;
        lastLinkQuality_ = telemetry.uplinkLinkQuality;
        lastRssiDbm_ = telemetry.uplinkRssiDbm;
    }

    if (telemetry.receiverVoltageValid != lastReceiverVoltageValid_ ||
        telemetry.receiverVoltageDecivolts != lastReceiverVoltageDecivolts_) {
        display->fillRect(168, 101, 130, 25, kBlack);
        display->setTextSize(2);
        display->setCursor(170, 105);
        if (telemetry.receiverVoltageValid) {
            display->setTextColor(kGreen);
            display->printf("RX %2u.%uV",
                            telemetry.receiverVoltageDecivolts / 10,
                            telemetry.receiverVoltageDecivolts % 10);
        } else {
            display->setTextColor(kDim);
            display->print("RX  --.-V");
        }
        lastReceiverVoltageValid_ = telemetry.receiverVoltageValid;
        lastReceiverVoltageDecivolts_ = telemetry.receiverVoltageDecivolts;
    }
}

void Channel2Ui::drawChannelsPage()
{
    if (!displayReady_) return;
    activePage_ = UiPage::Channels;
    display->fillScreen(kBlack);
    display->fillRect(0, 0, kScreenWidth, 44, 0x0015);
    display->setTextColor(kWhite);
    display->setTextSize(3);
    display->setCursor(14, 10);
    display->print("CHANNELS");
    drawCentredButton(376, 5, 94, 34, "BACK", 2);

    const uint8_t firstChannel = channelsBank_ * 8 + 1;
    for (uint8_t index = 0; index < 8; ++index) {
        const uint8_t channel = firstChannel + index;
        const int16_t col = index % 2;
        const int16_t row = index / 2;
        const int16_t x = 10 + col * 235;
        const int16_t y = 52 + row * 52;
        char label[24];
        if (channel == 1) snprintf(label, sizeof(label), "CH1  STEERING");
        else if (channel == 2) snprintf(label, sizeof(label), "CH2  THROTTLE");
        else snprintf(label, sizeof(label), "CH%u  AUX", channel);
        drawCentredButton(x, y, 225, 44, label, 2,
                          channel == 2 ? 0x0841 : kPanel,
                          channel == 2 ? kCyan : kWhite);
    }

    drawCentredButton(12, 270, 58, 40, "<", 3,
                      channelsBank_ == 0 ? kBlack : kPanel,
                      channelsBank_ == 0 ? kDim : kWhite);
    drawCentredButton(410, 270, 58, 40, ">", 3,
                      channelsBank_ == 1 ? kBlack : kPanel,
                      channelsBank_ == 1 ? kDim : kWhite);
    display->setTextColor(kDim);
    display->setTextSize(2);
    display->setCursor(211, 282);
    display->printf("%u / 2", channelsBank_ + 1);
}

void Channel2Ui::drawChannelPlaceholder()
{
    if (!displayReady_) return;
    activePage_ = UiPage::ChannelPlaceholder;
    display->fillScreen(kBlack);
    display->fillRect(0, 0, kScreenWidth, 44, 0x0015);
    display->setTextColor(kWhite);
    display->setTextSize(3);
    display->setCursor(14, 10);
    display->printf("CHANNEL %u", selectedChannel_);
    drawCentredButton(376, 5, 94, 34, "BACK", 2);

    display->setTextColor(kCyan);
    display->setTextSize(3);
    display->setCursor(102, 116);
    display->print("CONFIGURATION");
    display->setCursor(150, 158);
    display->print("COMING NEXT");
    display->setTextColor(kDim);
    display->setTextSize(2);
    display->setCursor(112, 226);
    display->printf("CRSF CHANNEL %u IS CENTRED", selectedChannel_);
}

void Channel2Ui::drawSetupRow(int row)
{
    static const char *labels[] = {"BUZZER", "KEY CLICKS", "PITCH", "CLICK LENGTH"};
    const int16_t y = 56 + row * 50;
    display->fillRect(10, y, 460, 44, kBlack);
    display->drawRoundRect(10, y, 460, 44, 5, kLine);
    display->setTextColor(kCyan);
    display->setTextSize(2);
    display->setCursor(22, y + 14);
    display->print(labels[row]);

    char value[20];
    if (row == 0) {
        snprintf(value, sizeof(value), "%s", systemSettings_.buzzerEnabled ? "ON" : "OFF");
    } else if (row == 1) {
        snprintf(value, sizeof(value), "%s", systemSettings_.keyClicksEnabled ? "ON" : "OFF");
    } else if (row == 2) {
        snprintf(value, sizeof(value), "%u Hz", systemSettings_.buzzerPitchHz);
    } else {
        snprintf(value, sizeof(value), "%u ms", systemSettings_.keyClickDurationMs);
    }

    display->setTextColor((row < 2 && value[1] == 'F') ? kDim : kWhite);
    display->setTextSize(2);
    display->setCursor(222, y + 14);
    display->print(value);

    if (row < 2) {
        const bool enabled = row == 0 ? systemSettings_.buzzerEnabled
                                      : systemSettings_.keyClicksEnabled;
        display->fillRoundRect(354, y + 3, 106, 38, 5,
                               enabled ? kGreen : kPanel);
        display->drawRoundRect(354, y + 3, 106, 38, 5, kLine);
        display->setTextColor(enabled ? kBlack : kWhite);
        display->setCursor(370, y + 13);
        display->print("TOGGLE");
    } else {
        drawButton(354, y + 3, 48, 38, "-");
        drawButton(412, y + 3, 48, 38, "+");
    }
}

void Channel2Ui::drawSetupPage()
{
    if (!displayReady_) return;
    activePage_ = UiPage::Setup;
    display->fillScreen(kBlack);
    display->fillRect(0, 0, kScreenWidth, 44, 0x0015);
    display->setTextColor(kWhite);
    display->setTextSize(3);
    display->setCursor(14, 10);
    display->print("SYSTEM SETUP");
    display->fillRoundRect(376, 5, 94, 34, 5, kPanel);
    display->drawRoundRect(376, 5, 94, 34, 5, kLine);
    display->setTextColor(kCyan);
    display->setTextSize(2);
    display->setCursor(397, 14);
    display->print("BACK");

    for (int row = 0; row < 4; ++row) drawSetupRow(row);

    display->fillRoundRect(160, 265, 160, 44, 5, kPanel);
    display->drawRoundRect(160, 265, 160, 44, 5, kLine);
    display->setTextColor(kAmber);
    display->setTextSize(2);
    display->setCursor(211, 279);
    display->print("TEST");
}

void Channel2Ui::drawCalibrationPage()
{
    if (!displayReady_) return;
    display->fillScreen(kBlack);
    display->fillRect(0, 0, kScreenWidth, 44, 0x0015);
    display->setTextColor(kWhite);
    display->setTextSize(3);
    display->setCursor(14, 10);
    display->print("THROTTLE CALIBRATION");
    display->fillRoundRect(376, 5, 94, 34, 5, kPanel);
    display->drawRoundRect(376, 5, 94, 34, 5, kLine);
    display->setTextColor(kAmber);
    display->setTextSize(2);
    display->setCursor(385, 14);
    display->print("CANCEL");

    display->setTextColor(kCyan);
    display->setTextSize(2);
    display->setCursor(20, 62);
    display->print("1. MOVE TO FULL BRAKE");
    display->setCursor(20, 92);
    display->print("2. MOVE TO FULL THROTTLE");
    display->setCursor(20, 122);
    display->print("3. RELEASE AND HOLD CENTRE");
    display->setTextColor(kAmber);
    display->setCursor(20, 160);
    display->print("CH2 OUTPUT HELD AT NEUTRAL");

    display->drawRoundRect(18, 202, 444, 92, 5, kLine);
    display->setTextSize(2);
    display->setTextColor(kDim);
    display->setCursor(34, 216);
    display->print("MIN AVG");
    display->setCursor(200, 216);
    display->print("LIVE");
    display->setCursor(365, 216);
    display->print("MAX AVG");
    drawCalibrationValues(latestRawAdc_);
}

void Channel2Ui::drawCalibrationValues(uint16_t adc)
{
    if (!displayReady_ || !calibrationActive_) return;
    display->fillRect(28, 242, 424, 43, kBlack);
    display->setTextSize(3);
    display->setTextColor(calibrationSawMinimum_ ? kGreen : kWhite);
    display->setCursor(28, 250);
    display->printf("%4u", calibrationMinimum_);
    display->setTextColor(kWhite);
    display->setCursor(190, 250);
    display->printf("%4u", adc);
    display->setTextColor(calibrationSawMaximum_ ? kGreen : kWhite);
    display->setCursor(352, 250);
    display->printf("%4u", calibrationMaximum_);
}

void Channel2Ui::startCalibration(uint16_t adc)
{
    activePage_ = UiPage::Calibration;
    calibrationActive_ = true;
    calibrationSawMinimum_ = false;
    calibrationSawMaximum_ = false;
    calibrationMinimum_ = adc;
    calibrationMaximum_ = adc;
    calibrationStableAdc_ = adc;
    calibrationStableSinceMs_ = 0;
    drawCalibrationPage();
    Serial.println("CAL: move trigger to both ends, then release and hold neutral");
}

void Channel2Ui::cancelCalibration()
{
    calibrationActive_ = false;
    drawPage();
    Serial.println("CAL: cancelled; previous calibration retained");
}

void Channel2Ui::updateCalibration(uint16_t adc)
{
    calibrationMinimum_ = min(calibrationMinimum_, adc);
    calibrationMaximum_ = max(calibrationMaximum_, adc);

    const uint16_t lowThreshold = calibration_.centre -
        max<uint16_t>(100, (calibration_.centre - calibration_.minimum) / 3);
    const uint16_t highThreshold = calibration_.centre +
        max<uint16_t>(100, (calibration_.maximum - calibration_.centre) / 3);
    calibrationSawMinimum_ |= adc <= lowThreshold;
    calibrationSawMaximum_ |= adc >= highThreshold;

    if (!calibrationSawMinimum_ || !calibrationSawMaximum_) {
        calibrationStableSinceMs_ = 0;
        return;
    }

    // Only consider a stable point away from either captured endpoint. This
    // prevents a held full-brake/full-throttle position becoming neutral.
    const uint16_t span = calibrationMaximum_ - calibrationMinimum_;
    const uint16_t centreLow = calibrationMinimum_ + span / 8;
    const uint16_t centreHigh = calibrationMaximum_ - span / 8;
    if (adc <= centreLow || adc >= centreHigh) {
        calibrationStableSinceMs_ = 0;
        calibrationStableAdc_ = adc;
        return;
    }

    if (abs(static_cast<int>(adc) - static_cast<int>(calibrationStableAdc_)) >
        kCalibrationMovementThreshold) {
        calibrationStableAdc_ = adc;
        calibrationStableSinceMs_ = millis();
        return;
    }
    if (calibrationStableSinceMs_ == 0) calibrationStableSinceMs_ = millis();
    if (millis() - calibrationStableSinceMs_ < kCalibrationStableMs) return;

    const uint16_t centre = adc;
    if (calibrationMinimum_ + 100 >= centre || centre + 100 >= calibrationMaximum_) {
        calibrationStableSinceMs_ = 0;
        return;
    }

    calibration_.minimum = calibrationMinimum_;
    calibration_.centre = centre;
    calibration_.maximum = calibrationMaximum_;
    saveCalibration();
    calibrationActive_ = false;
    Serial.printf("CAL: saved min=%u centre=%u max=%u\n",
                  calibration_.minimum, calibration_.centre, calibration_.maximum);
    playBuzzer(180);
    drawPage();
}

void Channel2Ui::drawLiveIndicator(int16_t outputPermille, uint16_t crsfValue)
{
    if (!displayReady_) return;

    (void)outputPermille;
    const int16_t x = markerX(crsfValue);
    if (lastMarkerX_ >= 0 && lastMarkerX_ != x) {
        display->fillRect(lastMarkerX_ - 7, kTrackY - 21, 15, 32, kBlack);
        display->drawFastHLine(lastMarkerX_ - 7, kTrackY, 15, kDim);
        const int16_t centre = (kTrackLeft + kTrackRight) / 2;
        if (centre >= lastMarkerX_ - 4 && centre <= lastMarkerX_ + 4) {
            display->drawFastVLine(centre, kTrackY - 9, 19, kWhite);
        }
    }
    display->fillTriangle(x, kTrackY - 10, x - 6, kTrackY - 20,
                          x + 6, kTrackY - 20, kGreen);
    display->drawFastVLine(x, kTrackY - 9, 19, kGreen);
    lastMarkerX_ = x;

    if (crsfValue != lastDisplayedCrsf_) {
        display->fillRect(405, 8, 70, 28, 0x0015);
        display->setTextColor(kGreen);
        display->setTextSize(2);
        display->setCursor(411, 14);
        display->printf("%4u", crsfValue);
        lastDisplayedCrsf_ = crsfValue;
    }
}

uint16_t Channel2Ui::readTouchRegister(uint8_t command)
{
    Wire.beginTransmission(NS2009_I2C_ADDRESS);
    Wire.write(command);
    if (Wire.endTransmission() != 0) return 0;
    if (Wire.requestFrom(NS2009_I2C_ADDRESS, static_cast<uint8_t>(2)) != 2) return 0;
    const uint8_t high = Wire.read();
    const uint8_t low = Wire.read();
    return static_cast<uint16_t>((high << 4) | (low >> 4));
}

bool Channel2Ui::readTouch(int16_t &x, int16_t &y, uint16_t &rawX,
                           uint16_t &rawY, uint16_t &rawZ)
{
    if (digitalRead(TOUCH_IRQ_PIN) != LOW) return false;
    rawZ = readTouchRegister(kNsReadZ);
    if (rawZ <= kTouchPressureThreshold || rawZ == 0x0FFF) return false;
    rawX = readTouchRegister(kNsReadX);
    rawY = readTouchRegister(kNsReadY);

    // Native panel coordinates are portrait. Rotation 1 makes the LCD
    // landscape, so Y becomes screen X and X becomes the inverted screen Y.
    x = constrain(map(rawY, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX, 0,
                      kScreenWidth - 1), 0, kScreenWidth - 1);
    y = constrain(map(rawX, TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX,
                      kScreenHeight - 1, 0), 0, kScreenHeight - 1);
    return true;
}

void Channel2Ui::handleTouch(int16_t x, int16_t y)
{
    if (calibrationActive_) {
        if (y < 48 && x >= 360) cancelCalibration();
        return;
    }
    if (activePage_ == UiPage::Dashboard) {
        handleDashboardTouch(x, y);
        return;
    }
    if (activePage_ == UiPage::MainMenu) {
        handleMainMenuTouch(x, y);
        return;
    }
    if (activePage_ == UiPage::Channels) {
        handleChannelsTouch(x, y);
        return;
    }
    if (activePage_ == UiPage::ChannelPlaceholder) {
        if (y < 48 && x >= 360) {
            playKeyClick();
            drawChannelsPage();
        }
        return;
    }
    if (activePage_ == UiPage::Setup) {
        handleSetupTouch(x, y);
        return;
    }
    if (y < 48 && x >= 250 && x < 340) {
        playKeyClick();
        drawChannelsPage();
        return;
    }
    if (y < 48 && x >= 340) {
        playKeyClick();
        startCalibration(latestRawAdc_);
        return;
    }
    if (y < 56 || y >= 256) return;
    const int row = (y - 56) / 50;
    bool changed = false;

    if (row < 3 && x >= 354 && x < 404) {
        if (row == 0 && settings_.brakeEndpointPercent > 50) --settings_.brakeEndpointPercent;
        if (row == 1 && settings_.subtrimPercent > -20) --settings_.subtrimPercent;
        if (row == 2 && settings_.throttleEndpointPercent > 50) --settings_.throttleEndpointPercent;
        changed = true;
    } else if (row < 3 && x >= 408 && x < 470) {
        if (row == 0 && settings_.brakeEndpointPercent < 100) ++settings_.brakeEndpointPercent;
        if (row == 1 && settings_.subtrimPercent < 20) ++settings_.subtrimPercent;
        if (row == 2 && settings_.throttleEndpointPercent < 100) ++settings_.throttleEndpointPercent;
        changed = true;
    } else if (row == 3 && x >= 340) {
        settings_.reversed = !settings_.reversed;
        changed = true;
    }

    if (changed) {
        playKeyClick();
        saveSettings();
        drawSettingRow(row);
        Serial.printf("CH2 settings: brake=%u%% subtrim=%+d%% throttle=%u%% direction=%s\n",
                      settings_.brakeEndpointPercent, settings_.subtrimPercent,
                      settings_.throttleEndpointPercent,
                      settings_.reversed ? "reverse" : "normal");
    }
}

void Channel2Ui::handleDashboardTouch(int16_t x, int16_t y)
{
    if ((y < 45 && x >= 415) || (y >= 245 && x < 120)) {
        playKeyClick();
        drawMainMenu();
        return;
    }
    if (y >= 245 && x >= 120 && x < 236) {
        playKeyClick();
        drawChannelsPage();
    }
}

void Channel2Ui::handleMainMenuTouch(int16_t x, int16_t y)
{
    if (y < 48 && x >= 390) {
        playKeyClick();
        drawDashboard();
        return;
    }
    if (x < 5 || x >= 475 || y < 52 || y >= 270) return;
    const int row = (y - 52) / 54;
    if (row < 0 || row > 3) return;
    const int col = x >= 240 ? 1 : 0;
    const int item = row * 2 + col;

    if (item == 1) {
        playKeyClick();
        drawChannelsPage();
    } else if (item == 7) {
        playKeyClick();
        drawSetupPage();
    } else {
        playKeyClick();
        static const char *labels[] = {
            "MODELS", "CHANNELS", "RESPONSE", "MIXES",
            "DRIVE AIDS", "TIMERS", "LINK/TELEM", "SETUP",
        };
        display->fillRect(0, 277, kScreenWidth, 36, kBlack);
        display->setTextColor(kAmber);
        display->setTextSize(1);
        const int16_t textWidth = strlen(labels[item]) * 6;
        display->setCursor((kScreenWidth - textWidth - 78) / 2, 288);
        display->printf("%s: COMING SOON", labels[item]);
    }
}

void Channel2Ui::handleChannelsTouch(int16_t x, int16_t y)
{
    if (y < 48 && x >= 360) {
        playKeyClick();
        drawMainMenu();
        return;
    }
    if (y >= 270) {
        if (x < 90 && channelsBank_ > 0) {
            --channelsBank_;
            playKeyClick();
            drawChannelsPage();
        } else if (x >= 390 && channelsBank_ < 1) {
            ++channelsBank_;
            playKeyClick();
            drawChannelsPage();
        }
        return;
    }
    if (y < 52 || y >= 264) return;
    const int row = (y - 52) / 52;
    if (row < 0 || row > 3) return;
    const int col = x >= 240 ? 1 : 0;
    const uint8_t channel = channelsBank_ * 8 + row * 2 + col + 1;
    selectedChannel_ = channel;
    playKeyClick();
    if (channel == 2) drawPage();
    else drawChannelPlaceholder();
}

void Channel2Ui::handleSetupTouch(int16_t x, int16_t y)
{
    if (y < 48 && x >= 360) {
        playKeyClick();
        drawMainMenu();
        return;
    }
    if (y >= 265 && y < 315 && x >= 145 && x < 335) {
        playBuzzer(500);
        return;
    }
    if (y < 56 || y >= 256) return;

    const int row = (y - 56) / 50;
    bool changed = false;
    if (row < 2 && x >= 340) {
        if (row == 0) {
            systemSettings_.buzzerEnabled = !systemSettings_.buzzerEnabled;
            if (!systemSettings_.buzzerEnabled) stopBuzzer();
        } else {
            systemSettings_.keyClicksEnabled = !systemSettings_.keyClicksEnabled;
        }
        changed = true;
    } else if (row == 2 && x >= 354 && x < 404 &&
               systemSettings_.buzzerPitchHz > kBuzzerMinimumPitchHz) {
        systemSettings_.buzzerPitchHz -= kBuzzerPitchStepHz;
        changed = true;
    } else if (row == 2 && x >= 408 && x < 470 &&
               systemSettings_.buzzerPitchHz < kBuzzerMaximumPitchHz) {
        systemSettings_.buzzerPitchHz += kBuzzerPitchStepHz;
        changed = true;
    } else if (row == 3 && x >= 354 && x < 404 &&
               systemSettings_.keyClickDurationMs > kBuzzerMinimumDurationMs) {
        systemSettings_.keyClickDurationMs -= kBuzzerDurationStepMs;
        changed = true;
    } else if (row == 3 && x >= 408 && x < 470 &&
               systemSettings_.keyClickDurationMs < kBuzzerMaximumDurationMs) {
        systemSettings_.keyClickDurationMs += kBuzzerDurationStepMs;
        changed = true;
    }

    if (changed) {
        saveSystemSettings();
        drawSetupRow(row);
        playKeyClick();
        Serial.printf("SYSTEM: buzzer=%s key-clicks=%s pitch=%uHz length=%ums\n",
                      systemSettings_.buzzerEnabled ? "on" : "off",
                      systemSettings_.keyClicksEnabled ? "on" : "off",
                      systemSettings_.buzzerPitchHz,
                      systemSettings_.keyClickDurationMs);
    }
}

void Channel2Ui::update(uint16_t rawAdc, int16_t rawInputPermille,
                        int16_t outputPermille, uint16_t crsfValue,
                        bool throttleArmed, const CrsfTelemetryState &telemetry)
{
    latestRawAdc_ = rawAdc;
    (void)rawInputPermille;
    int16_t x = 0;
    int16_t y = 0;
    uint16_t rawX = 0;
    uint16_t rawY = 0;
    uint16_t rawZ = 0;
    const bool touched = readTouch(x, y, rawX, rawY, rawZ);
    if (touched) {
        lastTouchX_ = x;
        lastTouchY_ = y;
        if (!touchWasDown_) {
            Serial.printf("TOUCH raw=(%u,%u,%u) screen=(%d,%d)\n",
                          rawX, rawY, rawZ, x, y);
        }
    } else if (touchWasDown_) {
        handleTouch(lastTouchX_, lastTouchY_);
    }
    touchWasDown_ = touched;

    if (calibrationActive_) updateCalibration(rawAdc);

    const uint32_t now = millis();
    if (static_cast<int32_t>(now - nextDisplayUpdateMs_) >= 0) {
        nextDisplayUpdateMs_ = now + kUiUpdateMs;
        if (calibrationActive_) drawCalibrationValues(rawAdc);
        else if (activePage_ == UiPage::Dashboard) {
            drawDashboardLive(crsfValue, throttleArmed, telemetry);
        }
        else if (activePage_ == UiPage::Channel2) {
            drawLiveIndicator(outputPermille, crsfValue);
        }
    }
}
