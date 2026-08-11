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
constexpr uint32_t kTouchReleaseDebounceMs = 25;
constexpr uint32_t kTouchRetriggerGuardMs = 75;
constexpr uint8_t kNsReadX = 0xC0;
constexpr uint8_t kNsReadY = 0xD0;
constexpr uint8_t kNsReadZ = 0xE0;
constexpr uint16_t kBuzzerMinimumPitchHz = 500;
constexpr uint16_t kBuzzerMaximumPitchHz = 5000;
constexpr uint16_t kBuzzerPitchStepHz = 100;
constexpr uint16_t kBuzzerMinimumDurationMs = 10;
constexpr uint16_t kBuzzerMaximumDurationMs = 200;
constexpr uint16_t kBuzzerDurationStepMs = 10;
constexpr uint16_t kBatteryMinimumAlarmDecivolts = 10;
constexpr uint16_t kBatteryMaximumAlarmDecivolts = 168;
constexpr uint32_t kLowBatteryRepeatMs = 10000;
constexpr uint8_t kResponseRateStepPercent = 5;
constexpr int8_t kResponseExpoStepPercent = 5;
constexpr uint16_t kChannelSpeedStepMs = 50;
constexpr uint16_t kChannelSpeedMaximumMs = 2000;

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

uint8_t uiBrightnessPercent = 100;
bool uiNightMode = false;

Arduino_DataBus *displayBus = new Arduino_ESP32SPI(
    TFT_DC_PIN, TFT_CS_PIN, TFT_SCK_PIN, TFT_MOSI_PIN, TFT_MISO_PIN);
Arduino_GFX *display = new Arduino_ILI9486_18bit(
    displayBus, GFX_NOT_DEFINED, 1, false);

lv_disp_draw_buf_t drawBuffer;
lv_disp_drv_t displayDriver;
lv_color_t lineBuffer[kScreenWidth * 16];

lv_color_t colour(uint32_t rgb)
{
    uint16_t scale = uiBrightnessPercent;
    if (uiNightMode) scale = scale * 60U / 100U;
    const uint8_t red = ((rgb >> 16) & 0xffU) * scale / 100U;
    const uint8_t green = ((rgb >> 8) & 0xffU) * scale / 100U;
    const uint8_t blue = (rgb & 0xffU) * scale / 100U;
    rgb = (static_cast<uint32_t>(red) << 16) |
          (static_cast<uint32_t>(green) << 8) | blue;
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

int16_t applyExpo(int16_t value, int8_t expoPercent)
{
    const int32_t sign = value < 0 ? -1 : 1;
    const int32_t magnitude = abs(static_cast<int32_t>(value));
    const int32_t cubic = magnitude * magnitude * magnitude / 1000000L;
    const int32_t shaped = magnitude +
        static_cast<int32_t>(expoPercent) * (cubic - magnitude) / 100;
    return static_cast<int16_t>(sign * constrain(shaped, 0L, 1000L));
}

uint8_t adjacentChannel(uint8_t current, int8_t direction, uint8_t unavailable)
{
    for (uint8_t tries = 0; tries < 16; ++tries) {
        int16_t candidate = static_cast<int16_t>(current) + direction;
        if (candidate < 1) candidate = 16;
        if (candidate > 16) candidate = 1;
        current = static_cast<uint8_t>(candidate);
        if (current != unavailable) return current;
    }
    return current;
}

const char *batteryChemistryName(BatteryChemistry chemistry)
{
    switch (chemistry) {
        case BatteryChemistry::LiFe: return "LiFe";
        case BatteryChemistry::LiPo: return "LiPo";
        case BatteryChemistry::NiMH: return "NiMH";
        case BatteryChemistry::LiIon: return "Li-ion";
    }
    return "?";
}

uint8_t maximumCellsFor(BatteryChemistry chemistry)
{
    return chemistry == BatteryChemistry::NiMH ? 12 : 4;
}

uint16_t defaultLowAlarmDecivolts(BatteryChemistry chemistry,
                                  uint8_t cells)
{
    uint8_t perCellDecivolts = 30; // 3.0 V/cell for LiFe
    if (chemistry == BatteryChemistry::LiPo) perCellDecivolts = 35;
    else if (chemistry == BatteryChemistry::NiMH) perCellDecivolts = 10;
    else if (chemistry == BatteryChemistry::LiIon) perCellDecivolts = 33;
    return static_cast<uint16_t>(perCellDecivolts) * cells;
}

const char *uiPageName(UiPage page)
{
    switch (page) {
        case UiPage::Dashboard: return "dashboard";
        case UiPage::MainMenu: return "main-menu";
        case UiPage::Channels: return "channels";
        case UiPage::ChannelRoles: return "channel-roles";
        case UiPage::Channel2: return "channel";
        case UiPage::ThrottleResponse: return "response";
        case UiPage::ChannelSpeed: return "channel-speed";
        case UiPage::ControlAssignments: return "control-map";
        case UiPage::InputMonitor: return "input-monitor";
        case UiPage::DriveModes: return "drive-modes";
        case UiPage::ChannelPlaceholder: return "channel-placeholder";
        case UiPage::Setup: return "setup";
        case UiPage::TxBatterySetup: return "tx-battery";
        case UiPage::RxBatterySetup: return "rx-battery";
        case UiPage::DisplaySetup: return "display";
        case UiPage::Calibration: return "calibration";
    }
    return "unknown";
}

} // namespace

bool Channel2Ui::begin()
{
    loadSettings();
    loadChannelRoleSettings();
    loadSystemSettings();
    loadTxBatterySettings();
    loadRxBatterySettings();
    loadDriveModeSettings();
    uiBrightnessPercent = systemSettings_.displayBrightnessPercent;
    uiNightMode = systemSettings_.nightMode;
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
    lastInteractionMs_ = millis();
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
    settings_.forwardEndpointPercent = preferences.getUChar("throt_ep", 100);
    settings_.brakeRatePercent = preferences.getUChar("brk_rate", 100);
    settings_.forwardRatePercent = preferences.getUChar("fwd_rate", 100);
    settings_.brakeExpoPercent = preferences.getChar("brk_expo", 0);
    settings_.forwardExpoPercent = preferences.getChar("fwd_expo", 0);
    settings_.brakeSpeedMs = preferences.getUShort("brk_speed", 0);
    settings_.forwardSpeedMs = preferences.getUShort("fwd_speed", 0);
    settings_.subtrimPercent = preferences.getChar("trim", 0);
    settings_.reversed = preferences.getBool("reverse", false);
    // Read the old channel-scoped keys as migration defaults. Calibration is
    // now stored against the physical trigger input, not its assigned output.
    const ProportionalCalibration legacyCalibration {
        preferences.getUShort("adc_min", THROTTLE_ADC_MIN),
        preferences.getUShort("adc_ctr", THROTTLE_ADC_CENTER),
        preferences.getUShort("adc_max", THROTTLE_ADC_MAX),
    };
    preferences.end();

    preferences.begin("jr-r1-trigger", true);
    ProportionalCalibration calibration {
        preferences.getUShort("adc_min", legacyCalibration.minimum),
        preferences.getUShort("adc_ctr", legacyCalibration.centre),
        preferences.getUShort("adc_max", legacyCalibration.maximum),
    };
    preferences.end();

    settings_.brakeEndpointPercent = constrain(settings_.brakeEndpointPercent, 50, 100);
    settings_.forwardEndpointPercent = constrain(settings_.forwardEndpointPercent, 50, 100);
    settings_.brakeRatePercent = constrain(settings_.brakeRatePercent, 0, 100);
    settings_.forwardRatePercent = constrain(settings_.forwardRatePercent, 0, 100);
    settings_.brakeExpoPercent = constrain(settings_.brakeExpoPercent, -100, 100);
    settings_.forwardExpoPercent = constrain(settings_.forwardExpoPercent, -100, 100);
    settings_.brakeSpeedMs = constrain(settings_.brakeSpeedMs, 0, kChannelSpeedMaximumMs);
    settings_.forwardSpeedMs = constrain(settings_.forwardSpeedMs, 0, kChannelSpeedMaximumMs);
    settings_.subtrimPercent = constrain(settings_.subtrimPercent, -20, 20);
    if (!calibration.valid()) {
        calibration = {THROTTLE_ADC_MIN, THROTTLE_ADC_CENTER, THROTTLE_ADC_MAX};
    }
    triggerInput_.setCalibration(calibration);
}

void Channel2Ui::saveSettings()
{
    Preferences preferences;
    preferences.begin("jr-r1-ch2", false);
    preferences.putUChar("brake_ep", settings_.brakeEndpointPercent);
    preferences.putUChar("throt_ep", settings_.forwardEndpointPercent);
    preferences.putUChar("brk_rate", settings_.brakeRatePercent);
    preferences.putUChar("fwd_rate", settings_.forwardRatePercent);
    preferences.putChar("brk_expo", settings_.brakeExpoPercent);
    preferences.putChar("fwd_expo", settings_.forwardExpoPercent);
    preferences.putUShort("brk_speed", settings_.brakeSpeedMs);
    preferences.putUShort("fwd_speed", settings_.forwardSpeedMs);
    preferences.putChar("trim", settings_.subtrimPercent);
    preferences.putBool("reverse", settings_.reversed);
    preferences.end();
}

void Channel2Ui::loadChannelRoleSettings()
{
    Preferences preferences;
    preferences.begin("jr-r1-model1", true);
    channelRoleSettings_.steeringChannel = preferences.getUChar("steer_ch", 1);
    channelRoleSettings_.throttleChannel = preferences.getUChar("throt_ch", 2);
    channelRoleSettings_.steeringEnabled = preferences.getBool("steer_en", true);
    channelRoleSettings_.throttleEnabled = preferences.getBool("throt_en", true);
    preferences.end();
    channelRoleSettings_.steeringChannel = constrain(channelRoleSettings_.steeringChannel, 1, 16);
    channelRoleSettings_.throttleChannel = constrain(channelRoleSettings_.throttleChannel, 1, 16);
    if (channelRoleSettings_.steeringChannel == channelRoleSettings_.throttleChannel) {
        channelRoleSettings_.steeringChannel = 1;
        channelRoleSettings_.throttleChannel = 2;
    }
}

void Channel2Ui::saveChannelRoleSettings()
{
    Preferences preferences;
    preferences.begin("jr-r1-model1", false);
    preferences.putUChar("steer_ch", channelRoleSettings_.steeringChannel);
    preferences.putUChar("throt_ch", channelRoleSettings_.throttleChannel);
    preferences.putBool("steer_en", channelRoleSettings_.steeringEnabled);
    preferences.putBool("throt_en", channelRoleSettings_.throttleEnabled);
    preferences.end();
}

bool Channel2Ui::consumeChannelRolesChanged()
{
    const bool changed = channelRolesChanged_;
    channelRolesChanged_ = false;
    return changed;
}

bool Channel2Ui::consumeSafetyResetRequested()
{
    const bool requested = safetyResetRequested_;
    safetyResetRequested_ = false;
    return requested;
}

void Channel2Ui::saveCalibration()
{
    const ProportionalCalibration &calibration = triggerInput_.calibration();
    Preferences preferences;
    preferences.begin("jr-r1-trigger", false);
    preferences.putUShort("adc_min", calibration.minimum);
    preferences.putUShort("adc_ctr", calibration.centre);
    preferences.putUShort("adc_max", calibration.maximum);
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
    systemSettings_.screenTimeoutSeconds = preferences.getUShort("scr_timeout", 60);
    systemSettings_.displayBrightnessPercent = preferences.getUChar("brightness", 100);
    systemSettings_.nightMode = preferences.getBool("night", false);
    preferences.end();
    systemSettings_.buzzerPitchHz = constrain(systemSettings_.buzzerPitchHz,
                                               kBuzzerMinimumPitchHz,
                                               kBuzzerMaximumPitchHz);
    systemSettings_.keyClickDurationMs = constrain(systemSettings_.keyClickDurationMs,
                                                    kBuzzerMinimumDurationMs,
                                                    kBuzzerMaximumDurationMs);
    if (systemSettings_.screenTimeoutSeconds != 0 &&
        systemSettings_.screenTimeoutSeconds != 30 &&
        systemSettings_.screenTimeoutSeconds != 60 &&
        systemSettings_.screenTimeoutSeconds != 300) {
        systemSettings_.screenTimeoutSeconds = 60;
    }
    systemSettings_.displayBrightnessPercent = constrain(
        systemSettings_.displayBrightnessPercent, 20, 100);
}

void Channel2Ui::saveSystemSettings()
{
    Preferences preferences;
    preferences.begin("jr-r1-system", false);
    preferences.putBool("buzz_en", systemSettings_.buzzerEnabled);
    preferences.putBool("key_beep", systemSettings_.keyClicksEnabled);
    preferences.putUShort("pitch", systemSettings_.buzzerPitchHz);
    preferences.putUShort("click_ms", systemSettings_.keyClickDurationMs);
    preferences.putUShort("scr_timeout", systemSettings_.screenTimeoutSeconds);
    preferences.putUChar("brightness", systemSettings_.displayBrightnessPercent);
    preferences.putBool("night", systemSettings_.nightMode);
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
        static_cast<uint8_t>(BatteryChemistry::LiIon)) {
        txBatterySettings_.chemistry = BatteryChemistry::LiFe;
    }
    txBatterySettings_.cellCount = constrain(txBatterySettings_.cellCount, 1, 12);
    txBatterySettings_.lowAlarmDecivolts = constrain(
        txBatterySettings_.lowAlarmDecivolts,
        kBatteryMinimumAlarmDecivolts, kBatteryMaximumAlarmDecivolts);
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

void Channel2Ui::loadRxBatterySettings()
{
    Preferences preferences;
    preferences.begin("jr-r1-rxbat", true);
    rxBatterySettings_.chemistry = static_cast<BatteryChemistry>(
        preferences.getUChar("chem", static_cast<uint8_t>(BatteryChemistry::LiFe)));
    rxBatterySettings_.cellCount = preferences.getUChar("cells", 2);
    rxBatterySettings_.lowAlarmDecivolts = preferences.getUShort("low_dv", 60);
    rxBatterySettings_.alarmEnabled = preferences.getBool("alarm_en", true);
    preferences.end();

    if (static_cast<uint8_t>(rxBatterySettings_.chemistry) >
        static_cast<uint8_t>(BatteryChemistry::LiIon)) {
        rxBatterySettings_.chemistry = BatteryChemistry::LiFe;
    }
    rxBatterySettings_.cellCount = constrain(rxBatterySettings_.cellCount, 1, 12);
    rxBatterySettings_.lowAlarmDecivolts = constrain(
        rxBatterySettings_.lowAlarmDecivolts,
        kBatteryMinimumAlarmDecivolts, kBatteryMaximumAlarmDecivolts);
}

void Channel2Ui::saveRxBatterySettings()
{
    Preferences preferences;
    preferences.begin("jr-r1-rxbat", false);
    preferences.putUChar("chem", static_cast<uint8_t>(rxBatterySettings_.chemistry));
    preferences.putUChar("cells", rxBatterySettings_.cellCount);
    preferences.putUShort("low_dv", rxBatterySettings_.lowAlarmDecivolts);
    preferences.putBool("alarm_en", rxBatterySettings_.alarmEnabled);
    preferences.end();
}

void Channel2Ui::loadDriveModeSettings()
{
    Preferences preferences;
    preferences.begin("jr-r1-drive", true);
    driveModeSettings_.walkLimitPercent = preferences.getUChar("walk_limit", 35);
    preferences.end();
    driveModeSettings_.walkLimitPercent = constrain(
        driveModeSettings_.walkLimitPercent, 10, 80);
    // A power cycle is always a deliberate throttle cut, regardless of the
    // mode that was active before shutdown.
    driveModeSettings_.activeMode = DriveMode::Cut;
}

void Channel2Ui::saveDriveModeSettings()
{
    Preferences preferences;
    preferences.begin("jr-r1-drive", false);
    preferences.putUChar("walk_limit", driveModeSettings_.walkLimitPercent);
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

void Channel2Ui::sleepDisplay()
{
    if (!displayReady_ || displaySleeping_) return;
    displayBus->beginWrite();
    displayBus->writeCommand(0x28); // display off
    displayBus->writeCommand(0x10); // sleep in
    displayBus->endWrite();
    displaySleeping_ = true;
    Serial.println("DISPLAY: sleeping (touch to wake)");
}

void Channel2Ui::wakeDisplay()
{
    if (!displayReady_ || !displaySleeping_) return;
    displayBus->beginWrite();
    displayBus->writeCommand(0x11); // sleep out
    displayBus->endWrite();
    delay(120);
    displayBus->beginWrite();
    displayBus->writeCommand(0x29); // display on
    displayBus->endWrite();
    displaySleeping_ = false;
    lastInteractionMs_ = millis();
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(nullptr);
    Serial.println("DISPLAY: awake");
}

void Channel2Ui::redrawActivePage()
{
    switch (activePage_) {
        case UiPage::Dashboard: drawDashboard(); break;
        case UiPage::MainMenu: drawMainMenu(); break;
        case UiPage::Channels: drawChannelsPage(); break;
        case UiPage::ChannelRoles: drawChannelRolesPage(); break;
        case UiPage::Channel2: drawPage(); break;
        case UiPage::ThrottleResponse: drawThrottleResponsePage(); break;
        case UiPage::ChannelSpeed: drawChannelSpeedPage(); break;
        case UiPage::ControlAssignments: drawControlAssignmentsPage(); break;
        case UiPage::InputMonitor: drawInputMonitorPage(); break;
        case UiPage::DriveModes: drawDriveModesPage(); break;
        case UiPage::ChannelPlaceholder: drawChannelPlaceholder(); break;
        case UiPage::Setup: drawSetupPage(); break;
        case UiPage::TxBatterySetup: drawTxBatteryPage(); break;
        case UiPage::RxBatterySetup: drawRxBatteryPage(); break;
        case UiPage::DisplaySetup: drawDisplayPage(); break;
        case UiPage::Calibration: drawCalibrationPage(); break;
    }
}

int16_t Channel2Ui::mapAdcToPermille(uint16_t adc) const
{
    return triggerInput_.mapToPermille(adc);
}

int16_t Channel2Ui::applySettings(int16_t rawInputPermille) const
{
    // Keep physical trigger semantics independent of output direction:
    // negative input is always brake/reverse; positive is always forward.
    const bool braking = rawInputPermille < 0;
    const int8_t expo = braking ? settings_.brakeExpoPercent
                                : settings_.forwardExpoPercent;
    const uint8_t rate = braking ? settings_.brakeRatePercent
                                 : settings_.forwardRatePercent;
    const uint8_t endpoint = braking ? settings_.brakeEndpointPercent
                                     : settings_.forwardEndpointPercent;
    int32_t value = applyExpo(rawInputPermille, expo);
    value = value * rate / 100;
    value = value * endpoint / 100;
    if (settings_.reversed) value = -value;
    value += static_cast<int32_t>(settings_.subtrimPercent) * 10;
    return clampExtendedTravel(value);
}

int16_t Channel2Ui::applyDriveMode(int16_t configuredPermille) const
{
    if (driveModeSettings_.activeMode == DriveMode::Cut) return 0;
    if (driveModeSettings_.activeMode == DriveMode::Walk) {
        return static_cast<int16_t>(
            static_cast<int32_t>(configuredPermille) *
            driveModeSettings_.walkLimitPercent / 100);
    }
    return configuredPermille;
}

int16_t Channel2Ui::applyChannelSpeed(int16_t targetPermille, uint32_t nowMs)
{
    if (lastSlewUpdateMs_ == 0) {
        lastSlewUpdateMs_ = nowMs;
        slewedThrottlePermille_ = 0;
    }
    const uint32_t elapsedMs = min<uint32_t>(nowMs - lastSlewUpdateMs_, 100U);
    lastSlewUpdateMs_ = nowMs;

    const bool forwardSide = targetPermille > 0 ||
        (targetPermille == 0 && slewedThrottlePermille_ > 0);
    const uint16_t travelMs = forwardSide ? settings_.forwardSpeedMs
                                          : settings_.brakeSpeedMs;
    if (travelMs == 0 || elapsedMs == 0) {
        slewedThrottlePermille_ = targetPermille;
        return targetPermille;
    }

    const int32_t maximumStep = max<int32_t>(1,
        (1000L * static_cast<int32_t>(elapsedMs) + travelMs - 1) / travelMs);
    const int32_t delta = static_cast<int32_t>(targetPermille) -
                          slewedThrottlePermille_;
    slewedThrottlePermille_ += static_cast<int16_t>(
        constrain(delta, -maximumStep, maximumStep));
    return slewedThrottlePermille_;
}

void Channel2Ui::resetChannelSpeed()
{
    slewedThrottlePermille_ = 0;
    lastSlewUpdateMs_ = millis();
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
    dashboardSafetyLabel_ = makeLabel(link, "THROTTLE SAFE", 10, 61, 12, cAmber);
    makeLabel(link, "TAP TO ARM / CUT", 10, 74, 10, cDim);

    lv_obj_t *monitor = makePanel(8, 141, 464, 99);
    makeLabel(monitor, "CHANNEL MONITOR", 10, 6, 12, cCyan);
    for (uint8_t channel = 0; channel < 16; ++channel) {
        const int16_t x = 9 + channel * 28;
        char number[4];
        snprintf(number, sizeof(number), "%u", channel + 1);
        const bool assignedRole = channel + 1 == channelRoleSettings_.steeringChannel ||
                                  channel + 1 == channelRoleSettings_.throttleChannel;
        makeLabel(monitor, number, x + (channel < 9 ? 5 : 1), 21, 12,
                  assignedRole ? cWhite : cDim);
        lv_obj_t *bar = makeBar(monitor, x + 3, 37, 16, 44, 0, kCrsfMaximum,
                                assignedRole ? cCyan : cDim);
        lv_bar_set_value(bar, kCrsfMid, LV_ANIM_OFF);
        if (channel + 1 == channelRoleSettings_.throttleChannel)
            dashboardThrottleBar_ = bar;
    }

    makeButton(8, 249, 108, 62, "MENU", 18);
    makeButton(124, 249, 108, 62, "CHANNELS", 12, cPanel2, cCyan);
    makeButton(240, 249, 108, 62, "QUICK", 16, cPanel2, cCyan);
    const char *mode = driveModeSettings_.activeMode == DriveMode::Cut ? "CUT" :
                       driveModeSettings_.activeMode == DriveMode::Walk ? "WALK" : "NORMAL";
    makeButton(356, 249, 116, 62, mode, 14,
               driveModeSettings_.activeMode == DriveMode::Cut ? cAmber : cPanel2,
               driveModeSettings_.activeMode == DriveMode::Cut ? cBlack : cGreen);

    lastDisplayedCrsf_ = UINT16_MAX;
    lastSafetyStatus_ = OutputSafetyStatus::ControlStale;
    lastLinkValid_ = !lastLinkValid_;
    lastReceiverVoltageValid_ = !lastReceiverVoltageValid_;
    lastTxVoltageValid_ = !lastTxVoltageValid_;
}

void Channel2Ui::drawDashboardLive(uint16_t crsfValue,
                                   OutputSafetyStatus safetyStatus,
                                   const CrsfTelemetryState &telemetry,
                                   uint16_t txVoltageDecivolts,
                                   bool txVoltageValid)
{
    if (!displayReady_ || activePage_ != UiPage::Dashboard) return;
    if (dashboardThrottleBar_ && crsfValue != lastDisplayedCrsf_) {
        lv_bar_set_value(dashboardThrottleBar_, crsfValue, LV_ANIM_OFF);
        lastDisplayedCrsf_ = crsfValue;
    }
    if (dashboardSafetyLabel_ && safetyStatus != lastSafetyStatus_) {
        const char *text = "THROTTLE SAFE";
        uint32_t textColour = cAmber;
        switch (safetyStatus) {
            case OutputSafetyStatus::Armed: text = "OUTPUT LIVE"; textColour = cGreen; break;
            case OutputSafetyStatus::NeutralRequired: text = "RELEASE TO ARM"; break;
            case OutputSafetyStatus::Calibration: text = "CALIBRATION"; break;
            case OutputSafetyStatus::InputFault: text = "INPUT FAULT"; textColour = cRed; break;
            case OutputSafetyStatus::ControlStale: text = "CONTROL STALE"; textColour = cRed; break;
            case OutputSafetyStatus::ChannelDisabled: text = "CHANNEL OFF"; break;
            case OutputSafetyStatus::Cut: text = "THROTTLE CUT"; break;
        }
        lv_label_set_text(dashboardSafetyLabel_, text);
        lv_obj_set_style_text_color(dashboardSafetyLabel_, colour(textColour), 0);
        lastSafetyStatus_ = safetyStatus;
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
            const bool low = rxBatterySettings_.alarmEnabled &&
                telemetry.receiverVoltageDecivolts <= rxBatterySettings_.lowAlarmDecivolts;
            lv_label_set_text_fmt(dashboardRxVoltageLabel_, "RX  %u.%uV",
                telemetry.receiverVoltageDecivolts / 10,
                telemetry.receiverVoltageDecivolts % 10);
            lv_obj_set_style_text_color(dashboardRxVoltageLabel_,
                                        colour(low ? cRed : cGreen), 0);
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
        if (static_cast<int32_t>(now - nextTxLowBatteryAlarmMs_) >= 0) {
            playBuzzer(300);
            nextTxLowBatteryAlarmMs_ = now + kLowBatteryRepeatMs;
        }
    } else {
        nextTxLowBatteryAlarmMs_ = now;
    }
    if (rxBatterySettings_.alarmEnabled && telemetry.receiverVoltageValid &&
        telemetry.receiverVoltageDecivolts <= rxBatterySettings_.lowAlarmDecivolts) {
        if (static_cast<int32_t>(now - nextRxLowBatteryAlarmMs_) >= 0) {
            playBuzzer(300);
            nextRxLowBatteryAlarmMs_ = now + kLowBatteryRepeatMs;
        }
    } else {
        nextRxLowBatteryAlarmMs_ = now;
    }
}

void Channel2Ui::drawMainMenu()
{
    activePage_ = UiPage::MainMenu;
    startScreen();
    resetLvglObjects();
    makeHeader("JR R1 CONTROLLER", "HOME");
    static const char *labels[] = {"MODELS", "CHANNELS", "RESPONSE", "CONTROL MAP",
        "DRIVE MODES", "TIMERS", "LINK / TELEM", "SETUP"};
    for (uint8_t index = 0; index < 8; ++index) {
        const bool enabled = index == 1 || index == 2 || index == 3 ||
                             index == 4 || index == 7;
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
    makeButton(282, 5, 88, 34, "ROLES", 12, cPanel2, cCyan);
    const uint8_t first = channelsBank_ * 8 + 1;
    for (uint8_t index = 0; index < 8; ++index) {
        const uint8_t channel = first + index;
        char label[24];
        if (channel == channelRoleSettings_.steeringChannel)
            snprintf(label, sizeof(label), "CH%u  STEERING", channel);
        else if (channel == channelRoleSettings_.throttleChannel)
            snprintf(label, sizeof(label), "CH%u  THROTTLE", channel);
        else snprintf(label, sizeof(label), "CH%u  AUX", channel);
        makeButton(10 + (index % 2) * 235, 52 + (index / 2) * 52, 225, 44,
                   label, 16,
                   channel == channelRoleSettings_.throttleChannel ? cPanel2 : cPanel,
                   channel == channelRoleSettings_.throttleChannel ? cCyan : cWhite);
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

void Channel2Ui::drawChannelRoleRow(int row)
{
    const int16_t y = 72 + row * 68;
    lv_obj_t *panel = makePanel(18, y, 444, 56);
    makeLabel(panel, row == 0 ? "STEERING" : "THROTTLE", 14, 16, 18, cCyan);
    const uint8_t channel = row == 0 ? channelRoleSettings_.steeringChannel
                                     : channelRoleSettings_.throttleChannel;
    char value[12];
    snprintf(value, sizeof(value), "CH%u", channel);
    makeLabel(panel, value, 242, 16, 18, cWhite);
    makeButton(350, y + 8, 44, 40, "-", 22);
    makeButton(404, y + 8, 44, 40, "+", 22);
}

void Channel2Ui::drawControlAssignmentsPage()
{
    activePage_ = UiPage::ControlAssignments;
    startScreen();
    resetLvglObjects();
    makeHeader("CONTROL ASSIGNMENTS", "BACK");

    const char *inputs[] = {"STEERING WHEEL", "THROTTLE TRIGGER"};
    const bool enabled[] = {channelRoleSettings_.steeringEnabled,
                            channelRoleSettings_.throttleEnabled};
    const uint8_t channels[] = {channelRoleSettings_.steeringChannel,
                                channelRoleSettings_.throttleChannel};
    for (uint8_t row = 0; row < 2; ++row) {
        const int16_t y = 58 + row * 92;
        lv_obj_t *panel = makePanel(10, y, 460, 80);
        makeLabel(panel, inputs[row], 12, 10, 16, cCyan);
        makeLabel(panel, row == 0 ? "PROPORTIONAL / STEERING" :
                                   "PROPORTIONAL / THROTTLE", 12, 39, 12, cDim);
        lv_obj_t *status = makeLabel(panel, enabled[row] ? "ON" : "OFF", 278, 12,
                                     16, enabled[row] ? cGreen : cAmber);
        (void)status;
        char channel[8];
        snprintf(channel, sizeof(channel), "CH%u", channels[row]);
        makeLabel(panel, channel, 278, 43, 16, cWhite);
        makeButton(340, y + 34, 34, 36, "-", 22);
        makeButton(380, y + 34, 34, 36, "+", 22);
        makeButton(420, y + 6, 40, 28, enabled[row] ? "OFF" : "ON", 11,
                   enabled[row] ? cAmber : cPanel2,
                   enabled[row] ? cBlack : cWhite);
    }
    makeButton(18, 252, 214, 54, "INPUT MONITOR", 16, cPanel2, cCyan);
    makeLabel(lv_scr_act(), "2 OF 19 INPUTS MAPPED", 258, 270, 12, cDim);
}

void Channel2Ui::drawInputMonitorPage()
{
    activePage_ = UiPage::InputMonitor;
    startScreen();
    resetLvglObjects();
    makeHeader("INPUT MONITOR", "BACK");

    constexpr uint8_t kRowsPerBank = 5;
    constexpr uint8_t kBankCount =
        (kPhysicalControlCount + kRowsPerBank - 1) / kRowsPerBank;
    const size_t first = inputMonitorBank_ * kRowsPerBank;
    for (uint8_t row = 0; row < kRowsPerBank; ++row) {
        const size_t index = first + row;
        if (index >= kPhysicalControlCount) break;
        const PhysicalControlDescriptor &control = physicalControlDescriptor(index);
        const int16_t y = 54 + row * 43;
        lv_obj_t *panel = makePanel(10, y, 460, 37);
        makeLabel(panel, control.name, 10, 5, 14, cWhite);
        makeLabel(panel, physicalControlKindName(control.kind), 210, 5, 12, cDim);
        makeLabel(panel, control.hardwareMapped ? "MAPPED" : "UNMAPPED",
                  365, 5, 12, control.hardwareMapped ? cGreen : cAmber);
    }

    makeButton(12, 274, 58, 34, "<", 22,
               inputMonitorBank_ == 0 ? cBlack : cPanel,
               inputMonitorBank_ == 0 ? cDim : cWhite);
    makeButton(410, 274, 58, 34, ">", 22,
               inputMonitorBank_ + 1 >= kBankCount ? cBlack : cPanel,
               inputMonitorBank_ + 1 >= kBankCount ? cDim : cWhite);
    char page[16];
    snprintf(page, sizeof(page), "%u / %u", inputMonitorBank_ + 1, kBankCount);
    lv_obj_t *pageLabel = makeLabel(lv_scr_act(), page, 0, 284, 14, cDim);
    lv_obj_align(pageLabel, LV_ALIGN_TOP_MID, 0, 284);
}

void Channel2Ui::drawChannelRolesPage()
{
    activePage_ = UiPage::ChannelRoles;
    startScreen();
    resetLvglObjects();
    makeHeader("CHANNEL ROLES", "BACK");
    drawChannelRoleRow(0);
    drawChannelRoleRow(1);
    lv_obj_t *note = makeLabel(lv_scr_act(),
        "Changing roles disarms throttle output.\n"
        "Steering stays centred until its pot is connected.",
        0, 222, 14, cAmber);
    lv_obj_set_style_text_align(note, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 222);
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
    static const char *labels[] = {"BRAKE/REV END", "SUB-TRIM", "FORWARD END", "DIRECTION"};
    const int16_t y = 56 + row * 50;
    lv_obj_t *panel = makePanel(10, y, 460, 44);
    makeLabel(panel, labels[row], 12, 12, 16, cCyan);
    char value[20];
    if (row == 0) snprintf(value, sizeof(value), "%u%%", settings_.brakeEndpointPercent);
    else if (row == 1) snprintf(value, sizeof(value), "%+d%%", settings_.subtrimPercent);
    else if (row == 2) snprintf(value, sizeof(value), "%u%%", settings_.forwardEndpointPercent);
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
    char title[24];
    snprintf(title, sizeof(title), "CH%u THROTTLE", channelRoleSettings_.throttleChannel);
    makeHeader(title);
    makeButton(214, 5, 76, 34, "RESP", 12, cPanel2, cCyan);
    makeButton(298, 5, 72, 34, "BACK", 12);
    makeButton(378, 5, 72, 34, "CAL", 12);
    for (int row = 0; row < 4; ++row) drawSettingRow(row);
    char channelLabel[16];
    snprintf(channelLabel, sizeof(channelLabel), "CH%u", channelRoleSettings_.throttleChannel);
    makeLabel(lv_scr_act(), channelLabel, 12, 283, 16, cCyan);
    channel2ValueLabel_ = makeLabel(lv_scr_act(), "992", 422, 283, 14, cGreen);
    channel2LiveBar_ = makeBar(lv_scr_act(), 60, 287, 350, 14, 0, kCrsfMaximum, cGreen);
    lv_bar_set_value(channel2LiveBar_, kCrsfMid, LV_ANIM_OFF);
    makeLabel(lv_scr_act(), settings_.reversed ? "FORWARD" : "BRAKE/REV", 60, 304, 12, cDim);
    makeLabel(lv_scr_act(), settings_.reversed ? "BRAKE/REV" : "FORWARD", 338, 304, 12, cDim);
    lastDisplayedCrsf_ = UINT16_MAX;
}

void Channel2Ui::drawThrottleResponseRow(int row)
{
    static const char *labels[] = {
        "FORWARD RATE", "FORWARD EXPO", "BRAKE/REV RATE", "BRAKE/REV EXPO"
    };
    const int16_t y = 56 + row * 50;
    lv_obj_t *panel = makePanel(10, y, 460, 44);
    makeLabel(panel, labels[row], 12, 12, 16, cCyan);
    char value[20];
    if (row == 0) snprintf(value, sizeof(value), "%u%%", settings_.forwardRatePercent);
    else if (row == 1) snprintf(value, sizeof(value), "%+d%%", settings_.forwardExpoPercent);
    else if (row == 2) snprintf(value, sizeof(value), "%u%%", settings_.brakeRatePercent);
    else snprintf(value, sizeof(value), "%+d%%", settings_.brakeExpoPercent);
    makeLabel(panel, value, 222, 12, 16, cWhite);

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

void Channel2Ui::drawThrottleResponsePage()
{
    activePage_ = UiPage::ThrottleResponse;
    startScreen();
    resetLvglObjects();
    makeHeader("THROTTLE RESPONSE", "BACK");
    makeButton(282, 5, 88, 34, "SPEED", 12, cPanel2, cCyan);
    for (int row = 0; row < 4; ++row) drawThrottleResponseRow(row);
    lv_obj_t *note = makeLabel(lv_scr_act(),
        "+EXPO = SOFTER AROUND NEUTRAL", 0, 280, 12, cDim);
    lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 280);
}

void Channel2Ui::drawChannelSpeedRow(int row)
{
    const int16_t y = 78 + row * 76;
    lv_obj_t *panel = makePanel(14, y, 452, 62);
    makeLabel(panel, row == 0 ? "FORWARD TRAVEL TIME" : "BRAKE/REV TRAVEL TIME",
              12, 10, 16, cCyan);
    const uint16_t value = row == 0 ? settings_.forwardSpeedMs : settings_.brakeSpeedMs;
    char text[20];
    if (value == 0) snprintf(text, sizeof(text), "DIRECT");
    else snprintf(text, sizeof(text), "%u ms", value);
    makeLabel(panel, text, 242, 10, 16, cWhite);
    makeButton(350, y + 20, 42, 34, "-", 22);
    makeButton(402, y + 20, 42, 34, "+", 22);
}

void Channel2Ui::drawChannelSpeedPage()
{
    activePage_ = UiPage::ChannelSpeed;
    startScreen();
    resetLvglObjects();
    makeHeader("THROTTLE CHANNEL SPEED", "BACK");
    drawChannelSpeedRow(0);
    drawChannelSpeedRow(1);
    lv_obj_t *note = makeLabel(lv_scr_act(),
        "0 ms = direct. Time is neutral-to-endpoint slew.", 0, 250, 14, cDim);
    lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 250);
}

void Channel2Ui::drawDriveModesPage()
{
    activePage_ = UiPage::DriveModes;
    startScreen();
    resetLvglObjects();
    makeHeader("DRIVE MODES", "BACK");
    const char *labels[] = {"CUT", "WALK", "NORMAL"};
    for (uint8_t index = 0; index < 3; ++index) {
        const DriveMode mode = static_cast<DriveMode>(index);
        const bool active = driveModeSettings_.activeMode == mode;
        makeButton(12 + index * 156, 64, 144, 62, labels[index], 18,
                   active ? (mode == DriveMode::Cut ? cAmber : cGreen) : cPanel2,
                   active ? cBlack : cWhite);
    }
    lv_obj_t *panel = makePanel(16, 150, 448, 66);
    makeLabel(panel, "WALK LIMIT", 14, 20, 18, cCyan);
    char value[12]; snprintf(value, sizeof(value), "%u%%", driveModeSettings_.walkLimitPercent);
    makeLabel(panel, value, 260, 20, 18);
    makeButton(358, 164, 42, 38, "-", 22);
    makeButton(410, 164, 42, 38, "+", 22);
    makeLabel(lv_scr_act(),
        "Changing mode cuts output. Select ARM on the dashboard\nafter releasing the trigger.",
        20, 242, 14, cAmber);
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
    makeButton(10, 265, 88, 44, "TEST", 14, cPanel2, cAmber);
    makeButton(106, 265, 110, 44, "DISPLAY", 13, cPanel2, cCyan);
    makeButton(224, 265, 116, 44, "TX BAT", 13, cPanel2, cCyan);
    makeButton(348, 265, 122, 44, "RX BAT", 13, cPanel2, cCyan);
}

void Channel2Ui::drawDisplayRow(int row)
{
    static const char *labels[] = {"SCREEN TIMEOUT", "UI BRIGHTNESS", "NIGHT MODE", "SLEEP NOW"};
    const int16_t y = 56 + row * 50;
    lv_obj_t *panel = makePanel(10, y, 460, 44);
    makeLabel(panel, labels[row], 12, 12, 16, cCyan);
    char value[24];
    if (row == 0) {
        if (systemSettings_.screenTimeoutSeconds == 0) snprintf(value, sizeof(value), "NEVER");
        else snprintf(value, sizeof(value), "%u sec", systemSettings_.screenTimeoutSeconds);
    } else if (row == 1) {
        snprintf(value, sizeof(value), "%u%%", systemSettings_.displayBrightnessPercent);
    } else if (row == 2) {
        snprintf(value, sizeof(value), "%s", systemSettings_.nightMode ? "ON" : "OFF");
    } else snprintf(value, sizeof(value), "DISPLAY OFF");
    makeLabel(panel, value, 220, 12, 16, row == 2 && systemSettings_.nightMode ? cAmber : cWhite);
    if (row < 2) {
        makeButton(353, y + 4, 48, 36, "-", 24);
        makeButton(411, y + 4, 48, 36, "+", 24);
    } else {
        makeButton(353, y + 4, 106, 36, row == 2 ? "TOGGLE" : "SLEEP", 13,
                   row == 3 ? cAmber : cPanel2,
                   row == 3 ? cBlack : cWhite);
    }
}

void Channel2Ui::drawDisplayPage()
{
    activePage_ = UiPage::DisplaySetup;
    startScreen();
    resetLvglObjects();
    makeHeader("DISPLAY", "BACK");
    for (int row = 0; row < 4; ++row) drawDisplayRow(row);
    makeLabel(lv_scr_act(), "UI dimming is visual; full backlight saving needs a BL switch.",
              20, 280, 12, cDim);
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

void Channel2Ui::drawRxBatteryRow(int row)
{
    static const char *labels[] = {"CHEMISTRY", "CELL COUNT", "LOW ALARM", "ALARM"};
    const int16_t y = 56 + row * 50;
    lv_obj_t *panel = makePanel(10, y, 460, 44);
    makeLabel(panel, labels[row], 12, 12, 16, cCyan);
    char value[24];
    if (row == 0) snprintf(value, sizeof(value), "%s", batteryChemistryName(rxBatterySettings_.chemistry));
    else if (row == 1) snprintf(value, sizeof(value), "%uS", rxBatterySettings_.cellCount);
    else if (row == 2) snprintf(value, sizeof(value), "%u.%u V",
        rxBatterySettings_.lowAlarmDecivolts / 10, rxBatterySettings_.lowAlarmDecivolts % 10);
    else snprintf(value, sizeof(value), "%s", rxBatterySettings_.alarmEnabled ? "ON" : "OFF");
    makeLabel(panel, value, 212, 12, 16,
              row == 3 && rxBatterySettings_.alarmEnabled ? cGreen : cWhite);

    if (row == 0 || row == 3) {
        const bool enabled = row == 3 && rxBatterySettings_.alarmEnabled;
        lv_obj_t *toggle = lv_obj_create(panel);
        position(toggle, 343, 3, 106, 36);
        lv_obj_set_style_bg_color(toggle, colour(enabled ? cGreen : cPanel2), 0);
        lv_obj_set_style_border_color(toggle, colour(cLine), 0);
        lv_obj_set_style_border_width(toggle, 1, 0);
        lv_obj_set_style_radius(toggle, 5, 0);
        lv_obj_set_style_pad_all(toggle, 0, 0);
        lv_obj_t *label = makeLabel(toggle, row == 0 ? "NEXT" : "TOGGLE", 0, 0, 14,
                                    enabled ? cBlack : cWhite);
        lv_obj_center(label);
    } else {
        makeButton(353, y + 4, 48, 36, "-", 24);
        makeButton(411, y + 4, 48, 36, "+", 24);
    }
}

void Channel2Ui::drawRxBatteryPage()
{
    activePage_ = UiPage::RxBatterySetup;
    startScreen();
    resetLvglObjects();
    makeHeader("RX BATTERY", "BACK");
    for (int row = 0; row < 4; ++row) drawRxBatteryRow(row);
    lv_obj_t *note = makeLabel(lv_scr_act(),
        "Voltage received from ER6G telemetry", 0, 280, 12, cDim);
    lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 280);
}

void Channel2Ui::drawCalibrationPage()
{
    startScreen();
    resetLvglObjects();
    makeHeader("THROTTLE CALIBRATION", "CANCEL");
    makeLabel(lv_scr_act(), "1. MOVE TO FULL BRAKE", 20, 62, 16, cCyan);
    makeLabel(lv_scr_act(), "2. MOVE TO FULL FORWARD", 20, 92, 16, cCyan);
    makeLabel(lv_scr_act(), "3. RELEASE AND HOLD CENTRE", 20, 122, 16, cCyan);
    char held[44];
    snprintf(held, sizeof(held), "CH%u OUTPUT HELD AT NEUTRAL",
             channelRoleSettings_.throttleChannel);
    makeLabel(lv_scr_act(), held, 20, 160, 16, cAmber);
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
    if (!triggerInput_.calibrationActive() || !calibrationLiveLabel_) return;
    const ProportionalCalibrationProgress progress =
        triggerInput_.calibrationProgress();
    lv_label_set_text_fmt(calibrationMinimumLabel_, "%4u", progress.minimum);
    lv_label_set_text_fmt(calibrationLiveLabel_, "%4u", adc);
    lv_label_set_text_fmt(calibrationMaximumLabel_, "%4u", progress.maximum);
    lv_obj_set_style_text_color(calibrationMinimumLabel_,
        colour(progress.sawMinimum ? cGreen : cWhite), 0);
    lv_obj_set_style_text_color(calibrationMaximumLabel_,
        colour(progress.sawMaximum ? cGreen : cWhite), 0);
}

void Channel2Ui::startCalibration(uint16_t adc)
{
    activePage_ = UiPage::Calibration;
    triggerInput_.startCalibration(adc);
    drawCalibrationPage();
    Serial.println("CAL: move proportional input to both ends, then release and hold neutral");
}

void Channel2Ui::cancelCalibration()
{
    triggerInput_.cancelCalibration();
    drawPage();
    Serial.println("CAL: cancelled; previous calibration retained");
}

void Channel2Ui::updateCalibration(uint16_t adc)
{
    if (!triggerInput_.updateCalibration(adc, millis())) return;
    const ProportionalCalibration &calibration = triggerInput_.calibration();
    saveCalibration();
    Serial.printf("CAL: saved min=%u centre=%u max=%u\n", calibration.minimum,
                  calibration.centre, calibration.maximum);
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
    else if (y >= 245 && x >= 236 && x < 352) { playKeyClick(); drawThrottleResponsePage(); }
    else if (y >= 245 && x >= 352) { playKeyClick(); drawDriveModesPage(); }
    else if (x >= 310 && y >= 45 && y < 140) {
        if (driveModeSettings_.activeMode == DriveMode::Cut ||
            !channelRoleSettings_.throttleEnabled) {
            playKeyClick();
            drawDriveModesPage();
        } else {
            armRequested_ = !armRequested_;
            safetyResetRequested_ = true;
            playBuzzer(armRequested_ ? 80 : 180);
            lastSafetyStatus_ = OutputSafetyStatus::ControlStale;
        }
    }
}

void Channel2Ui::handleMainMenuTouch(int16_t x, int16_t y)
{
    if (y < 48 && x >= 390) { playKeyClick(); drawDashboard(); return; }
    if (x < 5 || x >= 475 || y < 52 || y >= 270) return;
    const int item = ((y - 52) / 54) * 2 + (x >= 240 ? 1 : 0);
    playKeyClick();
    if (item == 1) drawChannelsPage();
    else if (item == 2) drawThrottleResponsePage();
    else if (item == 3) drawControlAssignmentsPage();
    else if (item == 4) drawDriveModesPage();
    else if (item == 7) drawSetupPage();
}

void Channel2Ui::handleChannelsTouch(int16_t x, int16_t y)
{
    if (y < 48 && x >= 275 && x < 375) {
        playKeyClick(); drawChannelRolesPage(); return;
    }
    if (y < 48 && x >= 375) { playKeyClick(); drawMainMenu(); return; }
    if (y >= 270) {
        if (x < 90 && channelsBank_ > 0) { --channelsBank_; playKeyClick(); drawChannelsPage(); }
        else if (x >= 390 && channelsBank_ < 1) { ++channelsBank_; playKeyClick(); drawChannelsPage(); }
        return;
    }
    if (y < 52 || y >= 264) return;
    selectedChannel_ = channelsBank_ * 8 + ((y - 52) / 52) * 2 + (x >= 240 ? 1 : 0) + 1;
    playKeyClick();
    if (selectedChannel_ == channelRoleSettings_.throttleChannel) drawPage();
    else drawChannelPlaceholder();
}

void Channel2Ui::handleChannelRolesTouch(int16_t x, int16_t y)
{
    if (y < 48 && x >= 360) { playKeyClick(); drawChannelsPage(); return; }
    if (y < 72 || y >= 196 || x < 340) return;
    const int row = y < 140 ? 0 : 1;
    const int8_t direction = x < 400 ? -1 : 1;
    if (row == 0) {
        channelRoleSettings_.steeringChannel = adjacentChannel(
            channelRoleSettings_.steeringChannel, direction,
            channelRoleSettings_.throttleChannel);
    } else {
        channelRoleSettings_.throttleChannel = adjacentChannel(
            channelRoleSettings_.throttleChannel, direction,
            channelRoleSettings_.steeringChannel);
    }
    channelRolesChanged_ = true;
    safetyResetRequested_ = true;
    armRequested_ = false;
    saveChannelRoleSettings();
    playKeyClick();
    drawChannelRolesPage();
}

void Channel2Ui::handleThrottleResponseTouch(int16_t x, int16_t y)
{
    if (y < 48 && x >= 375) { playKeyClick(); drawPage(); return; }
    if (y < 48 && x >= 275 && x < 375) { playKeyClick(); drawChannelSpeedPage(); return; }
    if (y < 56 || y >= 256 || x < 340) return;
    const int row = (y - 56) / 50;
    const int8_t direction = x < 400 ? -1 : 1;
    bool changed = false;
    if (row == 0) {
        const int value = settings_.forwardRatePercent + direction * kResponseRateStepPercent;
        settings_.forwardRatePercent = constrain(value, 0, 100); changed = true;
    } else if (row == 1) {
        const int value = settings_.forwardExpoPercent + direction * kResponseExpoStepPercent;
        settings_.forwardExpoPercent = constrain(value, -100, 100); changed = true;
    } else if (row == 2) {
        const int value = settings_.brakeRatePercent + direction * kResponseRateStepPercent;
        settings_.brakeRatePercent = constrain(value, 0, 100); changed = true;
    } else if (row == 3) {
        const int value = settings_.brakeExpoPercent + direction * kResponseExpoStepPercent;
        settings_.brakeExpoPercent = constrain(value, -100, 100); changed = true;
    }
    if (changed) { saveSettings(); playKeyClick(); drawThrottleResponsePage(); }
}

void Channel2Ui::handleChannelSpeedTouch(int16_t x, int16_t y)
{
    if (y < 48 && x >= 360) { playKeyClick(); drawThrottleResponsePage(); return; }
    if (x < 340 || y < 78 || y >= 230) return;
    const uint8_t row = (y - 78) / 76;
    const int16_t direction = x < 400 ? -1 : 1;
    uint16_t &value = row == 0 ? settings_.forwardSpeedMs : settings_.brakeSpeedMs;
    const int32_t changed = static_cast<int32_t>(value) +
                            direction * kChannelSpeedStepMs;
    value = constrain(changed, 0L, static_cast<long>(kChannelSpeedMaximumMs));
    saveSettings();
    playKeyClick();
    drawChannelSpeedPage();
}

void Channel2Ui::handleControlAssignmentsTouch(int16_t x, int16_t y)
{
    if (y < 48 && x >= 360) { playKeyClick(); drawMainMenu(); return; }
    if (y >= 248 && x < 245) {
        playKeyClick();
        inputMonitorBank_ = 0;
        drawInputMonitorPage();
        return;
    }
    if (y < 58 || y >= 242) return;
    const uint8_t row = (y - 58) / 92;
    const int16_t localY = y - (58 + row * 92);
    if (x >= 415 && localY < 38) {
        if (row == 0) channelRoleSettings_.steeringEnabled =
            !channelRoleSettings_.steeringEnabled;
        else channelRoleSettings_.throttleEnabled =
            !channelRoleSettings_.throttleEnabled;
    } else if (x >= 335 && localY >= 28) {
        const int8_t direction = x < 378 ? -1 : 1;
        if (row == 0) channelRoleSettings_.steeringChannel = adjacentChannel(
            channelRoleSettings_.steeringChannel, direction,
            channelRoleSettings_.throttleChannel);
        else channelRoleSettings_.throttleChannel = adjacentChannel(
            channelRoleSettings_.throttleChannel, direction,
            channelRoleSettings_.steeringChannel);
    } else return;
    channelRolesChanged_ = true;
    safetyResetRequested_ = true;
    armRequested_ = false;
    saveChannelRoleSettings();
    playKeyClick();
    drawControlAssignmentsPage();
}

void Channel2Ui::handleInputMonitorTouch(int16_t x, int16_t y)
{
    constexpr uint8_t kRowsPerBank = 5;
    constexpr uint8_t kBankCount =
        (kPhysicalControlCount + kRowsPerBank - 1) / kRowsPerBank;
    if (y < 48 && x >= 360) {
        playKeyClick(); drawControlAssignmentsPage(); return;
    }
    if (y < 270) return;
    if (x < 90 && inputMonitorBank_ > 0) {
        --inputMonitorBank_;
        playKeyClick();
        drawInputMonitorPage();
    } else if (x >= 390 && inputMonitorBank_ + 1 < kBankCount) {
        ++inputMonitorBank_;
        playKeyClick();
        drawInputMonitorPage();
    }
}

void Channel2Ui::handleDriveModesTouch(int16_t x, int16_t y)
{
    if (y < 48 && x >= 360) { playKeyClick(); drawMainMenu(); return; }
    if (y >= 60 && y < 132) {
        const uint8_t index = constrain(x / 156, 0, 2);
        driveModeSettings_.activeMode = static_cast<DriveMode>(index);
        armRequested_ = false;
        safetyResetRequested_ = true;
        playBuzzer(index == 0 ? 180 : 80);
        drawDriveModesPage();
        return;
    }
    if (y >= 150 && y < 220 && x >= 345) {
        const int16_t direction = x < 405 ? -1 : 1;
        driveModeSettings_.walkLimitPercent = constrain(
            static_cast<int>(driveModeSettings_.walkLimitPercent) + direction * 5,
            10, 80);
        saveDriveModeSettings();
        playKeyClick();
        drawDriveModesPage();
    }
}

void Channel2Ui::handleSetupTouch(int16_t x, int16_t y)
{
    if (y < 48 && x >= 360) { playKeyClick(); drawMainMenu(); return; }
    if (y >= 265 && y < 315 && x < 102) { playBuzzer(500); return; }
    if (y >= 265 && y < 315 && x >= 102 && x < 220) {
        playKeyClick(); drawDisplayPage(); return;
    }
    if (y >= 265 && y < 315 && x >= 220 && x < 344) {
        playKeyClick(); drawTxBatteryPage(); return;
    }
    if (y >= 265 && y < 315 && x >= 344) {
        playKeyClick(); drawRxBatteryPage(); return;
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

void Channel2Ui::handleDisplayTouch(int16_t x, int16_t y)
{
    if (y < 48 && x >= 360) { playKeyClick(); drawSetupPage(); return; }
    if (y < 56 || y >= 256 || x < 340) return;
    const uint8_t row = (y - 56) / 50;
    const int8_t direction = x < 405 ? -1 : 1;
    if (row == 0) {
        static const uint16_t values[] = {0, 30, 60, 300};
        uint8_t index = 0;
        while (index < 4 && values[index] != systemSettings_.screenTimeoutSeconds) ++index;
        if (index >= 4) index = 2;
        index = static_cast<uint8_t>((index + (direction > 0 ? 1 : 3)) % 4);
        systemSettings_.screenTimeoutSeconds = values[index];
    } else if (row == 1) {
        systemSettings_.displayBrightnessPercent = constrain(
            static_cast<int>(systemSettings_.displayBrightnessPercent) + direction * 10,
            20, 100);
        uiBrightnessPercent = systemSettings_.displayBrightnessPercent;
    } else if (row == 2) {
        systemSettings_.nightMode = !systemSettings_.nightMode;
        uiNightMode = systemSettings_.nightMode;
    } else {
        playKeyClick();
        sleepDisplay();
        return;
    }
    saveSystemSettings();
    playKeyClick();
    drawDisplayPage();
}

void Channel2Ui::handleTxBatteryTouch(int16_t x, int16_t y)
{
    if (y < 48 && x >= 360) { playKeyClick(); drawSetupPage(); return; }
    if (y < 56 || y >= 256) return;
    const int row = (y - 56) / 50;
    bool changed = false;

    if (row == 0 && x >= 340) {
        uint8_t chemistry = static_cast<uint8_t>(txBatterySettings_.chemistry);
        chemistry = (chemistry + 1) % 4;
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
               txBatterySettings_.lowAlarmDecivolts > kBatteryMinimumAlarmDecivolts) {
        --txBatterySettings_.lowAlarmDecivolts; changed = true;
    } else if (row == 2 && x >= 400 &&
               txBatterySettings_.lowAlarmDecivolts < kBatteryMaximumAlarmDecivolts) {
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

void Channel2Ui::handleRxBatteryTouch(int16_t x, int16_t y)
{
    if (y < 48 && x >= 360) { playKeyClick(); drawSetupPage(); return; }
    if (y < 56 || y >= 256) return;
    const int row = (y - 56) / 50;
    bool changed = false;

    if (row == 0 && x >= 340) {
        const uint8_t chemistry = static_cast<uint8_t>(rxBatterySettings_.chemistry);
        rxBatterySettings_.chemistry = static_cast<BatteryChemistry>((chemistry + 1) % 4);
        rxBatterySettings_.cellCount = min(rxBatterySettings_.cellCount,
            maximumCellsFor(rxBatterySettings_.chemistry));
        rxBatterySettings_.lowAlarmDecivolts = defaultLowAlarmDecivolts(
            rxBatterySettings_.chemistry, rxBatterySettings_.cellCount);
        changed = true;
    } else if (row == 1 && x >= 343 && x < 405 && rxBatterySettings_.cellCount > 1) {
        --rxBatterySettings_.cellCount;
        rxBatterySettings_.lowAlarmDecivolts = defaultLowAlarmDecivolts(
            rxBatterySettings_.chemistry, rxBatterySettings_.cellCount);
        changed = true;
    } else if (row == 1 && x >= 405 &&
               rxBatterySettings_.cellCount < maximumCellsFor(rxBatterySettings_.chemistry)) {
        ++rxBatterySettings_.cellCount;
        rxBatterySettings_.lowAlarmDecivolts = defaultLowAlarmDecivolts(
            rxBatterySettings_.chemistry, rxBatterySettings_.cellCount);
        changed = true;
    } else if (row == 2 && x >= 343 && x < 405 &&
               rxBatterySettings_.lowAlarmDecivolts > kBatteryMinimumAlarmDecivolts) {
        --rxBatterySettings_.lowAlarmDecivolts; changed = true;
    } else if (row == 2 && x >= 405 &&
               rxBatterySettings_.lowAlarmDecivolts < kBatteryMaximumAlarmDecivolts) {
        ++rxBatterySettings_.lowAlarmDecivolts; changed = true;
    } else if (row == 3 && x >= 340) {
        rxBatterySettings_.alarmEnabled = !rxBatterySettings_.alarmEnabled;
        changed = true;
    }

    if (changed) {
        saveRxBatterySettings();
        playKeyClick();
        drawRxBatteryPage();
    }
}

void Channel2Ui::handleTouch(int16_t x, int16_t y)
{
    if (triggerInput_.calibrationActive()) { if (y < 48 && x >= 360) cancelCalibration(); return; }
    if (activePage_ == UiPage::Dashboard) { handleDashboardTouch(x, y); return; }
    if (activePage_ == UiPage::MainMenu) { handleMainMenuTouch(x, y); return; }
    if (activePage_ == UiPage::Channels) { handleChannelsTouch(x, y); return; }
    if (activePage_ == UiPage::ChannelRoles) { handleChannelRolesTouch(x, y); return; }
    if (activePage_ == UiPage::ThrottleResponse) { handleThrottleResponseTouch(x, y); return; }
    if (activePage_ == UiPage::ChannelSpeed) { handleChannelSpeedTouch(x, y); return; }
    if (activePage_ == UiPage::ControlAssignments) { handleControlAssignmentsTouch(x, y); return; }
    if (activePage_ == UiPage::InputMonitor) { handleInputMonitorTouch(x, y); return; }
    if (activePage_ == UiPage::DriveModes) { handleDriveModesTouch(x, y); return; }
    if (activePage_ == UiPage::ChannelPlaceholder) { if (y < 48 && x >= 360) { playKeyClick(); drawChannelsPage(); } return; }
    if (activePage_ == UiPage::Setup) { handleSetupTouch(x, y); return; }
    if (activePage_ == UiPage::TxBatterySetup) { handleTxBatteryTouch(x, y); return; }
    if (activePage_ == UiPage::RxBatterySetup) { handleRxBatteryTouch(x, y); return; }
    if (activePage_ == UiPage::DisplaySetup) { handleDisplayTouch(x, y); return; }
    if (y < 48 && x >= 210 && x < 294) { playKeyClick(); drawThrottleResponsePage(); return; }
    if (y < 48 && x >= 294 && x < 374) { playKeyClick(); drawChannelsPage(); return; }
    if (y < 48 && x >= 374) { playKeyClick(); startCalibration(latestRawAdc_); return; }
    if (y < 56 || y >= 256) return;
    const int row = (y - 56) / 50;
    bool changed = false;
    if (row < 3 && x >= 354 && x < 404) {
        if (row == 0 && settings_.brakeEndpointPercent > 50) --settings_.brakeEndpointPercent;
        if (row == 1 && settings_.subtrimPercent > -20) --settings_.subtrimPercent;
        if (row == 2 && settings_.forwardEndpointPercent > 50) --settings_.forwardEndpointPercent;
        changed = true;
    } else if (row < 3 && x >= 408 && x < 470) {
        if (row == 0 && settings_.brakeEndpointPercent < 100) ++settings_.brakeEndpointPercent;
        if (row == 1 && settings_.subtrimPercent < 20) ++settings_.subtrimPercent;
        if (row == 2 && settings_.forwardEndpointPercent < 100) ++settings_.forwardEndpointPercent;
        changed = true;
    } else if (row == 3 && x >= 340) { settings_.reversed = !settings_.reversed; changed = true; }
    if (changed) { playKeyClick(); saveSettings(); drawPage(); }
}

void Channel2Ui::updateTouch()
{
    int16_t x = 0, y = 0;
    uint16_t rawX = 0, rawY = 0, rawZ = 0;
    const uint32_t now = millis();
    const bool irqActive = digitalRead(TOUCH_IRQ_PIN) == LOW;
    if (displaySleeping_) {
        if (irqActive) {
            wakeDisplay();
            discardWakeTouchUntilRelease_ = true;
            touchWasDown_ = false;
            touchReleaseSinceMs_ = 0;
            touchIgnoreUntilMs_ = now + 250;
        }
        return;
    }
    if (discardWakeTouchUntilRelease_) {
        if (!irqActive) {
            discardWakeTouchUntilRelease_ = false;
            touchIgnoreUntilMs_ = now + kTouchRetriggerGuardMs;
        }
        return;
    }
    const bool touched = readTouch(x, y, rawX, rawY, rawZ);

    if (touched) {
        // A low-pressure or failed I2C sample while IRQ remains asserted is not
        // a release. Clear the pending-release timer whenever valid pressure
        // returns and keep treating this as the same physical touch.
        touchReleaseSinceMs_ = 0;
        if (!touchWasDown_ &&
            static_cast<int32_t>(now - touchIgnoreUntilMs_) >= 0) {
            touchWasDown_ = true;
            Serial.printf("TOUCH down page=%s raw=(%u,%u,%u) screen=(%d,%d)\n",
                          uiPageName(activePage_), rawX, rawY, rawZ, x, y);
        }
        if (touchWasDown_) {
            lastTouchX_ = x;
            lastTouchY_ = y;
        }
        return;
    }

    if (!touchWasDown_) return;
    if (irqActive) {
        // Pressure conversion can momentarily fail while the controller's IRQ
        // still proves the panel is being touched.
        touchReleaseSinceMs_ = 0;
        return;
    }

    if (touchReleaseSinceMs_ == 0) {
        touchReleaseSinceMs_ = now;
        return;
    }
    if (now - touchReleaseSinceMs_ < kTouchReleaseDebounceMs) return;

    const int16_t releasedX = lastTouchX_;
    const int16_t releasedY = lastTouchY_;
    touchWasDown_ = false;
    touchReleaseSinceMs_ = 0;
    touchIgnoreUntilMs_ = now + kTouchRetriggerGuardMs;
    lastInteractionMs_ = now;
    Serial.printf("TOUCH up page=%s screen=(%d,%d)\n",
                  uiPageName(activePage_), releasedX, releasedY);
    handleTouch(releasedX, releasedY);
}

void Channel2Ui::update(uint16_t rawAdc, int16_t, int16_t outputPermille,
                        uint16_t crsfValue, OutputSafetyStatus safetyStatus,
                        const CrsfTelemetryState &telemetry,
                        uint16_t txVoltageDecivolts, bool txVoltageValid)
{
    latestRawAdc_ = rawAdc;
    updateTouch();
    if (triggerInput_.calibrationActive()) updateCalibration(rawAdc);

    const uint32_t now = millis();
    if (!displaySleeping_ && systemSettings_.screenTimeoutSeconds != 0 &&
        now - lastInteractionMs_ >=
            static_cast<uint32_t>(systemSettings_.screenTimeoutSeconds) * 1000U) {
        sleepDisplay();
    }
    if (static_cast<int32_t>(now - nextDisplayUpdateMs_) >= 0) {
        nextDisplayUpdateMs_ = now + kUiUpdateMs;
        if (triggerInput_.calibrationActive()) drawCalibrationValues(rawAdc);
        else if (activePage_ == UiPage::Dashboard) {
            drawDashboardLive(crsfValue, safetyStatus, telemetry,
                              txVoltageDecivolts, txVoltageValid);
        }
        else if (activePage_ == UiPage::Channel2) drawLiveIndicator(outputPermille, crsfValue);
    }
    lv_timer_handler();
}
