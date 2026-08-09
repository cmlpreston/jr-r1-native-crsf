#include "channel2_ui.h"

#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include <Wire.h>

#include "config.h"

LV_IMG_DECLARE(hyper7_icon);

namespace {

constexpr int16_t kScreenWidth = 480;
constexpr int16_t kScreenHeight = 320;
constexpr uint16_t kCrsfMid = 992;
constexpr uint16_t kCrsfMaximum = 2047;
constexpr int16_t kExtendedTravelPermille = 1200;
constexpr uint32_t kUiUpdateMs = 80;
constexpr uint16_t kTouchPressureThreshold = 40;
constexpr uint8_t kNsReadX = 0xC0;
constexpr uint8_t kNsReadY = 0xD0;
constexpr uint8_t kNsReadZ = 0xE0;
constexpr uint32_t kCalibrationStableMs = 1500;
constexpr uint16_t kCalibrationMovementThreshold = 16;
constexpr uint16_t kBuzzerMinimumPitchHz = 500;
constexpr uint16_t kBuzzerMaximumPitchHz = 5000;
constexpr uint16_t kBuzzerPitchStepHz = 100;
constexpr uint16_t kBuzzerMinimumDurationMs = 10;
constexpr uint16_t kBuzzerMaximumDurationMs = 200;
constexpr uint16_t kBuzzerDurationStepMs = 10;
constexpr uint16_t kTxBatteryMinimumAlarmDecivolts = 30;
constexpr uint16_t kTxBatteryMaximumAlarmDecivolts = 168;
constexpr uint32_t kLowBatteryRepeatMs = 10000;

constexpr uint32_t cBlack = 0x05080d;
constexpr uint32_t cHeader = 0x06182c;
constexpr uint32_t cPanel = 0x0b121c;
constexpr uint32_t cPanel2 = 0x132238;
constexpr uint32_t cLine = 0x175184;
constexpr uint32_t cCyan = 0x19d9f4;
constexpr uint32_t cWhite = 0xf1f4f7;
constexpr uint32_t cDim = 0x526477;
constexpr uint32_t cGreen = 0x67e33d;
constexpr uint32_t cAmber = 0xffa51f;
constexpr uint32_t cRed = 0xff4b43;

Arduino_DataBus *displayBus = new Arduino_ESP32SPI(
    TFT_DC_PIN, TFT_CS_PIN, TFT_SCK_PIN, TFT_MOSI_PIN, TFT_MISO_PIN);
Arduino_GFX *display = new Arduino_ILI9486_18bit(
    displayBus, GFX_NOT_DEFINED, 1, false);

lv_disp_draw_buf_t drawBuffer;
lv_disp_drv_t displayDriver;
lv_color_t lineBuffer[kScreenWidth * 16];

lv_color_t colour(uint32_t rgb)
{
    return lv_color_hex(rgb);
}

const lv_font_t *fontFor(uint8_t size)
{
    switch (size) {
        case 32: return &lv_font_montserrat_32;
        case 28: return &lv_font_montserrat_28;
        case 24: return &lv_font_montserrat_24;
        case 22: return &lv_font_montserrat_22;
        case 20: return &lv_font_montserrat_20;
        case 18: return &lv_font_montserrat_18;
        case 16: return &lv_font_montserrat_16;
        case 12: return &lv_font_montserrat_12;
        default: return &lv_font_montserrat_14;
    }
}

void flushDisplay(lv_disp_drv_t *driver, const lv_area_t *area,
                  lv_color_t *pixels)
{
    const int16_t width = area->x2 - area->x1 + 1;
    const int16_t height = area->y2 - area->y1 + 1;
    display->draw16bitRGBBitmap(area->x1, area->y1,
                                reinterpret_cast<uint16_t *>(pixels),
                                width, height);
    lv_disp_flush_ready(driver);
}

void position(lv_obj_t *object, int16_t x, int16_t y, int16_t width,
              int16_t height)
{
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *makePanel(int16_t x, int16_t y, int16_t width, int16_t height,
                    uint32_t fill = cPanel, uint32_t border = cLine,
                    uint8_t radius = 7)
{
    lv_obj_t *panel = lv_obj_create(lv_scr_act());
    position(panel, x, y, width, height);
    lv_obj_set_style_bg_color(panel, colour(fill), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, colour(border), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, radius, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    return panel;
}

lv_obj_t *makeLabel(lv_obj_t *parent, const char *text, int16_t x, int16_t y,
                    uint8_t fontSize = 14, uint32_t textColour = cWhite)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_font(label, fontFor(fontSize), 0);
    lv_obj_set_style_text_color(label, colour(textColour), 0);
    return label;
}

lv_obj_t *makeButton(int16_t x, int16_t y, int16_t width, int16_t height,
                     const char *text, uint8_t fontSize = 16,
                     uint32_t fill = cPanel2, uint32_t textColour = cWhite,
                     uint32_t border = cLine)
{
    lv_obj_t *button = makePanel(x, y, width, height, fill, border, 7);
    lv_obj_t *label = makeLabel(button, text, 0, 0, fontSize, textColour);
    lv_obj_center(label);
    return button;
}

void startScreen()
{
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), colour(cBlack), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
}

void makeHeader(const char *title, const char *action = nullptr)
{
    lv_obj_t *header = makePanel(0, 0, kScreenWidth, 44, cHeader, cHeader, 0);
    makeLabel(header, title, 14, 8, 24);
    if (action != nullptr) makeButton(382, 5, 88, 34, action, 14);
}

lv_obj_t *makeBar(lv_obj_t *parent, int16_t x, int16_t y, int16_t width,
                  int16_t height, int16_t minimum, int16_t maximum,
                  uint32_t indicator = cCyan)
{
    lv_obj_t *bar = lv_bar_create(parent);
    position(bar, x, y, width, height);
    lv_bar_set_range(bar, minimum, maximum);
    lv_obj_set_style_bg_color(bar, colour(cPanel2), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(bar, colour(cLine), LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, colour(indicator), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 2, LV_PART_INDICATOR);
    return bar;
}

int16_t clampExtendedTravel(int32_t value)
{
    return static_cast<int16_t>(constrain(value, -kExtendedTravelPermille,
                                          kExtendedTravelPermille));
}

const char *batteryChemistryName(BatteryChemistry chemistry)
{
    switch (chemistry) {
        case BatteryChemistry::LiFe: return "LiFe";
        case BatteryChemistry::LiPo: return "LiPo";
        case BatteryChemistry::NiMH: return "NiMH";
    }
    return "?";
}

uint8_t maximumCellsFor(BatteryChemistry chemistry)
{
    return chemistry == BatteryChemistry::NiMH ? 10 : 4;
}

uint16_t defaultLowAlarmDecivolts(BatteryChemistry chemistry,
                                  uint8_t cells)
{
    uint8_t perCellDecivolts = 30; // 3.0 V/cell for LiFe
    if (chemistry == BatteryChemistry::LiPo) perCellDecivolts = 35;
    else if (chemistry == BatteryChemistry::NiMH) perCellDecivolts = 10;
    return static_cast<uint16_t>(perCellDecivolts) * cells;
}

} // namespace

bool Channel2Ui::begin()
{
    loadSettings();
    loadSystemSettings();
    loadTxBatterySettings();
    setToneChannel(BUZZER_LEDC_CHANNEL);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    pinMode(TOUCH_IRQ_PIN, INPUT_PULLUP);
    Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);
    Wire.setClock(400000);

    displayReady_ = display->begin(TFT_SPI_FREQUENCY);
    if (!displayReady_) {
        Serial.println("TFT35/LVGL: display initialisation failed");
        return false;
    }

    lv_init();
    lv_disp_draw_buf_init(&drawBuffer, lineBuffer, nullptr,
                          kScreenWidth * 16);
    lv_disp_drv_init(&displayDriver);
    displayDriver.hor_res = kScreenWidth;
    displayDriver.ver_res = kScreenHeight;
    displayDriver.flush_cb = flushDisplay;
    displayDriver.draw_buf = &drawBuffer;
    lv_disp_drv_register(&displayDriver);

    drawDashboard();
    lv_refr_now(nullptr);
    Serial.printf("TFT35/LVGL: %dx%d, SPI=%lu Hz, buffer=%u rows\n",
                  kScreenWidth, kScreenHeight,
                  static_cast<unsigned long>(TFT_SPI_FREQUENCY), 16U);
    playBuzzer(80);
    return true;
}

void Channel2Ui::resetLvglObjects()
{
    dashboardThrottleBar_ = nullptr;
    dashboardSafetyLabel_ = nullptr;
    dashboardLinkLabel_ = nullptr;
    dashboardRssiLabel_ = nullptr;
    dashboardRxVoltageLabel_ = nullptr;
    dashboardTxVoltageLabel_ = nullptr;
    channel2LiveBar_ = nullptr;
    channel2ValueLabel_ = nullptr;
    calibrationMinimumLabel_ = nullptr;
    calibrationLiveLabel_ = nullptr;
    calibrationMaximumLabel_ = nullptr;
}

void Channel2Ui::loadSettings()
{
    Preferences preferences;
    preferences.begin("jr-r1-ch2", true);
    settings_.brakeEndpointPercent = preferences.getUChar("brake_ep", 100);
    settings_.throttleEndpointPercent = preferences.getUChar("throt_ep", 100);
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
    systemSettings_.buzzerPitchHz = preferences.getUShort("pitch", 3200);
    systemSettings_.keyClickDurationMs = preferences.getUShort("click_ms", 10);
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

void Channel2Ui::loadTxBatterySettings()
{
    Preferences preferences;
    preferences.begin("jr-r1-txbat", true);
    txBatterySettings_.chemistry = static_cast<BatteryChemistry>(
        preferences.getUChar("chem", static_cast<uint8_t>(BatteryChemistry::LiFe)));
    txBatterySettings_.cellCount = preferences.getUChar("cells", 3);
    txBatterySettings_.lowAlarmDecivolts = preferences.getUShort("low_dv", 90);
    txBatterySettings_.voltageSenseEnabled = preferences.getBool("sense_en", false);
    preferences.end();

    if (static_cast<uint8_t>(txBatterySettings_.chemistry) >
        static_cast<uint8_t>(BatteryChemistry::NiMH)) {
        txBatterySettings_.chemistry = BatteryChemistry::LiFe;
    }
    txBatterySettings_.cellCount = constrain(txBatterySettings_.cellCount, 1, 12);
    txBatterySettings_.lowAlarmDecivolts = constrain(
        txBatterySettings_.lowAlarmDecivolts,
        kTxBatteryMinimumAlarmDecivolts, kTxBatteryMaximumAlarmDecivolts);
}

void Channel2Ui::saveTxBatterySettings()
{
    Preferences preferences;
    preferences.begin("jr-r1-txbat", false);
    preferences.putUChar("chem", static_cast<uint8_t>(txBatterySettings_.chemistry));
    preferences.putUChar("cells", txBatterySettings_.cellCount);
    preferences.putUShort("low_dv", txBatterySettings_.lowAlarmDecivolts);
    preferences.putBool("sense_en", txBatterySettings_.voltageSenseEnabled);
    preferences.end();
}

void Channel2Ui::playBuzzer(uint16_t durationMs)
{
    if (systemSettings_.buzzerEnabled) tone(BUZZER_PIN, systemSettings_.buzzerPitchHz, durationMs);
}

void Channel2Ui::playKeyClick()
{
    if (systemSettings_.keyClicksEnabled) playBuzzer(systemSettings_.keyClickDurationMs);
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
    value = value * (value < 0 ? settings_.brakeEndpointPercent
                               : settings_.throttleEndpointPercent) / 100;
    value += static_cast<int32_t>(settings_.subtrimPercent) * 10;
    return clampExtendedTravel(value);
}

void Channel2Ui::drawDashboard()
{
    if (!displayReady_) return;
    activePage_ = UiPage::Dashboard;
    startScreen();
    resetLvglObjects();

    lv_obj_t *header = makePanel(0, 0, 480, 40, cHeader, cHeader, 0);
    makeLabel(header, "MODEL 01", 12, 8, 20);
    makeLabel(header, "HYPER 7", 120, 10, 16, cCyan);
    makeLabel(header, "CRSF TX", 320, 10, 16, cGreen);
    makeButton(433, 4, 39, 32, LV_SYMBOL_SETTINGS, 18);

    lv_obj_t *model = makePanel(8, 47, 144, 87);
    makeLabel(model, "ACTIVE MODEL", 10, 8, 12, cCyan);
    makeLabel(model, "01", 10, 28, 28);
    lv_obj_t *buggy = lv_img_create(model);
    lv_img_set_src(buggy, &hyper7_icon);
    lv_obj_set_pos(buggy, 55, 18);
    makeLabel(model, "HYPER 7", 10, 68, 12, cDim);

    lv_obj_t *power = makePanel(160, 47, 144, 87);
    makeLabel(power, "POWER", 10, 8, 12, cCyan);
    dashboardTxVoltageLabel_ = makeLabel(power, "TX  --.-V", 10, 31, 16, cDim);
    dashboardRxVoltageLabel_ = makeLabel(power, "RX  --.-V", 10, 57, 16, cDim);

    lv_obj_t *link = makePanel(312, 47, 160, 87);
    makeLabel(link, "LINK / SAFETY", 10, 8, 12, cCyan);
    dashboardLinkLabel_ = makeLabel(link, "NO LINK", 10, 29, 18, cRed);
    dashboardRssiLabel_ = makeLabel(link, "", 10, 52, 14, cWhite);
    dashboardSafetyLabel_ = makeLabel(link, "THROTTLE SAFE", 10, 69, 12, cAmber);

    lv_obj_t *monitor = makePanel(8, 141, 464, 99);
    makeLabel(monitor, "CHANNEL MONITOR", 10, 6, 12, cCyan);
    for (uint8_t channel = 0; channel < 16; ++channel) {
        const int16_t x = 9 + channel * 28;
        char number[4];
        snprintf(number, sizeof(number), "%u", channel + 1);
        makeLabel(monitor, number, x + (channel < 9 ? 5 : 1), 21, 12,
                  channel < 2 ? cWhite : cDim);
        lv_obj_t *bar = makeBar(monitor, x + 3, 37, 16, 44, 0, kCrsfMaximum,
                                channel < 2 ? cCyan : cDim);
        lv_bar_set_value(bar, kCrsfMid, LV_ANIM_OFF);
        if (channel == 1) dashboardThrottleBar_ = bar;
    }

    makeButton(8, 249, 108, 62, "MENU", 18);
    makeButton(124, 249, 108, 62, "CHANNELS", 12, cPanel2, cCyan);
    makeButton(240, 249, 108, 62, "TRIM", 18, cBlack, cDim, cDim);
    makeButton(356, 249, 116, 62, "TELEMETRY", 12, cBlack, cDim, cDim);

    lastDisplayedCrsf_ = UINT16_MAX;
    lastDashboardArmed_ = true;
    lastLinkValid_ = !lastLinkValid_;
    lastReceiverVoltageValid_ = !lastReceiverVoltageValid_;
    lastTxVoltageValid_ = !lastTxVoltageValid_;
}

void Channel2Ui::drawDashboardLive(uint16_t crsfValue, bool throttleArmed,
                                   const CrsfTelemetryState &telemetry,
                                   uint16_t txVoltageDecivolts,
                                   bool txVoltageValid)
{
    if (!displayReady_ || activePage_ != UiPage::Dashboard) return;
    if (dashboardThrottleBar_ && crsfValue != lastDisplayedCrsf_) {
        lv_bar_set_value(dashboardThrottleBar_, crsfValue, LV_ANIM_OFF);
        lastDisplayedCrsf_ = crsfValue;
    }
    if (dashboardSafetyLabel_ && throttleArmed != lastDashboardArmed_) {
        lv_label_set_text(dashboardSafetyLabel_, throttleArmed ? "OUTPUT LIVE" : "THROTTLE SAFE");
        lv_obj_set_style_text_color(dashboardSafetyLabel_,
                                    colour(throttleArmed ? cGreen : cAmber), 0);
        lastDashboardArmed_ = throttleArmed;
    }
    if (dashboardLinkLabel_ &&
        (telemetry.linkValid != lastLinkValid_ ||
         telemetry.uplinkLinkQuality != lastLinkQuality_ ||
         telemetry.uplinkRssiDbm != lastRssiDbm_)) {
        if (telemetry.linkValid) {
            lv_label_set_text_fmt(dashboardLinkLabel_, "LQ %3u%%", telemetry.uplinkLinkQuality);
            lv_obj_set_style_text_color(dashboardLinkLabel_,
                colour(telemetry.uplinkLinkQuality >= 80 ? cGreen : cAmber), 0);
            lv_label_set_text_fmt(dashboardRssiLabel_, "RSSI %d dBm", telemetry.uplinkRssiDbm);
        } else {
            lv_label_set_text(dashboardLinkLabel_, "NO LINK");
            lv_label_set_text(dashboardRssiLabel_, "");
            lv_obj_set_style_text_color(dashboardLinkLabel_, colour(cRed), 0);
        }
        lastLinkValid_ = telemetry.linkValid;
        lastLinkQuality_ = telemetry.uplinkLinkQuality;
        lastRssiDbm_ = telemetry.uplinkRssiDbm;
    }
    if (dashboardRxVoltageLabel_ &&
        (telemetry.receiverVoltageValid != lastReceiverVoltageValid_ ||
         telemetry.receiverVoltageDecivolts != lastReceiverVoltageDecivolts_)) {
        if (telemetry.receiverVoltageValid) {
            lv_label_set_text_fmt(dashboardRxVoltageLabel_, "RX  %u.%uV",
                telemetry.receiverVoltageDecivolts / 10,
                telemetry.receiverVoltageDecivolts % 10);
            lv_obj_set_style_text_color(dashboardRxVoltageLabel_, colour(cGreen), 0);
        } else {
            lv_label_set_text(dashboardRxVoltageLabel_, "RX  --.-V");
            lv_obj_set_style_text_color(dashboardRxVoltageLabel_, colour(cDim), 0);
        }
        lastReceiverVoltageValid_ = telemetry.receiverVoltageValid;
        lastReceiverVoltageDecivolts_ = telemetry.receiverVoltageDecivolts;
    }
    if (dashboardTxVoltageLabel_ &&
        (txVoltageValid != lastTxVoltageValid_ ||
         txVoltageDecivolts != lastTxVoltageDecivolts_)) {
        if (txVoltageValid) {
            const bool low = txVoltageDecivolts <= txBatterySettings_.lowAlarmDecivolts;
            lv_label_set_text_fmt(dashboardTxVoltageLabel_, "TX  %u.%uV",
                txVoltageDecivolts / 10, txVoltageDecivolts % 10);
            lv_obj_set_style_text_color(dashboardTxVoltageLabel_,
                                        colour(low ? cRed : cGreen), 0);
        } else {
            lv_label_set_text(dashboardTxVoltageLabel_, "TX  --.-V");
            lv_obj_set_style_text_color(dashboardTxVoltageLabel_, colour(cDim), 0);
        }
        lastTxVoltageValid_ = txVoltageValid;
        lastTxVoltageDecivolts_ = txVoltageDecivolts;
    }

    const uint32_t now = millis();
    if (txVoltageValid &&
        txVoltageDecivolts <= txBatterySettings_.lowAlarmDecivolts) {
        if (static_cast<int32_t>(now - nextLowBatteryAlarmMs_) >= 0) {
            playBuzzer(300);
            nextLowBatteryAlarmMs_ = now + kLowBatteryRepeatMs;
        }
    } else {
        nextLowBatteryAlarmMs_ = now;
    }
}

void Channel2Ui::drawMainMenu()
{
    activePage_ = UiPage::MainMenu;
    startScreen();
    resetLvglObjects();
    makeHeader("JR R1 CONTROLLER", "HOME");
    static const char *labels[] = {"MODELS", "CHANNELS", "RESPONSE", "MIXES",
        "DRIVE AIDS", "TIMERS", "LINK / TELEM", "SETUP"};
    for (uint8_t index = 0; index < 8; ++index) {
        const bool enabled = index == 1 || index == 7;
        makeButton(10 + (index % 2) * 235, 52 + (index / 2) * 54, 225, 46,
                   labels[index], 16,
                   enabled ? cPanel2 : cBlack,
                   enabled ? (index == 1 ? cCyan : cWhite) : cDim,
                   enabled ? cLine : cDim);
    }
    lv_obj_t *note = makeLabel(lv_scr_act(), "DIMMED ITEMS: COMING SOON", 0, 284, 12, cDim);
    lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 284);
}

void Channel2Ui::drawChannelsPage()
{
    activePage_ = UiPage::Channels;
    startScreen();
    resetLvglObjects();
    makeHeader("CHANNELS", "BACK");
    const uint8_t first = channelsBank_ * 8 + 1;
    for (uint8_t index = 0; index < 8; ++index) {
        const uint8_t channel = first + index;
        char label[24];
        if (channel == 1) snprintf(label, sizeof(label), "CH1  STEERING");
        else if (channel == 2) snprintf(label, sizeof(label), "CH2  THROTTLE");
        else snprintf(label, sizeof(label), "CH%u  AUX", channel);
        makeButton(10 + (index % 2) * 235, 52 + (index / 2) * 52, 225, 44,
                   label, 16, channel == 2 ? cPanel2 : cPanel,
                   channel == 2 ? cCyan : cWhite);
    }
    makeButton(12, 270, 58, 40, "<", 24,
               channelsBank_ == 0 ? cBlack : cPanel,
               channelsBank_ == 0 ? cDim : cWhite);
    makeButton(410, 270, 58, 40, ">", 24,
               channelsBank_ == 1 ? cBlack : cPanel,
               channelsBank_ == 1 ? cDim : cWhite);
    char page[8];
    snprintf(page, sizeof(page), "%u / 2", channelsBank_ + 1);
    lv_obj_t *pageLabel = makeLabel(lv_scr_act(), page, 0, 280, 16, cDim);
    lv_obj_align(pageLabel, LV_ALIGN_TOP_MID, 0, 280);
}

void Channel2Ui::drawChannelPlaceholder()
{
    activePage_ = UiPage::ChannelPlaceholder;
    startScreen();
    resetLvglObjects();
    char title[20];
    snprintf(title, sizeof(title), "CHANNEL %u", selectedChannel_);
    makeHeader(title, "BACK");
    lv_obj_t *coming = makeLabel(lv_scr_act(), "CONFIGURATION\nCOMING NEXT", 0, 110, 28, cCyan);
    lv_obj_set_style_text_align(coming, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(coming, LV_ALIGN_TOP_MID, 0, 110);
    char detail[40];
    snprintf(detail, sizeof(detail), "CRSF CHANNEL %u IS CENTRED", selectedChannel_);
    lv_obj_t *detailLabel = makeLabel(lv_scr_act(), detail, 0, 226, 16, cDim);
    lv_obj_align(detailLabel, LV_ALIGN_TOP_MID, 0, 226);
}

void Channel2Ui::drawSettingRow(int row)
{
    static const char *labels[] = {"BRAKE END", "SUB-TRIM", "THROTTLE END", "DIRECTION"};
    const int16_t y = 56 + row * 50;
    lv_obj_t *panel = makePanel(10, y, 460, 44);
    makeLabel(panel, labels[row], 12, 12, 16, cCyan);
    char value[20];
    if (row == 0) snprintf(value, sizeof(value), "%u%%", settings_.brakeEndpointPercent);
    else if (row == 1) snprintf(value, sizeof(value), "%+d%%", settings_.subtrimPercent);
    else if (row == 2) snprintf(value, sizeof(value), "%u%%", settings_.throttleEndpointPercent);
    else snprintf(value, sizeof(value), "%s", settings_.reversed ? "REVERSE" : "NORMAL");
    makeLabel(panel, value, 212, 12, 16, row == 3 && settings_.reversed ? cAmber : cWhite);
    if (row < 3) {
        lv_obj_t *minus = lv_obj_create(panel);
        position(minus, 343, 3, 48, 36);
        lv_obj_set_style_bg_color(minus, colour(cPanel2), 0);
        lv_obj_set_style_border_color(minus, colour(cLine), 0);
        lv_obj_set_style_border_width(minus, 1, 0);
        lv_obj_set_style_radius(minus, 5, 0);
        lv_obj_set_style_pad_all(minus, 0, 0);
        lv_obj_t *ml = makeLabel(minus, "-", 0, 0, 24); lv_obj_center(ml);
        lv_obj_t *plus = lv_obj_create(panel);
        position(plus, 401, 3, 48, 36);
        lv_obj_set_style_bg_color(plus, colour(cPanel2), 0);
        lv_obj_set_style_border_color(plus, colour(cLine), 0);
        lv_obj_set_style_border_width(plus, 1, 0);
        lv_obj_set_style_radius(plus, 5, 0);
        lv_obj_set_style_pad_all(plus, 0, 0);
        lv_obj_t *pl = makeLabel(plus, "+", 0, 0, 24); lv_obj_center(pl);
    } else {
        lv_obj_t *toggle = lv_obj_create(panel);
        position(toggle, 343, 3, 106, 36);
        lv_obj_set_style_bg_color(toggle, colour(settings_.reversed ? cAmber : cPanel2), 0);
        lv_obj_set_style_border_color(toggle, colour(cLine), 0);
        lv_obj_set_style_border_width(toggle, 1, 0);
        lv_obj_set_style_radius(toggle, 5, 0);
        lv_obj_set_style_pad_all(toggle, 0, 0);
        lv_obj_t *tl = makeLabel(toggle, "TOGGLE", 0, 0, 14,
                                 settings_.reversed ? cBlack : cWhite);
        lv_obj_center(tl);
    }
}

void Channel2Ui::drawPage()
{
    activePage_ = UiPage::Channel2;
    startScreen();
    resetLvglObjects();
    makeHeader("CH2  THROTTLE");
    makeButton(264, 5, 72, 34, "BACK", 12);
    makeButton(344, 5, 58, 34, "CAL", 12);
    channel2ValueLabel_ = makeLabel(lv_scr_act(), " 992", 414, 12, 16, cGreen);
    for (int row = 0; row < 4; ++row) drawSettingRow(row);
    makeLabel(lv_scr_act(), "CH2", 12, 283, 16, cCyan);
    channel2LiveBar_ = makeBar(lv_scr_act(), 60, 287, 402, 14, 0, kCrsfMaximum, cGreen);
    lv_bar_set_value(channel2LiveBar_, kCrsfMid, LV_ANIM_OFF);
    makeLabel(lv_scr_act(), "BRAKE", 60, 304, 12, cDim);
    makeLabel(lv_scr_act(), "THROTTLE", 400, 304, 12, cDim);
    lastDisplayedCrsf_ = UINT16_MAX;
}

void Channel2Ui::drawLiveIndicator(int16_t, uint16_t crsfValue)
{
    if (channel2LiveBar_ && crsfValue != lastDisplayedCrsf_) {
        lv_bar_set_value(channel2LiveBar_, crsfValue, LV_ANIM_OFF);
        lv_label_set_text_fmt(channel2ValueLabel_, "%4u", crsfValue);
        lastDisplayedCrsf_ = crsfValue;
    }
}

void Channel2Ui::drawSetupRow(int row)
{
    static const char *labels[] = {"BUZZER", "KEY CLICKS", "PITCH", "CLICK LENGTH"};
    const int16_t y = 56 + row * 50;
    lv_obj_t *panel = makePanel(10, y, 460, 44);
    makeLabel(panel, labels[row], 12, 12, 16, cCyan);
    char value[20];
    if (row == 0) snprintf(value, sizeof(value), "%s", systemSettings_.buzzerEnabled ? "ON" : "OFF");
    else if (row == 1) snprintf(value, sizeof(value), "%s", systemSettings_.keyClicksEnabled ? "ON" : "OFF");
    else if (row == 2) snprintf(value, sizeof(value), "%u Hz", systemSettings_.buzzerPitchHz);
    else snprintf(value, sizeof(value), "%u ms", systemSettings_.keyClickDurationMs);
    makeLabel(panel, value, 212, 12, 16, cWhite);
    if (row < 2) {
        const bool enabled = row == 0 ? systemSettings_.buzzerEnabled : systemSettings_.keyClicksEnabled;
        lv_obj_t *toggle = lv_obj_create(panel);
        position(toggle, 343, 3, 106, 36);
        lv_obj_set_style_bg_color(toggle, colour(enabled ? cGreen : cPanel2), 0);
        lv_obj_set_style_border_color(toggle, colour(cLine), 0);
        lv_obj_set_style_border_width(toggle, 1, 0);
        lv_obj_set_style_radius(toggle, 5, 0);
        lv_obj_set_style_pad_all(toggle, 0, 0);
        lv_obj_t *label = makeLabel(toggle, "TOGGLE", 0, 0, 14, enabled ? cBlack : cWhite);
        lv_obj_center(label);
    } else {
        lv_obj_t *minus = lv_obj_create(panel); position(minus, 343, 3, 48, 36);
        lv_obj_set_style_bg_color(minus, colour(cPanel2), 0); lv_obj_set_style_pad_all(minus, 0, 0);
        lv_obj_t *ml = makeLabel(minus, "-", 0, 0, 24); lv_obj_center(ml);
        lv_obj_t *plus = lv_obj_create(panel); position(plus, 401, 3, 48, 36);
        lv_obj_set_style_bg_color(plus, colour(cPanel2), 0); lv_obj_set_style_pad_all(plus, 0, 0);
        lv_obj_t *pl = makeLabel(plus, "+", 0, 0, 24); lv_obj_center(pl);
    }
}

void Channel2Ui::drawSetupPage()
{
    activePage_ = UiPage::Setup;
    startScreen();
    resetLvglObjects();
    makeHeader("SYSTEM SETUP", "BACK");
    for (int row = 0; row < 4; ++row) drawSetupRow(row);
    makeButton(70, 265, 130, 44, "TEST", 16, cPanel2, cAmber);
    makeButton(210, 265, 200, 44, "TX BATTERY", 16, cPanel2, cCyan);
}

void Channel2Ui::drawTxBatteryRow(int row)
{
    static const char *labels[] = {"CHEMISTRY", "CELL COUNT", "LOW ALARM", "VOLT SENSE"};
    const int16_t y = 56 + row * 50;
    lv_obj_t *panel = makePanel(10, y, 460, 44);
    makeLabel(panel, labels[row], 12, 12, 16, cCyan);
    char value[24];
    if (row == 0) {
        snprintf(value, sizeof(value), "%s", batteryChemistryName(txBatterySettings_.chemistry));
    } else if (row == 1) {
        snprintf(value, sizeof(value), "%uS", txBatterySettings_.cellCount);
    } else if (row == 2) {
        snprintf(value, sizeof(value), "%u.%u V",
                 txBatterySettings_.lowAlarmDecivolts / 10,
                 txBatterySettings_.lowAlarmDecivolts % 10);
    } else {
        snprintf(value, sizeof(value), "%s",
                 txBatterySettings_.voltageSenseEnabled ? "ON" : "OFF");
    }
    makeLabel(panel, value, 212, 12, 16,
              row == 3 && txBatterySettings_.voltageSenseEnabled ? cGreen : cWhite);

    if (row == 0 || row == 3) {
        lv_obj_t *toggle = lv_obj_create(panel);
        position(toggle, 343, 3, 106, 36);
        lv_obj_set_style_bg_color(toggle,
            colour(row == 3 && txBatterySettings_.voltageSenseEnabled ? cGreen : cPanel2), 0);
        lv_obj_set_style_border_color(toggle, colour(cLine), 0);
        lv_obj_set_style_border_width(toggle, 1, 0);
        lv_obj_set_style_radius(toggle, 5, 0);
        lv_obj_set_style_pad_all(toggle, 0, 0);
        lv_obj_t *label = makeLabel(toggle, row == 0 ? "NEXT" : "TOGGLE", 0, 0, 14,
            row == 3 && txBatterySettings_.voltageSenseEnabled ? cBlack : cWhite);
        lv_obj_center(label);
    } else {
        lv_obj_t *minus = lv_obj_create(panel); position(minus, 343, 3, 48, 36);
        lv_obj_set_style_bg_color(minus, colour(cPanel2), 0);
        lv_obj_set_style_border_color(minus, colour(cLine), 0);
        lv_obj_set_style_border_width(minus, 1, 0);
        lv_obj_set_style_radius(minus, 5, 0);
        lv_obj_set_style_pad_all(minus, 0, 0);
        lv_obj_t *ml = makeLabel(minus, "-", 0, 0, 24); lv_obj_center(ml);
        lv_obj_t *plus = lv_obj_create(panel); position(plus, 401, 3, 48, 36);
        lv_obj_set_style_bg_color(plus, colour(cPanel2), 0);
        lv_obj_set_style_border_color(plus, colour(cLine), 0);
        lv_obj_set_style_border_width(plus, 1, 0);
        lv_obj_set_style_radius(plus, 5, 0);
        lv_obj_set_style_pad_all(plus, 0, 0);
        lv_obj_t *pl = makeLabel(plus, "+", 0, 0, 24); lv_obj_center(pl);
    }
}

void Channel2Ui::drawTxBatteryPage()
{
    activePage_ = UiPage::TxBatterySetup;
    startScreen();
    resetLvglObjects();
    makeHeader("TX BATTERY", "BACK");
    for (int row = 0; row < 4; ++row) drawTxBatteryRow(row);
    lv_obj_t *note = makeLabel(lv_scr_act(),
        "GPIO5 via 47k/10k divider", 0, 280, 12, cDim);
    lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 280);
}

void Channel2Ui::drawCalibrationPage()
{
    startScreen();
    resetLvglObjects();
    makeHeader("THROTTLE CALIBRATION", "CANCEL");
    makeLabel(lv_scr_act(), "1. MOVE TO FULL BRAKE", 20, 62, 16, cCyan);
    makeLabel(lv_scr_act(), "2. MOVE TO FULL THROTTLE", 20, 92, 16, cCyan);
    makeLabel(lv_scr_act(), "3. RELEASE AND HOLD CENTRE", 20, 122, 16, cCyan);
    makeLabel(lv_scr_act(), "CH2 OUTPUT HELD AT NEUTRAL", 20, 160, 16, cAmber);
    lv_obj_t *panel = makePanel(18, 202, 444, 92);
    makeLabel(panel, "MIN", 16, 10, 14, cDim);
    makeLabel(panel, "LIVE", 180, 10, 14, cDim);
    makeLabel(panel, "MAX", 342, 10, 14, cDim);
    calibrationMinimumLabel_ = makeLabel(panel, "----", 10, 42, 24);
    calibrationLiveLabel_ = makeLabel(panel, "----", 168, 42, 24);
    calibrationMaximumLabel_ = makeLabel(panel, "----", 326, 42, 24);
    drawCalibrationValues(latestRawAdc_);
}

void Channel2Ui::drawCalibrationValues(uint16_t adc)
{
    if (!calibrationActive_ || !calibrationLiveLabel_) return;
    lv_label_set_text_fmt(calibrationMinimumLabel_, "%4u", calibrationMinimum_);
    lv_label_set_text_fmt(calibrationLiveLabel_, "%4u", adc);
    lv_label_set_text_fmt(calibrationMaximumLabel_, "%4u", calibrationMaximum_);
    lv_obj_set_style_text_color(calibrationMinimumLabel_,
        colour(calibrationSawMinimum_ ? cGreen : cWhite), 0);
    lv_obj_set_style_text_color(calibrationMaximumLabel_,
        colour(calibrationSawMaximum_ ? cGreen : cWhite), 0);
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
    const uint16_t low = calibration_.centre - max<uint16_t>(100, (calibration_.centre - calibration_.minimum) / 3);
    const uint16_t high = calibration_.centre + max<uint16_t>(100, (calibration_.maximum - calibration_.centre) / 3);
    calibrationSawMinimum_ |= adc <= low;
    calibrationSawMaximum_ |= adc >= high;
    if (!calibrationSawMinimum_ || !calibrationSawMaximum_) { calibrationStableSinceMs_ = 0; return; }
    const uint16_t span = calibrationMaximum_ - calibrationMinimum_;
    if (adc <= calibrationMinimum_ + span / 8 || adc >= calibrationMaximum_ - span / 8) {
        calibrationStableSinceMs_ = 0; calibrationStableAdc_ = adc; return;
    }
    if (abs(static_cast<int>(adc) - static_cast<int>(calibrationStableAdc_)) > kCalibrationMovementThreshold) {
        calibrationStableAdc_ = adc; calibrationStableSinceMs_ = millis(); return;
    }
    if (calibrationStableSinceMs_ == 0) calibrationStableSinceMs_ = millis();
    if (millis() - calibrationStableSinceMs_ < kCalibrationStableMs) return;
    if (calibrationMinimum_ + 100 >= adc || adc + 100 >= calibrationMaximum_) {
        calibrationStableSinceMs_ = 0; return;
    }
    calibration_.minimum = calibrationMinimum_;
    calibration_.centre = adc;
    calibration_.maximum = calibrationMaximum_;
    saveCalibration();
    calibrationActive_ = false;
    Serial.printf("CAL: saved min=%u centre=%u max=%u\n", calibration_.minimum,
                  calibration_.centre, calibration_.maximum);
    playBuzzer(180);
    drawPage();
}

uint16_t Channel2Ui::readTouchRegister(uint8_t command)
{
    Wire.beginTransmission(NS2009_I2C_ADDRESS);
    Wire.write(command);
    if (Wire.endTransmission() != 0) return 0;
    if (Wire.requestFrom(NS2009_I2C_ADDRESS, static_cast<uint8_t>(2)) != 2) return 0;
    return static_cast<uint16_t>((Wire.read() << 4) | (Wire.read() >> 4));
}

bool Channel2Ui::readTouch(int16_t &x, int16_t &y, uint16_t &rawX,
                           uint16_t &rawY, uint16_t &rawZ)
{
    if (digitalRead(TOUCH_IRQ_PIN) != LOW) return false;
    rawZ = readTouchRegister(kNsReadZ);
    if (rawZ <= kTouchPressureThreshold || rawZ == 0x0fff) return false;
    rawX = readTouchRegister(kNsReadX);
    rawY = readTouchRegister(kNsReadY);
    x = constrain(map(rawY, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX, 0, kScreenWidth - 1), 0, kScreenWidth - 1);
    y = constrain(map(rawX, TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX, kScreenHeight - 1, 0), 0, kScreenHeight - 1);
    return true;
}

void Channel2Ui::handleDashboardTouch(int16_t x, int16_t y)
{
    if ((y < 45 && x >= 415) || (y >= 245 && x < 120)) { playKeyClick(); drawMainMenu(); }
    else if (y >= 245 && x >= 120 && x < 236) { playKeyClick(); drawChannelsPage(); }
}

void Channel2Ui::handleMainMenuTouch(int16_t x, int16_t y)
{
    if (y < 48 && x >= 390) { playKeyClick(); drawDashboard(); return; }
    if (x < 5 || x >= 475 || y < 52 || y >= 270) return;
    const int item = ((y - 52) / 54) * 2 + (x >= 240 ? 1 : 0);
    playKeyClick();
    if (item == 1) drawChannelsPage();
    else if (item == 7) drawSetupPage();
}

void Channel2Ui::handleChannelsTouch(int16_t x, int16_t y)
{
    if (y < 48 && x >= 360) { playKeyClick(); drawMainMenu(); return; }
    if (y >= 270) {
        if (x < 90 && channelsBank_ > 0) { --channelsBank_; playKeyClick(); drawChannelsPage(); }
        else if (x >= 390 && channelsBank_ < 1) { ++channelsBank_; playKeyClick(); drawChannelsPage(); }
        return;
    }
    if (y < 52 || y >= 264) return;
    selectedChannel_ = channelsBank_ * 8 + ((y - 52) / 52) * 2 + (x >= 240 ? 1 : 0) + 1;
    playKeyClick();
    if (selectedChannel_ == 2) drawPage(); else drawChannelPlaceholder();
}

void Channel2Ui::handleSetupTouch(int16_t x, int16_t y)
{
    if (y < 48 && x >= 360) { playKeyClick(); drawMainMenu(); return; }
    if (y >= 265 && y < 315 && x >= 60 && x < 205) { playBuzzer(500); return; }
    if (y >= 265 && y < 315 && x >= 205 && x < 420) {
        playKeyClick(); drawTxBatteryPage(); return;
    }
    if (y < 56 || y >= 256) return;
    const int row = (y - 56) / 50;
    bool changed = false;
    if (row < 2 && x >= 340) {
        if (row == 0) { systemSettings_.buzzerEnabled = !systemSettings_.buzzerEnabled; if (!systemSettings_.buzzerEnabled) stopBuzzer(); }
        else systemSettings_.keyClicksEnabled = !systemSettings_.keyClicksEnabled;
        changed = true;
    } else if (row == 2 && x >= 354 && x < 404 && systemSettings_.buzzerPitchHz > kBuzzerMinimumPitchHz) {
        systemSettings_.buzzerPitchHz -= kBuzzerPitchStepHz; changed = true;
    } else if (row == 2 && x >= 408 && systemSettings_.buzzerPitchHz < kBuzzerMaximumPitchHz) {
        systemSettings_.buzzerPitchHz += kBuzzerPitchStepHz; changed = true;
    } else if (row == 3 && x >= 354 && x < 404 && systemSettings_.keyClickDurationMs > kBuzzerMinimumDurationMs) {
        systemSettings_.keyClickDurationMs -= kBuzzerDurationStepMs; changed = true;
    } else if (row == 3 && x >= 408 && systemSettings_.keyClickDurationMs < kBuzzerMaximumDurationMs) {
        systemSettings_.keyClickDurationMs += kBuzzerDurationStepMs; changed = true;
    }
    if (changed) { saveSystemSettings(); playKeyClick(); drawSetupPage(); }
}

void Channel2Ui::handleTxBatteryTouch(int16_t x, int16_t y)
{
    if (y < 48 && x >= 360) { playKeyClick(); drawSetupPage(); return; }
    if (y < 56 || y >= 256) return;
    const int row = (y - 56) / 50;
    bool changed = false;

    if (row == 0 && x >= 340) {
        uint8_t chemistry = static_cast<uint8_t>(txBatterySettings_.chemistry);
        chemistry = (chemistry + 1) % 3;
        txBatterySettings_.chemistry = static_cast<BatteryChemistry>(chemistry);
        txBatterySettings_.cellCount = min(txBatterySettings_.cellCount,
            maximumCellsFor(txBatterySettings_.chemistry));
        txBatterySettings_.lowAlarmDecivolts = defaultLowAlarmDecivolts(
            txBatterySettings_.chemistry, txBatterySettings_.cellCount);
        changed = true;
    } else if (row == 1 && x >= 343 && x < 400 && txBatterySettings_.cellCount > 1) {
        --txBatterySettings_.cellCount;
        txBatterySettings_.lowAlarmDecivolts = defaultLowAlarmDecivolts(
            txBatterySettings_.chemistry, txBatterySettings_.cellCount);
        changed = true;
    } else if (row == 1 && x >= 400 &&
               txBatterySettings_.cellCount < maximumCellsFor(txBatterySettings_.chemistry)) {
        ++txBatterySettings_.cellCount;
        txBatterySettings_.lowAlarmDecivolts = defaultLowAlarmDecivolts(
            txBatterySettings_.chemistry, txBatterySettings_.cellCount);
        changed = true;
    } else if (row == 2 && x >= 343 && x < 400 &&
               txBatterySettings_.lowAlarmDecivolts > kTxBatteryMinimumAlarmDecivolts) {
        --txBatterySettings_.lowAlarmDecivolts; changed = true;
    } else if (row == 2 && x >= 400 &&
               txBatterySettings_.lowAlarmDecivolts < kTxBatteryMaximumAlarmDecivolts) {
        ++txBatterySettings_.lowAlarmDecivolts; changed = true;
    } else if (row == 3 && x >= 340) {
        txBatterySettings_.voltageSenseEnabled = !txBatterySettings_.voltageSenseEnabled;
        changed = true;
    }

    if (changed) {
        saveTxBatterySettings();
        playKeyClick();
        drawTxBatteryPage();
    }
}

void Channel2Ui::handleTouch(int16_t x, int16_t y)
{
    if (calibrationActive_) { if (y < 48 && x >= 360) cancelCalibration(); return; }
    if (activePage_ == UiPage::Dashboard) { handleDashboardTouch(x, y); return; }
    if (activePage_ == UiPage::MainMenu) { handleMainMenuTouch(x, y); return; }
    if (activePage_ == UiPage::Channels) { handleChannelsTouch(x, y); return; }
    if (activePage_ == UiPage::ChannelPlaceholder) { if (y < 48 && x >= 360) { playKeyClick(); drawChannelsPage(); } return; }
    if (activePage_ == UiPage::Setup) { handleSetupTouch(x, y); return; }
    if (activePage_ == UiPage::TxBatterySetup) { handleTxBatteryTouch(x, y); return; }
    if (y < 48 && x >= 250 && x < 340) { playKeyClick(); drawChannelsPage(); return; }
    if (y < 48 && x >= 340) { playKeyClick(); startCalibration(latestRawAdc_); return; }
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
    } else if (row == 3 && x >= 340) { settings_.reversed = !settings_.reversed; changed = true; }
    if (changed) { playKeyClick(); saveSettings(); drawPage(); }
}

void Channel2Ui::update(uint16_t rawAdc, int16_t, int16_t outputPermille,
                        uint16_t crsfValue, bool throttleArmed,
                        const CrsfTelemetryState &telemetry,
                        uint16_t txVoltageDecivolts, bool txVoltageValid)
{
    latestRawAdc_ = rawAdc;
    int16_t x = 0, y = 0;
    uint16_t rawX = 0, rawY = 0, rawZ = 0;
    const bool touched = readTouch(x, y, rawX, rawY, rawZ);
    if (touched) {
        lastTouchX_ = x; lastTouchY_ = y;
        if (!touchWasDown_) Serial.printf("TOUCH raw=(%u,%u,%u) screen=(%d,%d)\n", rawX, rawY, rawZ, x, y);
    } else if (touchWasDown_) handleTouch(lastTouchX_, lastTouchY_);
    touchWasDown_ = touched;
    if (calibrationActive_) updateCalibration(rawAdc);

    const uint32_t now = millis();
    if (static_cast<int32_t>(now - nextDisplayUpdateMs_) >= 0) {
        nextDisplayUpdateMs_ = now + kUiUpdateMs;
        if (calibrationActive_) drawCalibrationValues(rawAdc);
        else if (activePage_ == UiPage::Dashboard) {
            drawDashboardLive(crsfValue, throttleArmed, telemetry,
                              txVoltageDecivolts, txVoltageValid);
        }
        else if (activePage_ == UiPage::Channel2) drawLiveIndicator(outputPermille, crsfValue);
    }
    lv_timer_handler();
}
