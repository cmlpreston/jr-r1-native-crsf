#include "channel2_ui.h"

#include <Preferences.h>
#ifndef JR_R1_SIMULATOR
#include <Arduino_GFX_Library.h>
#include <SPI.h>
#include <Wire.h>
#else
#include "simulator_platform.h"
#endif

#include "config.h"
#include "binding_resolver.h"
#include "r1_input_map.h"

LV_IMG_DECLARE(hyper7_icon);

namespace {

constexpr int16_t kScreenWidth = DISPLAY_LANDSCAPE_WIDTH;
constexpr int16_t kScreenHeight = DISPLAY_LANDSCAPE_HEIGHT;
constexpr uint16_t kCrsfMid = 992;
// Standard CRSF/servo travel. Values 0..2047 are representable, but 172..1811
// maps to the conventional approximately 988..2012 us receiver PWM range.
// UI bars therefore use the standard travel so full trigger looks full-scale.
constexpr uint16_t kCrsfMinimum = 172;
constexpr uint16_t kCrsfMaximum = 1811;
constexpr int16_t kExtendedTravelPermille = 1200;
constexpr uint32_t kUiUpdateMs = 80;
constexpr uint16_t kTouchPressureThreshold = 40;
constexpr uint32_t kTouchReleaseDebounceMs = 25;
constexpr uint32_t kTouchRetriggerGuardMs = 100;
constexpr uint32_t kDisplayWakeArmMs = 250;
constexpr uint32_t kDisplayWakePollMs = 20;
constexpr uint32_t kSettingsSaveDelayMs = 750;
constexpr uint16_t kBindingProfileVersion = 1;
constexpr uint16_t kChannelParameterVersion = 1;
constexpr int16_t kChannelParameterLimitPermille = 200;
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
constexpr uint32_t kScreenshotSpiFrequency = 4000000;
constexpr uint8_t kLcdColumnAddressSet = 0x2a;
constexpr uint8_t kLcdPageAddressSet = 0x2b;
constexpr uint8_t kLcdMemoryRead = 0x2e;

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
constexpr uint32_t cSafetyCut = 0x402608;
constexpr uint32_t cSafetyArmed = 0x103b20;
constexpr uint32_t cSafetyWaiting = 0x3b3309;
constexpr uint32_t cSafetyFault = 0x421416;

uint8_t uiBrightnessPercent = 100;
bool uiNightMode = false;

constexpr TransmitterFunction kEditableTransmitterFunctions[] = {
    TransmitterFunction::OpenMenu,
    TransmitterFunction::Dashboard,
    TransmitterFunction::NextPage,
    TransmitterFunction::PreviousPage,
    TransmitterFunction::QuickPanel,
    TransmitterFunction::DisplayOnOff,
    TransmitterFunction::ToggleNightMode,
    TransmitterFunction::ArmCut,
    TransmitterFunction::MenuPrevious,
    TransmitterFunction::MenuNext,
    TransmitterFunction::MenuSelect,
    TransmitterFunction::MenuBack,
    TransmitterFunction::MenuDecrease,
    TransmitterFunction::MenuIncrease,
};

constexpr DriveModeSelection kEditableDriveModeSelections[] = {
    DriveModeSelection::WalkNormal,
    DriveModeSelection::CutArm,
};

constexpr bool mainMenuItemEnabled(uint8_t index)
{
    return index == 1 || index == 2 || index == 3 || index == 4 ||
           index == 6 || index == 7;
}

#ifndef JR_R1_SIMULATOR
Arduino_DataBus *displayBus = new Arduino_ESP32SPI(
    TFT_DC_PIN, TFT_CS_PIN, TFT_SCK_PIN, TFT_MOSI_PIN, TFT_MISO_PIN);
#if DISPLAY_PROFILE == DISPLAY_PROFILE_WAVESHARE_RPI_35F
Arduino_GFX *display = new Arduino_ST7796(
    displayBus, TFT_RESET_PIN, 3, true, 320, 480);
#else
Arduino_GFX *display = new Arduino_ILI9486_18bit(
    displayBus, GFX_NOT_DEFINED, 1, false);
#endif
SPIClass screenshotSpi(FSPI);

lv_disp_draw_buf_t drawBuffer;
lv_disp_drv_t displayDriver;
lv_color_t lineBuffer[kScreenWidth * 16];
uint8_t screenshotRow[kScreenWidth * 3];

void screenshotCommand(uint8_t command)
{
    digitalWrite(TFT_DC_PIN, LOW);
    screenshotSpi.transfer(command);
    digitalWrite(TFT_DC_PIN, HIGH);
}

void screenshotCoordinate(uint16_t start, uint16_t end)
{
    screenshotSpi.transfer(static_cast<uint8_t>(start >> 8));
    screenshotSpi.transfer(static_cast<uint8_t>(start));
    screenshotSpi.transfer(static_cast<uint8_t>(end >> 8));
    screenshotSpi.transfer(static_cast<uint8_t>(end));
}
#else
lv_disp_draw_buf_t drawBuffer;
lv_disp_drv_t displayDriver;
lv_color_t lineBuffer[kScreenWidth * 16];
#endif

lv_color_t colour(uint32_t rgb)
{
    // The BTT panel has no exposed backlight input, so its brightness setting
    // is approximated by scaling rendered colours. The Waveshare profile uses
    // real backlight PWM and keeps the full RGB range here.
    uint16_t scale = DISPLAY_HAS_PWM_BACKLIGHT ? 100 : uiBrightnessPercent;
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
#ifdef JR_R1_SIMULATOR
    simulatorFlush(area, pixels);
#else
    const int16_t width = area->x2 - area->x1 + 1;
    const int16_t height = area->y2 - area->y1 + 1;
    display->draw16bitRGBBitmap(area->x1, area->y1,
                                reinterpret_cast<uint16_t *>(pixels),
                                width, height);
#endif
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
    if (action != nullptr) makeButton(344, 5, 78, 34, action, 12);
    makeButton(430, 5, 42, 34, LV_SYMBOL_HOME, 18);
}

bool headerActionTouched(int16_t x, int16_t y)
{
    return y < 48 && x >= 340 && x < 428;
}

bool headerHomeTouched(int16_t x, int16_t y)
{
    return y < 48 && x >= 428;
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

void formatUid(const uint8_t *uid, char *text, size_t capacity)
{
    snprintf(text, capacity, "%02X:%02X:%02X:%02X:%02X:%02X",
             uid[0], uid[1], uid[2], uid[3], uid[4], uid[5]);
}

const char *uiPageName(UiPage page)
{
    switch (page) {
        case UiPage::Dashboard: return "dashboard";
        case UiPage::Telemetry: return "telemetry";
        case UiPage::MainMenu: return "main-menu";
        case UiPage::Channels: return "channels";
        case UiPage::ChannelRoles: return "channel-roles";
        case UiPage::Channel2: return "channel";
        case UiPage::ThrottleResponse: return "response";
        case UiPage::ChannelSpeed: return "channel-speed";
        case UiPage::ControlAssignments: return "control-map";
        case UiPage::BindingEditor: return "binding-editor";
        case UiPage::InputMonitor: return "input-monitor";
        case UiPage::DriveModes: return "drive-modes";
        case UiPage::ChannelPlaceholder: return "channel-placeholder";
        case UiPage::Setup: return "setup";
        case UiPage::TxBatterySetup: return "tx-battery";
        case UiPage::RxBatterySetup: return "rx-battery";
        case UiPage::DisplaySetup: return "display";
        case UiPage::ElrsBinding: return "elrs-binding";
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
    loadElrsSettings();
    loadChannelParameters();
    loadBindings();
    uiBrightnessPercent = systemSettings_.displayBrightnessPercent;
    uiNightMode = systemSettings_.nightMode;
#ifndef JR_R1_SIMULATOR
    setToneChannel(BUZZER_LEDC_CHANNEL);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);
    Wire.setClock(400000);
    const bool touchReady = initialiseTouchController();

    if (DISPLAY_HAS_PWM_BACKLIGHT) {
        ledcSetup(TFT_BACKLIGHT_LEDC_CHANNEL, TFT_BACKLIGHT_PWM_FREQUENCY,
                  TFT_BACKLIGHT_PWM_BITS);
        ledcAttachPin(TFT_BACKLIGHT_PIN, TFT_BACKLIGHT_LEDC_CHANNEL);
        ledcWrite(TFT_BACKLIGHT_LEDC_CHANNEL, 0);
    }

    // Initialise the shared hardware SPI handle before Arduino_GFX configures
    // the same FSPI peripheral. It is used only for explicit, synchronous LCD
    // GRAM reads while LVGL is idle.
    screenshotSpi.begin(TFT_SCK_PIN, TFT_MISO_PIN, TFT_MOSI_PIN, TFT_CS_PIN);
    displayReady_ = display->begin(TFT_SPI_FREQUENCY);
#else
    const bool touchReady = true;
    displayReady_ = simulatorDisplayBegin(kScreenWidth, kScreenHeight);
#endif
    if (!displayReady_) {
        Serial.printf("DISPLAY: %s initialisation failed\n", DISPLAY_PROFILE_NAME);
        return false;
    }
    applyBacklight(systemSettings_.displayBrightnessPercent);

#if defined(JR_R1_DISPLAY_DIAGNOSTIC) && !defined(JR_R1_SIMULATOR)
    // A deliberately primitive test before LVGL is involved. If these fills
    // appear, panel power, reset, backlight and the write side of SPI work.
    // MISO and touch are tested separately by the normal runtime.
    applyBacklight(100);
    Serial.println("DISPLAY TEST: RED GREEN BLUE WHITE BLACK");
    display->fillScreen(RGB565_RED);
    delay(800);
    display->fillScreen(RGB565_GREEN);
    delay(800);
    display->fillScreen(RGB565_BLUE);
    delay(800);
    display->fillScreen(RGB565_WHITE);
    delay(800);
    display->fillScreen(RGB565_BLACK);
    delay(800);

    for (const uint8_t address : {uint8_t(0x14), uint8_t(0x5d)}) {
        Wire.beginTransmission(address);
        Serial.printf("DISPLAY TEST: I2C 0x%02X %s\n", address,
                      Wire.endTransmission() == 0 ? "ACK" : "no response");
    }
#endif

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
    Serial.printf("DISPLAY: %s, %dx%d, SPI=%lu Hz, touch=%s, buffer=%u rows\n",
                  DISPLAY_PROFILE_NAME, kScreenWidth, kScreenHeight,
                  static_cast<unsigned long>(TFT_SPI_FREQUENCY),
                  touchReady ? "ready" : "offline", 16U);
    if (!touchReady)
        Serial.println("DISPLAY: touch controller did not respond; LCD remains usable");
    playBuzzer(80);
    return true;
}

bool Channel2Ui::captureScreenshot(Stream &output)
{
    if (!displayReady_) return false;
    if (displaySleeping_) wakeDisplay();

    // A capture blocks the UI/control loop while pixels cross USB. Keep the
    // independent CRSF task alive, but require an explicit re-arm afterwards.
    armRequested_ = false;
    safetyResetRequested_ = true;
    lv_refr_now(nullptr);
    delay(2);

    char header[64];
    const uint32_t payloadBytes =
        static_cast<uint32_t>(kScreenWidth) * kScreenHeight * 3U;
    const int headerLength = snprintf(
        header, sizeof(header), "TFTSHOT1 %d %d RGB888 %lu\n",
        kScreenWidth, kScreenHeight,
        static_cast<unsigned long>(payloadBytes));
    output.write(reinterpret_cast<const uint8_t *>(header), headerLength);

#ifdef JR_R1_SIMULATOR
    simulatorWriteScreenshot(output);
#else
    const SPISettings settings(kScreenshotSpiFrequency, MSBFIRST, SPI_MODE0);
    for (uint16_t row = 0; row < kScreenHeight; ++row) {
        screenshotSpi.beginTransaction(settings);
        digitalWrite(TFT_CS_PIN, LOW);

        screenshotCommand(kLcdColumnAddressSet);
        screenshotCoordinate(0, kScreenWidth - 1);
        screenshotCommand(kLcdPageAddressSet);
        screenshotCoordinate(row, row);
        screenshotCommand(kLcdMemoryRead);

        // ILI9486 memory reads contain one dummy byte followed by one byte per
        // red, green and blue component. The low two bits may be zero because
        // the controller's internal colour path is six bits per component.
        screenshotSpi.transfer(0x00);
        for (size_t index = 0; index < sizeof(screenshotRow); ++index)
            screenshotRow[index] = screenshotSpi.transfer(0x00);

        digitalWrite(TFT_CS_PIN, HIGH);
        screenshotSpi.endTransaction();
        output.write(screenshotRow, sizeof(screenshotRow));
        delay(0);
    }
#endif
    output.print("\nTFTSHOT-END\n");
    output.flush();
    return true;
}

void Channel2Ui::resetLvglObjects()
{
    dashboardThrottleBar_ = nullptr;
    pageFocusIndicator_ = nullptr;
    pageFocusCount_ = 0;
    pageFocusIndex_ = 0;
    for (lv_obj_t *&button : mainMenuButtons_) button = nullptr;
    for (lv_obj_t *&bar : dashboardChannelBars_) bar = nullptr;
    dashboardSafetyPanel_ = nullptr;
    dashboardSafetyLabel_ = nullptr;
    dashboardLinkLabel_ = nullptr;
    dashboardRxVoltageLabel_ = nullptr;
    dashboardTxVoltageLabel_ = nullptr;
    telemetryLinkStateLabel_ = nullptr;
    telemetryLinkQualityLabel_ = nullptr;
    telemetryRssiLabel_ = nullptr;
    telemetrySnrLabel_ = nullptr;
    telemetryRadioLabel_ = nullptr;
    telemetryTxVoltageLabel_ = nullptr;
    telemetryRxVoltageLabel_ = nullptr;
    telemetryFramesLabel_ = nullptr;
    telemetryCrcLabel_ = nullptr;
    telemetryBytesLabel_ = nullptr;
    telemetryAgeLabel_ = nullptr;
    channel2LiveBar_ = nullptr;
    channel2ValueLabel_ = nullptr;
    for (lv_obj_t *&label : channel2SettingValueLabels_) label = nullptr;
    channel2DirectionToggle_ = nullptr;
    channel2DirectionToggleLabel_ = nullptr;
    calibrationMinimumLabel_ = nullptr;
    calibrationLiveLabel_ = nullptr;
    calibrationMaximumLabel_ = nullptr;
    for (lv_obj_t *&label : inputMonitorStateLabels_) label = nullptr;
    for (lv_obj_t *&label : inputMonitorEventLabels_) label = nullptr;
}

void Channel2Ui::setPhysicalInputStates(const PhysicalInputStateStore &states)
{
    for (size_t index = 0; index < kPhysicalControlCount; ++index) {
        physicalInputStates_[index] = states.state(
            static_cast<PhysicalControlId>(index));
    }
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
    settingsSavePending_ = false;
}

void Channel2Ui::scheduleSettingsSave()
{
    settingsSavePending_ = true;
    settingsSaveDueMs_ = millis() + kSettingsSaveDelayMs;
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

size_t Channel2Ui::copyBindings(ControlBinding *destination,
                                size_t capacity) const
{
    if (destination == nullptr) return 0;
    const size_t count = bindingCount_ < capacity ? bindingCount_ : capacity;
    for (size_t index = 0; index < count; ++index) {
        destination[index] = bindings_[index];
        if (destination[index].target == AssignmentTarget::ModelParameter &&
            isChannelParameterTarget(destination[index].targetId)) {
            const uint8_t channel = channelParameterTargetChannel(
                destination[index].targetId);
            const ChannelParameterKind kind = channelParameterTargetKind(
                destination[index].targetId);
            const int16_t value = channelParameterValue(channel, kind);
            destination[index].initial = value;
            destination[index].failsafe = value;
        }
    }
    return count;
}

bool Channel2Ui::consumeBindingsChanged()
{
    const bool changed = bindingsChanged_;
    bindingsChanged_ = false;
    return changed;
}

bool Channel2Ui::consumeSafetyResetRequested()
{
    const bool requested = safetyResetRequested_;
    safetyResetRequested_ = false;
    return requested;
}

bool Channel2Ui::consumeElrsManagementRequest(ElrsManagementRequest &request)
{
    if (!elrsRequestPending_) return false;
    request = elrsPendingRequest_;
    elrsRequestPending_ = false;
    return true;
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
    // The profile selection is intentionally not retained yet. A power cycle
    // still starts safely because armRequested_ is always false.
    driveModeSettings_.activeMode = DriveMode::Normal;
}

void Channel2Ui::saveDriveModeSettings()
{
    Preferences preferences;
    preferences.begin("jr-r1-drive", false);
    preferences.putUChar("walk_limit", driveModeSettings_.walkLimitPercent);
    preferences.end();
}

void Channel2Ui::loadElrsSettings()
{
    Preferences preferences;
    preferences.begin("jr-r1-elrs", true);
    String phrase = preferences.getString("phrase", "");
    preferences.end();
    phrase.toCharArray(elrsBindPhrase_, sizeof(elrsBindPhrase_));
}

void Channel2Ui::saveElrsSettings()
{
    Preferences preferences;
    preferences.begin("jr-r1-elrs", false);
    preferences.putString("phrase", elrsBindPhrase_);
    preferences.end();
}

void Channel2Ui::restoreDefaultBindings()
{
    bindingCount_ = 1;
    bindings_[0] = defaultBindingForInput(PhysicalControlId::TimerButton);
    bindings_[0].target = AssignmentTarget::TransmitterFunction;
    bindings_[0].targetId = static_cast<uint16_t>(TransmitterFunction::NextPage);
    bindings_[0].contexts = ContextDashboard;
    bindings_[0].event = InputEvent::Pressed;
    bindings_[0].behaviour = ControlBehaviour::Momentary;
    bindings_[0].crsfChannel = 0;
    bindings_[0].enabled = true;
}

void Channel2Ui::loadBindings()
{
    Preferences preferences;
    preferences.begin("jr-r1-bind", true);
    const uint16_t version = preferences.getUShort("version", 0);
    const uint8_t count = preferences.getUChar("count", 0);
    const size_t storedBytes = preferences.getBytesLength("items");
    bool valid = version == kBindingProfileVersion &&
                 count <= kPhysicalControlCount &&
                 storedBytes == static_cast<size_t>(count) * sizeof(ControlBinding);
    if (valid && count != 0) {
        valid = preferences.getBytes("items", bindings_, storedBytes) == storedBytes;
    }
    preferences.end();
    bindingCount_ = valid ? count : 0;

    BindingResolver validator;
    if (!valid || !validator.setBindings(bindings_, bindingCount_).ok()) {
        restoreDefaultBindings();
        saveBindings();
        Serial.println("BINDINGS: invalid/missing profile; restored defaults");
    } else {
        Serial.printf("BINDINGS: loaded %u editable assignments\n",
                      static_cast<unsigned>(bindingCount_));
    }
}

void Channel2Ui::saveBindings()
{
    Preferences preferences;
    preferences.begin("jr-r1-bind", false);
    preferences.putUShort("version", kBindingProfileVersion);
    preferences.putUChar("count", static_cast<uint8_t>(bindingCount_));
    preferences.putBytes("items", bindings_, bindingCount_ * sizeof(ControlBinding));
    preferences.end();
}

void Channel2Ui::loadChannelParameters()
{
    Preferences preferences;
    preferences.begin("jr-r1-chparam", true);
    const uint16_t version = preferences.getUShort("version", 0);
    const size_t bytes = preferences.getBytesLength("values");
    const bool valid = version == kChannelParameterVersion &&
        bytes == sizeof(channelParameters_) &&
        preferences.getBytes("values", channelParameters_, bytes) == bytes;
    preferences.end();
    if (!valid) {
        for (ChannelParameterValues &values : channelParameters_)
            values = ChannelParameterValues{};
    }
    for (ChannelParameterValues &values : channelParameters_) {
        values.trimPermille = constrain(values.trimPermille,
            -kChannelParameterLimitPermille, kChannelParameterLimitPermille);
        values.subtrimPermille = constrain(values.subtrimPermille,
            -kChannelParameterLimitPermille, kChannelParameterLimitPermille);
    }
}

void Channel2Ui::saveChannelParameters()
{
    Preferences preferences;
    preferences.begin("jr-r1-chparam", false);
    preferences.putUShort("version", kChannelParameterVersion);
    preferences.putBytes("values", channelParameters_, sizeof(channelParameters_));
    preferences.end();
    channelParameterSavePending_ = false;
}

void Channel2Ui::scheduleChannelParameterSave()
{
    channelParameterSavePending_ = true;
    channelParameterSaveDueMs_ = millis() + kSettingsSaveDelayMs;
}

int Channel2Ui::bindingIndexForInput(PhysicalControlId input) const
{
    for (size_t index = 0; index < bindingCount_; ++index)
        if (bindings_[index].input == input) return static_cast<int>(index);
    return -1;
}

int Channel2Ui::bindingIndexForChannelParameter(
    uint8_t channel, ChannelParameterKind kind) const
{
    const uint16_t targetId = encodeChannelParameterTarget(channel, kind);
    for (size_t index = 0; index < bindingCount_; ++index) {
        if (bindings_[index].target == AssignmentTarget::ModelParameter &&
            bindings_[index].targetId == targetId)
            return static_cast<int>(index);
    }
    return -1;
}

PhysicalControlId Channel2Ui::adjacentChannelParameterControl(
    uint8_t channel, ChannelParameterKind kind, int8_t direction) const
{
    constexpr PhysicalControlId choices[] = {
        PhysicalControlId::Count,
        PhysicalControlId::GripTrimA,
        PhysicalControlId::GripTrimB,
        PhysicalControlId::ThrottleTrim,
        PhysicalControlId::SteeringTrim,
        PhysicalControlId::Aux3Trim,
    };
    PhysicalControlId current = PhysicalControlId::Count;
    const int currentBinding = bindingIndexForChannelParameter(channel, kind);
    if (currentBinding >= 0) current = bindings_[currentBinding].input;
    int selected = 0;
    for (size_t index = 0; index < sizeof(choices) / sizeof(choices[0]); ++index)
        if (choices[index] == current) selected = static_cast<int>(index);
    for (size_t attempt = 0; attempt < sizeof(choices) / sizeof(choices[0]); ++attempt) {
        selected += direction;
        if (selected < 0) selected = static_cast<int>(sizeof(choices) /
            sizeof(choices[0])) - 1;
        if (selected >= static_cast<int>(sizeof(choices) / sizeof(choices[0])))
            selected = 0;
        const PhysicalControlId candidate = choices[selected];
        if (candidate == PhysicalControlId::Count ||
            bindingIndexForInput(candidate) < 0 || candidate == current)
            return candidate;
    }
    return current;
}

bool Channel2Ui::assignChannelParameterControl(
    uint8_t channel, ChannelParameterKind kind, PhysicalControlId input)
{
    ControlBinding candidate[kPhysicalControlCount];
    size_t count = 0;
    const uint16_t targetId = encodeChannelParameterTarget(channel, kind);
    for (size_t index = 0; index < bindingCount_; ++index) {
        if (bindings_[index].target == AssignmentTarget::ModelParameter &&
            bindings_[index].targetId == targetId)
            continue;
        candidate[count++] = bindings_[index];
    }
    if (input != PhysicalControlId::Count) {
        for (size_t index = 0; index < count; ++index)
            if (candidate[index].input == input) return false;
        if (count >= kPhysicalControlCount) return false;
        ControlBinding binding = defaultBindingForInput(input);
        binding.target = AssignmentTarget::ModelParameter;
        binding.targetId = targetId;
        binding.crsfChannel = 0;
        binding.event = InputEvent::PositivePress;
        binding.behaviour = ControlBehaviour::Stepped;
        binding.minimum = -kChannelParameterLimitPermille;
        binding.maximum = kChannelParameterLimitPermille;
        binding.step = 10;
        binding.initial = channelParameterValue(channel, kind);
        binding.failsafe = binding.initial;
        binding.enabled = true;
        binding.persistent = true;
        candidate[count++] = binding;
    }
    BindingResolver validator;
    if (!validator.setBindings(candidate, count).ok()) return false;
    bindingCount_ = count;
    for (size_t index = 0; index < count; ++index) bindings_[index] = candidate[index];
    saveBindings();
    bindingsChanged_ = true;
    safetyResetRequested_ = true;
    armRequested_ = false;
    return true;
}

ControlBinding Channel2Ui::defaultBindingForInput(PhysicalControlId input) const
{
    ControlBinding binding;
    binding.input = input;
    binding.target = AssignmentTarget::CrsfChannel;
    binding.minimum = -1000;
    binding.maximum = 1000;
    binding.initial = 0;
    binding.failsafe = 0;
    binding.crsfChannel = 3;
    binding.enabled = false;
    const PhysicalControlKind kind = physicalControlDescriptor(
        static_cast<size_t>(input)).kind;
    if (kind == PhysicalControlKind::Proportional) {
        binding.event = InputEvent::ValueChanged;
        binding.behaviour = ControlBehaviour::Proportional;
    } else if (kind == PhysicalControlKind::MaintainedSwitch) {
        binding.event = InputEvent::Pressed;
        binding.behaviour = ControlBehaviour::TwoPosition;
    } else if (kind == PhysicalControlKind::SpringCentredTrim) {
        binding.event = InputEvent::PositivePress;
        binding.behaviour = ControlBehaviour::SpringReturnThreePosition;
    } else {
        binding.event = InputEvent::Pressed;
        binding.behaviour = ControlBehaviour::Momentary;
    }
    return binding;
}

uint8_t Channel2Ui::adjacentBindingChannel(uint8_t current,
                                           int8_t direction) const
{
    uint8_t candidate = current;
    for (uint8_t attempts = 0; attempts < 16; ++attempts) {
        candidate = static_cast<uint8_t>(
            ((static_cast<int>(candidate) - 1 + direction + 16) % 16) + 1);
        if (candidate == channelRoleSettings_.steeringChannel ||
            candidate == channelRoleSettings_.throttleChannel)
            continue;
        bool occupied = false;
        for (size_t index = 0; index < bindingCount_; ++index) {
            if (bindings_[index].input != selectedBindingInput_ &&
                bindings_[index].enabled &&
                bindings_[index].target == AssignmentTarget::CrsfChannel &&
                bindings_[index].crsfChannel == candidate) {
                occupied = true;
                break;
            }
        }
        if (!occupied) return candidate;
    }
    return current;
}

ControlBehaviour Channel2Ui::adjacentBindingBehaviour(
    PhysicalControlKind kind, ControlBehaviour current, int8_t direction) const
{
    const ControlBehaviour buttonModes[] = {
        ControlBehaviour::Momentary, ControlBehaviour::Toggle,
        ControlBehaviour::Pulse};
    const ControlBehaviour trimModes[] = {
        ControlBehaviour::SpringReturnThreePosition,
        ControlBehaviour::Stepped,
        ControlBehaviour::ThreePositionSelector};
    const ControlBehaviour *modes = buttonModes;
    size_t count = sizeof(buttonModes) / sizeof(buttonModes[0]);
    if (kind == PhysicalControlKind::MaintainedSwitch)
        return ControlBehaviour::TwoPosition;
    if (kind == PhysicalControlKind::SpringCentredTrim) {
        modes = trimModes;
        count = sizeof(trimModes) / sizeof(trimModes[0]);
    }
    size_t selected = 0;
    for (size_t index = 0; index < count; ++index)
        if (modes[index] == current) selected = index;
    int next = static_cast<int>(selected) + direction;
    if (next < 0) next = static_cast<int>(count) - 1;
    if (next >= static_cast<int>(count)) next = 0;
    return modes[next];
}

AssignmentTarget Channel2Ui::adjacentBindingTarget(
    PhysicalControlKind kind, AssignmentTarget current, int8_t direction) const
{
    const AssignmentTarget buttonTargets[] = {
        AssignmentTarget::CrsfChannel,
        AssignmentTarget::TransmitterFunction,
    };
    const AssignmentTarget switchTargets[] = {
        AssignmentTarget::CrsfChannel,
        AssignmentTarget::DriveMode,
        AssignmentTarget::TransmitterFunction,
    };
    const AssignmentTarget trimTargets[] = {
        AssignmentTarget::CrsfChannel,
        AssignmentTarget::ModelParameter,
    };
    const AssignmentTarget *targets = buttonTargets;
    size_t count = sizeof(buttonTargets) / sizeof(buttonTargets[0]);
    if (kind == PhysicalControlKind::MaintainedSwitch) {
        targets = switchTargets;
        count = sizeof(switchTargets) / sizeof(switchTargets[0]);
    } else if (kind == PhysicalControlKind::SpringCentredTrim) {
        targets = trimTargets;
        count = sizeof(trimTargets) / sizeof(trimTargets[0]);
    }
    size_t selected = 0;
    for (size_t index = 0; index < count; ++index)
        if (targets[index] == current) selected = index;
    int next = static_cast<int>(selected) + direction;
    if (next < 0) next = static_cast<int>(count) - 1;
    if (next >= static_cast<int>(count)) next = 0;
    return targets[next];
}

bool Channel2Ui::bindingUsesChannel(uint8_t channel,
                                    PhysicalControlId exceptInput) const
{
    for (size_t index = 0; index < bindingCount_; ++index) {
        const ControlBinding &binding = bindings_[index];
        if (binding.input != exceptInput && binding.enabled &&
            binding.target == AssignmentTarget::CrsfChannel &&
            binding.crsfChannel == channel)
            return true;
    }
    return false;
}

bool Channel2Ui::saveBindingDraft()
{
    bindingError_[0] = '\0';
    if (bindingDraft_.enabled &&
        bindingDraft_.target == AssignmentTarget::CrsfChannel &&
        (bindingDraft_.crsfChannel == channelRoleSettings_.steeringChannel ||
         bindingDraft_.crsfChannel == channelRoleSettings_.throttleChannel)) {
        snprintf(bindingError_, sizeof(bindingError_),
                 "CH%u is reserved for a primary role", bindingDraft_.crsfChannel);
        return false;
    }
    if (bindingDraft_.target == AssignmentTarget::ModelParameter &&
        isChannelParameterTarget(bindingDraft_.targetId)) {
        const uint8_t channel = channelParameterTargetChannel(bindingDraft_.targetId);
        const ChannelParameterKind parameter = channelParameterTargetKind(
            bindingDraft_.targetId);
        bindingDraft_.initial = channelParameterValue(channel, parameter);
        bindingDraft_.failsafe = bindingDraft_.initial;
    }

    ControlBinding candidate[kPhysicalControlCount];
    size_t count = bindingCount_;
    for (size_t index = 0; index < count; ++index) candidate[index] = bindings_[index];
    const int existing = bindingIndexForInput(selectedBindingInput_);
    if (existing >= 0) candidate[existing] = bindingDraft_;
    else if (count < kPhysicalControlCount) candidate[count++] = bindingDraft_;
    else {
        snprintf(bindingError_, sizeof(bindingError_), "Assignment storage is full");
        return false;
    }

    BindingResolver validator;
    const BindingLoadResult result = validator.setBindings(candidate, count);
    if (!result.ok()) {
        snprintf(bindingError_, sizeof(bindingError_), "%s",
                 result.reason == nullptr ? "Invalid assignment" : result.reason);
        return false;
    }
    bindingCount_ = count;
    for (size_t index = 0; index < count; ++index) bindings_[index] = candidate[index];
    saveBindings();
    bindingsChanged_ = true;
    safetyResetRequested_ = true;
    armRequested_ = false;
    return true;
}

bool Channel2Ui::removeSelectedBinding()
{
    const int existing = bindingIndexForInput(selectedBindingInput_);
    if (existing < 0) return false;
    for (size_t index = static_cast<size_t>(existing); index + 1 < bindingCount_;
         ++index)
        bindings_[index] = bindings_[index + 1];
    --bindingCount_;
    saveBindings();
    bindingsChanged_ = true;
    safetyResetRequested_ = true;
    armRequested_ = false;
    return true;
}

void Channel2Ui::playBuzzer(uint16_t durationMs)
{
    if (!systemSettings_.buzzerEnabled) return;
#ifdef JR_R1_SIMULATOR
    simulatorBeep(systemSettings_.buzzerPitchHz, durationMs);
#else
    tone(BUZZER_PIN, systemSettings_.buzzerPitchHz, durationMs);
#endif
}

void Channel2Ui::playKeyClick()
{
    if (systemSettings_.keyClicksEnabled) playBuzzer(systemSettings_.keyClickDurationMs);
}

void Channel2Ui::stopBuzzer()
{
#ifdef JR_R1_SIMULATOR
    simulatorStopBeep();
#else
    noTone(BUZZER_PIN);
#endif
}

void Channel2Ui::sleepDisplay()
{
    if (!displayReady_ || displaySleeping_) return;
#ifdef JR_R1_SIMULATOR
    simulatorSetDisplayEnabled(false);
#else
    displayBus->beginWrite();
    // Standard DCS Display OFF. Do not also issue Sleep IN here: on the BTT
    // panel that combination leaves the powered backlight illuminating a
    // solid white panel. The Waveshare profile separately turns its exposed
    // backlight off below.
    displayBus->writeCommand(0x28);
    displayBus->endWrite();
    if (DISPLAY_HAS_PWM_BACKLIGHT)
        ledcWrite(TFT_BACKLIGHT_LEDC_CHANNEL, 0);
#endif
    displaySleeping_ = true;
    displayWakeArmed_ = false;
    displayWakeIdleSinceMs_ = 0;
    nextDisplayWakePollMs_ = 0;
    Serial.println(DISPLAY_HAS_PWM_BACKLIGHT
        ? "DISPLAY: off (0x28 and backlight off; touch to wake)"
        : "DISPLAY: off (0x28; touch to wake)");
}

void Channel2Ui::wakeDisplay()
{
    if (!displayReady_ || !displaySleeping_) return;
#ifdef JR_R1_SIMULATOR
    simulatorSetDisplayEnabled(true);
#else
    displayBus->beginWrite();
    displayBus->writeCommand(0x29); // Standard DCS Display ON
    displayBus->endWrite();
#endif
    displaySleeping_ = false;
    applyBacklight(systemSettings_.displayBrightnessPercent);
    lastInteractionMs_ = millis();
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(nullptr);
    Serial.println("DISPLAY: awake");
}

void Channel2Ui::redrawActivePage()
{
    switch (activePage_) {
        case UiPage::Dashboard: drawDashboard(); break;
        case UiPage::Telemetry: drawTelemetryPage(); break;
        case UiPage::MainMenu: drawMainMenu(); break;
        case UiPage::Channels: drawChannelsPage(); break;
        case UiPage::ChannelRoles: drawChannelRolesPage(); break;
        case UiPage::Channel2: drawPage(); break;
        case UiPage::ThrottleResponse: drawThrottleResponsePage(); break;
        case UiPage::ChannelSpeed: drawChannelSpeedPage(); break;
        case UiPage::ControlAssignments: drawControlAssignmentsPage(); break;
        case UiPage::BindingEditor: drawBindingEditorPage(); break;
        case UiPage::InputMonitor: drawInputMonitorPage(); break;
        case UiPage::DriveModes: drawDriveModesPage(); break;
        case UiPage::ChannelPlaceholder: drawChannelPlaceholder(); break;
        case UiPage::Setup: drawSetupPage(); break;
        case UiPage::TxBatterySetup: drawTxBatteryPage(); break;
        case UiPage::RxBatterySetup: drawRxBatteryPage(); break;
        case UiPage::DisplaySetup: drawDisplayPage(); break;
        case UiPage::ElrsBinding: drawElrsBindingPage(); break;
        case UiPage::Calibration: drawCalibrationPage(); break;
    }
}

int16_t Channel2Ui::mapAdcToPermille(uint16_t adc) const
{
    const ProportionalCalibration &calibration = triggerInput_.calibration();
    const uint16_t neutralLow = max<uint16_t>(
        calibration.minimum + 1,
        calibration.centre > ARM_NEUTRAL_ADC_WINDOW
            ? calibration.centre - ARM_NEUTRAL_ADC_WINDOW
            : calibration.minimum + 1);
    const uint16_t neutralHigh = min<uint16_t>(
        calibration.maximum - 1,
        calibration.centre + ARM_NEUTRAL_ADC_WINDOW);

    // The same region accepted by the neutral safety interlock must produce an
    // exact zero command. Otherwise ordinary spring/pot variation can be armed
    // as a small drive/brake command, then visibly jump when CUT restores the
    // configured neutral. Remap outside the deadband so both endpoints retain
    // the full -1000..+1000 range.
    if (adc >= neutralLow && adc <= neutralHigh) return 0;
    if (adc < neutralLow) {
        adc = max(adc, calibration.minimum);
        return static_cast<int16_t>(-
            static_cast<int32_t>(neutralLow - adc) * 1000 /
            (neutralLow - calibration.minimum));
    }
    adc = min(adc, calibration.maximum);
    return static_cast<int16_t>(
        static_cast<uint32_t>(adc - neutralHigh) * 1000 /
        (calibration.maximum - neutralHigh));
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

int16_t Channel2Ui::applyDriveMode(int16_t configuredPermille,
                                   int16_t rawInputPermille) const
{
    const int16_t trim = static_cast<int16_t>(settings_.subtrimPercent) * 10;
    int32_t driveComponent = static_cast<int32_t>(configuredPermille) - trim;
    // Select the limited side from the physical trigger direction, not the
    // post-reverse output sign. Reversing CH2 must not turn the brake side into
    // the limited side. Sub-trim is a centre offset rather than drive demand,
    // so remove it before scaling and restore it unchanged afterwards.
    if (driveModeSettings_.activeMode == DriveMode::Walk &&
        rawInputPermille > 0) {
        driveComponent = driveComponent * driveModeSettings_.walkLimitPercent / 100;
    }
    return clampExtendedTravel(driveComponent + trim);
}

int16_t Channel2Ui::neutralOutputPermille() const
{
    return channelNeutralPermille(channelRoleSettings_.throttleChannel);
}

int16_t Channel2Ui::applyChannelAdjustments(uint8_t channel,
                                            int16_t inputPermille) const
{
    if (channel < 1 || channel > 16) return inputPermille;
    const ChannelParameterValues &values = channelParameters_[channel - 1];
    // Subtrim translates the full output envelope, preserving endpoint travel.
    // Ordinary trim is then constrained to conventional travel, so using it
    // consumes travel at one end just like a traditional transmitter trim.
    const int16_t shifted = clampExtendedTravel(
        static_cast<int32_t>(inputPermille) + values.subtrimPermille);
    return static_cast<int16_t>(constrain(
        static_cast<int32_t>(shifted) + values.trimPermille, -1000L, 1000L));
}

int16_t Channel2Ui::channelNeutralPermille(uint8_t channel) const
{
    int16_t base = 0;
    if (channel == channelRoleSettings_.throttleChannel)
        base = static_cast<int16_t>(settings_.subtrimPercent) * 10;
    return applyChannelAdjustments(channel, base);
}

int16_t Channel2Ui::channelParameterValue(uint8_t channel,
                                          ChannelParameterKind kind) const
{
    if (channel < 1 || channel > 16) return 0;
    const ChannelParameterValues &values = channelParameters_[channel - 1];
    return kind == ChannelParameterKind::Subtrim
        ? values.subtrimPermille : values.trimPermille;
}

bool Channel2Ui::applyChannelParameterAction(uint16_t targetId, int16_t value)
{
    if (!isChannelParameterTarget(targetId)) return false;
    const uint8_t channel = channelParameterTargetChannel(targetId);
    const ChannelParameterKind kind = channelParameterTargetKind(targetId);
    if (channel < 1 || channel > 16 ||
        (kind != ChannelParameterKind::Trim &&
         kind != ChannelParameterKind::Subtrim)) return false;
    value = constrain(value, -kChannelParameterLimitPermille,
                      kChannelParameterLimitPermille);
    int16_t &stored = kind == ChannelParameterKind::Subtrim
        ? channelParameters_[channel - 1].subtrimPermille
        : channelParameters_[channel - 1].trimPermille;
    if (stored == value) return true;
    stored = value;
    scheduleChannelParameterSave();
    Serial.printf("CHANNEL PARAM: CH%u %s=%+.1f%%\n", channel,
                  channelParameterKindName(kind), value / 10.0f);
    if (activePage_ == UiPage::ChannelPlaceholder && selectedChannel_ == channel)
        drawChannelPlaceholder();
    return true;
}

uint8_t Channel2Ui::activeControlContext() const
{
    return (activePage_ == UiPage::Dashboard || activePage_ == UiPage::Telemetry)
        ? ContextDashboard : ContextMenu;
}

bool Channel2Ui::navigateDashboardPage(int8_t direction)
{
    // Keep the ordered list in one place as driving pages are added. Direction
    // is meaningful once GPS/map and power pages join this initial pair.
    constexpr UiPage pages[] = {UiPage::Dashboard, UiPage::Telemetry};
    constexpr size_t count = sizeof(pages) / sizeof(pages[0]);
    size_t selected = count;
    for (size_t index = 0; index < count; ++index) {
        if (pages[index] == activePage_) {
            selected = index;
            break;
        }
    }
    if (selected == count) return false;
    const int next = (static_cast<int>(selected) +
                      (direction < 0 ? -1 : 1) +
                      static_cast<int>(count)) % static_cast<int>(count);
    playKeyClick();
    if (pages[next] == UiPage::Dashboard) drawDashboard();
    else drawTelemetryPage();
    return true;
}

bool Channel2Ui::applyTransmitterFunctionAction(uint16_t targetId, int16_t value)
{
    if (targetId >= static_cast<uint16_t>(TransmitterFunction::Count)) return false;
    const TransmitterFunction function = static_cast<TransmitterFunction>(targetId);
    switch (function) {
        case TransmitterFunction::OpenMenu:
            if (value > 0) { playKeyClick(); drawMainMenu(); }
            return true;
        case TransmitterFunction::Dashboard:
            if (value > 0) { playKeyClick(); drawDashboard(); }
            return true;
        case TransmitterFunction::NextPage:
            return value <= 0 || navigateDashboardPage(1);
        case TransmitterFunction::PreviousPage:
            return value <= 0 || navigateDashboardPage(-1);
        case TransmitterFunction::QuickPanel:
            if (value > 0) {
                driveModesReturnPage_ = activePage_ == UiPage::Dashboard
                    ? UiPage::Dashboard : UiPage::MainMenu;
                playKeyClick();
                drawDriveModesPage();
            }
            return true;
        case TransmitterFunction::DisplayOnOff:
            if (value > 0) wakeDisplay(); else sleepDisplay();
            return true;
        case TransmitterFunction::DisplayWake:
            if (value > 0) wakeDisplay();
            return true;
        case TransmitterFunction::ToggleNightMode:
            if (value > 0) {
                systemSettings_.nightMode = !systemSettings_.nightMode;
                uiNightMode = systemSettings_.nightMode;
                saveSystemSettings();
                playKeyClick();
                redrawActivePage();
            }
            return true;
        case TransmitterFunction::ArmCut:
            armRequested_ = value > 0;
            safetyResetRequested_ = true;
            playBuzzer(80);
            if (activePage_ == UiPage::Dashboard) drawDashboard();
            return true;
        case TransmitterFunction::MenuPrevious:
            if (value > 0) {
                if (activePage_ == UiPage::MainMenu) moveMainMenuFocus(-1);
                else movePageFocus(-1);
            }
            return true;
        case TransmitterFunction::MenuNext:
            if (value > 0) {
                if (activePage_ == UiPage::MainMenu) moveMainMenuFocus(1);
                else movePageFocus(1);
            }
            return true;
        case TransmitterFunction::MenuSelect:
            if (value > 0) {
                if (activePage_ == UiPage::MainMenu) activateMainMenuFocus();
                else activatePageFocus();
            }
            return true;
        case TransmitterFunction::MenuBack:
            if (value > 0) navigateBack();
            return true;
        case TransmitterFunction::MenuDecrease:
            if (value > 0) adjustPageFocus(-1);
            return true;
        case TransmitterFunction::MenuIncrease:
            if (value > 0) adjustPageFocus(1);
            return true;
        default:
            // Reserved in the data model but not exposed by the first editor
            // slice until the corresponding runtime feature exists.
            return false;
    }
}

bool Channel2Ui::applyDriveModeAction(uint16_t targetId, int16_t value)
{
    if (targetId >= static_cast<uint16_t>(DriveModeSelection::Count)) return false;
    const DriveModeSelection selection = static_cast<DriveModeSelection>(targetId);
    if (selection == DriveModeSelection::CutArm) {
        armRequested_ = value > 0;
        safetyResetRequested_ = true;
    } else if (selection == DriveModeSelection::WalkNormal) {
        const DriveMode requested = value > 0 ? DriveMode::Normal : DriveMode::Walk;
        if (driveModeSettings_.activeMode != requested) {
            driveModeSettings_.activeMode = requested;
            armRequested_ = false;
            safetyResetRequested_ = true;
            saveDriveModeSettings();
        }
    } else {
        return false;
    }
    playBuzzer(80);
    if (activePage_ == UiPage::DriveModes) drawDriveModesPage();
    else if (activePage_ == UiPage::Dashboard) drawDashboard();
    return true;
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
    makeLabel(header, "CRSF TX", 320, 10, 16, cGreen);
    makeButton(433, 4, 39, 32, LV_SYMBOL_SETTINGS, 18);

    lv_obj_t *model = makePanel(8, 47, 144, 87);
    makeLabel(model, "HYPER 7", 10, 7, 12, cCyan);
    lv_obj_t *buggy = lv_img_create(model);
    lv_img_set_src(buggy, &hyper7_icon);
    lv_obj_set_pos(buggy, 31, 23);

    lv_obj_t *power = makePanel(160, 47, 144, 87);
    makeLabel(power, "POWER", 10, 8, 12, cCyan);
    dashboardTxVoltageLabel_ = makeLabel(power, "TX  --.-V", 10, 31, 16, cDim);
    dashboardRxVoltageLabel_ = makeLabel(power, "RX  --.-V", 10, 57, 16, cDim);

    dashboardSafetyPanel_ = makePanel(312, 47, 160, 87, cSafetyCut, cAmber);
    makeLabel(dashboardSafetyPanel_, "LINK / SAFETY", 10, 7, 12, cCyan);
    dashboardLinkLabel_ = makeLabel(dashboardSafetyPanel_, "NO LINK", 10, 28, 14, cRed);
    dashboardSafetyLabel_ = makeLabel(dashboardSafetyPanel_, "CUT", 10, 51, 22, cAmber);

    lv_obj_t *monitor = makePanel(8, 141, 464, 99);
    makeLabel(monitor, "CHANNEL MONITOR", 10, 6, 12, cCyan);
    uint8_t enabledChannels[CRSF_CHANNEL_COUNT];
    uint8_t enabledCount = 0;
    for (uint8_t channel = 1; channel <= CRSF_CHANNEL_COUNT; ++channel) {
        const uint16_t bit = static_cast<uint16_t>(1U << (channel - 1));
        if ((dashboardEnabledChannelMask_ & bit) != 0)
            enabledChannels[enabledCount++] = channel;
    }

    if (enabledCount == 0) {
        makeLabel(monitor, "NO ENABLED CHANNELS", 10, 43, 16, cDim);
    } else {
        // Steering and throttle are permanent primary instruments. Once the
        // 16-channel output model supplies enabled auxiliaries, they occupy a
        // compact vertical bank on the right without shrinking the primary
        // channels into the same low-priority representation.
        uint8_t primaryChannels[2];
        uint8_t primaryCount = 0;
        uint8_t auxiliaryChannels[14];
        uint8_t auxiliaryCount = 0;
        for (uint8_t index = 0; index < enabledCount; ++index) {
            const uint8_t channel = enabledChannels[index];
            const bool primary = channel == channelRoleSettings_.steeringChannel ||
                                 channel == channelRoleSettings_.throttleChannel;
            if (primary && primaryCount < 2)
                primaryChannels[primaryCount++] = channel;
            else if (!primary && auxiliaryCount < 14)
                auxiliaryChannels[auxiliaryCount++] = channel;
        }

        constexpr int16_t kAuxRightEdge = 450;
        constexpr int16_t kAuxSlotWidth = 18;
        constexpr int16_t kPrimaryBarX = 112;
        constexpr int16_t kPrimaryAuxGap = 8;
        const uint8_t visibleAuxiliaryCount = min<uint8_t>(auxiliaryCount, 6);
        const int16_t auxBankWidth = visibleAuxiliaryCount * kAuxSlotWidth;
        const int16_t auxBankX = kAuxRightEdge - auxBankWidth;
        const int16_t primaryBarWidth = auxiliaryCount == 0
            ? 338
            : auxBankX - kPrimaryAuxGap - kPrimaryBarX;
        const int16_t firstY = primaryCount == 1 ? 46 : 29;
        for (uint8_t index = 0; index < primaryCount; ++index) {
            const uint8_t channel = primaryChannels[index];
            const bool steering = channel == channelRoleSettings_.steeringChannel;
            char label[18];
            snprintf(label, sizeof(label), "CH%u %s", channel,
                     steering ? "STEER" : "THROTTLE");
            const int16_t y = firstY + index * 34;
            makeLabel(monitor, label, 10, y - 1, 12,
                      steering ? cWhite : cCyan);
            lv_obj_t *bar = makeBar(monitor, kPrimaryBarX, y,
                                    primaryBarWidth, 18,
                                    kCrsfMinimum, kCrsfMaximum,
                                    steering ? cWhite : cCyan);
            lv_bar_set_value(bar, kCrsfMid, LV_ANIM_OFF);
            dashboardChannelBars_[channel - 1] = bar;
            if (channel == channelRoleSettings_.throttleChannel)
                dashboardThrottleBar_ = bar;
        }

        if (auxiliaryCount > 0) {
            constexpr uint8_t kMaximumVisibleAuxiliaries = 6;
            const uint8_t visibleCount = min<uint8_t>(auxiliaryCount,
                                                       kMaximumVisibleAuxiliaries);
            const int16_t bankX = kAuxRightEdge -
                                  visibleCount * kAuxSlotWidth;
            const int16_t slotWidth = kAuxSlotWidth;
            for (uint8_t index = 0; index < visibleCount; ++index) {
                const uint8_t channel = auxiliaryChannels[index];
                const int16_t x = bankX + index * slotWidth;
                char number[4];
                snprintf(number, sizeof(number), "%u", channel);
                makeLabel(monitor, number,
                          x + max<int16_t>(0, (slotWidth - 12) / 2),
                          21, 10, cWhite);
                lv_obj_t *bar = makeBar(
                    monitor, x + max<int16_t>(1, (slotWidth - 10) / 2),
                    37, min<int16_t>(10, slotWidth - 2), 44,
                    kCrsfMinimum, kCrsfMaximum, cCyan);
                lv_bar_set_value(bar, kCrsfMid, LV_ANIM_OFF);
                dashboardChannelBars_[channel - 1] = bar;
            }
        }
    }

    makeButton(8, 249, 108, 62, "MENU", 18);
    makeButton(124, 249, 108, 62, "TELEMETRY", 11, cPanel2, cCyan);
    makeButton(240, 249, 108, 62, "QUICK", 16, cPanel2, cCyan);
    const char *mode = driveModeSettings_.activeMode == DriveMode::Walk
        ? "MODE\nWALK" : "MODE\nNORMAL";
    makeButton(356, 249, 116, 62, mode, 13, cPanel2, cGreen);

    lastDisplayedCrsf_ = UINT16_MAX;
    for (uint16_t &value : lastDashboardChannelValues_) value = UINT16_MAX;
    lastSafetyStatus_ = OutputSafetyStatus::ControlStale;
    lastLinkValid_ = !lastLinkValid_;
    lastReceiverVoltageValid_ = !lastReceiverVoltageValid_;
    lastTxVoltageValid_ = !lastTxVoltageValid_;
}

void Channel2Ui::drawTelemetryPage()
{
    if (!displayReady_) return;
    activePage_ = UiPage::Telemetry;
    startScreen();
    resetLvglObjects();
    makeHeader("TELEMETRY", "BACK");

    lv_obj_t *link = makePanel(8, 47, 226, 126);
    makeLabel(link, "RADIO LINK", 10, 7, 12, cCyan);
    telemetryLinkStateLabel_ = makeLabel(link, "NO LINK", 10, 27, 20, cRed);
    telemetryLinkQualityLabel_ = makeLabel(link, "LQ   ---%", 10, 56, 17, cDim);
    telemetryRssiLabel_ = makeLabel(link, "RSSI --- dBm", 10, 80, 15, cDim);
    telemetrySnrLabel_ = makeLabel(link, "SNR  -- dB", 10, 101, 13, cDim);

    lv_obj_t *power = makePanel(246, 47, 226, 126);
    makeLabel(power, "POWER / RF", 10, 7, 12, cCyan);
    telemetryTxVoltageLabel_ = makeLabel(power, "TX  --.- V", 10, 31, 18, cDim);
    telemetryRxVoltageLabel_ = makeLabel(power, "RX  --.- V", 10, 59, 18, cDim);
    telemetryRadioLabel_ = makeLabel(power, "ANT -   MODE -   PWR -", 10, 94, 12, cDim);

    lv_obj_t *stream = makePanel(8, 181, 464, 98);
    makeLabel(stream, "CRSF STREAM", 10, 7, 12, cCyan);
    telemetryFramesLabel_ = makeLabel(stream, "FRAMES          0", 10, 31, 15, cWhite);
    telemetryCrcLabel_ = makeLabel(stream, "CRC ERR         0", 245, 31, 15, cWhite);
    telemetryBytesLabel_ = makeLabel(stream, "BYTES           0", 10, 58, 15, cWhite);
    telemetryAgeLabel_ = makeLabel(stream, "AGE          ----", 245, 58, 15, cDim);

    makeLabel(lv_scr_act(), "Live receiver telemetry via Ranger Nano", 12, 293, 12, cDim);
}

void Channel2Ui::drawTelemetryLive(const CrsfTelemetryState &telemetry,
                                   uint16_t txVoltageDecivolts,
                                   bool txVoltageValid)
{
    if (!displayReady_ || activePage_ != UiPage::Telemetry) return;

    if (telemetryLinkStateLabel_) {
        lv_label_set_text(telemetryLinkStateLabel_,
                          telemetry.linkValid ? "LINK OK" : "NO LINK");
        lv_obj_set_style_text_color(telemetryLinkStateLabel_,
            colour(telemetry.linkValid ? cGreen : cRed), 0);
    }
    if (telemetryLinkQualityLabel_) {
        if (telemetry.linkValid) {
            lv_label_set_text_fmt(telemetryLinkQualityLabel_, "LQ   %3u%%",
                                  telemetry.uplinkLinkQuality);
            lv_obj_set_style_text_color(telemetryLinkQualityLabel_,
                colour(telemetry.uplinkLinkQuality >= 80 ? cGreen : cAmber), 0);
        } else {
            lv_label_set_text(telemetryLinkQualityLabel_, "LQ   ---%");
            lv_obj_set_style_text_color(telemetryLinkQualityLabel_, colour(cDim), 0);
        }
    }
    if (telemetryRssiLabel_) {
        if (telemetry.linkValid)
            lv_label_set_text_fmt(telemetryRssiLabel_, "RSSI %4d dBm",
                                  telemetry.uplinkRssiDbm);
        else lv_label_set_text(telemetryRssiLabel_, "RSSI --- dBm");
        lv_obj_set_style_text_color(telemetryRssiLabel_,
            colour(telemetry.linkValid ? cWhite : cDim), 0);
    }
    if (telemetrySnrLabel_) {
        if (telemetry.linkValid)
            lv_label_set_text_fmt(telemetrySnrLabel_, "SNR  %3d dB",
                                  telemetry.uplinkSnrDb);
        else lv_label_set_text(telemetrySnrLabel_, "SNR  -- dB");
        lv_obj_set_style_text_color(telemetrySnrLabel_,
            colour(telemetry.linkValid ? cWhite : cDim), 0);
    }
    if (telemetryRadioLabel_) {
        if (telemetry.linkValid)
            lv_label_set_text_fmt(telemetryRadioLabel_, "ANT %u   MODE %u   PWR %u",
                                  telemetry.activeAntenna + 1,
                                  telemetry.rfMode, telemetry.txPowerCode);
        else lv_label_set_text(telemetryRadioLabel_, "ANT -   MODE -   PWR -");
    }
    if (telemetryTxVoltageLabel_) {
        if (txVoltageValid) {
            const bool low = txVoltageDecivolts <= txBatterySettings_.lowAlarmDecivolts;
            lv_label_set_text_fmt(telemetryTxVoltageLabel_, "TX  %u.%u V",
                                  txVoltageDecivolts / 10,
                                  txVoltageDecivolts % 10);
            lv_obj_set_style_text_color(telemetryTxVoltageLabel_,
                                        colour(low ? cRed : cGreen), 0);
        } else {
            lv_label_set_text(telemetryTxVoltageLabel_, "TX  --.- V");
            lv_obj_set_style_text_color(telemetryTxVoltageLabel_, colour(cDim), 0);
        }
    }
    if (telemetryRxVoltageLabel_) {
        if (telemetry.receiverVoltageValid) {
            const bool low = rxBatterySettings_.alarmEnabled &&
                telemetry.receiverVoltageDecivolts <= rxBatterySettings_.lowAlarmDecivolts;
            lv_label_set_text_fmt(telemetryRxVoltageLabel_, "RX  %u.%u V",
                                  telemetry.receiverVoltageDecivolts / 10,
                                  telemetry.receiverVoltageDecivolts % 10);
            lv_obj_set_style_text_color(telemetryRxVoltageLabel_,
                                        colour(low ? cRed : cGreen), 0);
        } else {
            lv_label_set_text(telemetryRxVoltageLabel_, "RX  --.- V");
            lv_obj_set_style_text_color(telemetryRxVoltageLabel_, colour(cDim), 0);
        }
    }
    if (telemetryFramesLabel_)
        lv_label_set_text_fmt(telemetryFramesLabel_, "FRAMES %10lu",
                              static_cast<unsigned long>(telemetry.validFrames));
    if (telemetryCrcLabel_) {
        lv_label_set_text_fmt(telemetryCrcLabel_, "CRC ERR %8lu",
                              static_cast<unsigned long>(telemetry.crcErrors));
        lv_obj_set_style_text_color(telemetryCrcLabel_,
            colour(telemetry.crcErrors == 0 ? cWhite : cAmber), 0);
    }
    if (telemetryBytesLabel_)
        lv_label_set_text_fmt(telemetryBytesLabel_, "BYTES  %10lu",
                              static_cast<unsigned long>(telemetry.rawBytes));
    if (telemetryAgeLabel_) {
        if (telemetry.linkUpdatedMs != 0) {
            const uint32_t age = millis() - telemetry.linkUpdatedMs;
            lv_label_set_text_fmt(telemetryAgeLabel_, "AGE %9lu ms",
                                  static_cast<unsigned long>(age));
            lv_obj_set_style_text_color(telemetryAgeLabel_,
                                        colour(age <= 1000 ? cGreen : cAmber), 0);
        } else {
            lv_label_set_text(telemetryAgeLabel_, "AGE          ----");
            lv_obj_set_style_text_color(telemetryAgeLabel_, colour(cDim), 0);
        }
    }
}

void Channel2Ui::drawDashboardLive(uint16_t crsfValue,
                                   OutputSafetyStatus safetyStatus,
                                   const CrsfTelemetryState &telemetry,
                                   uint16_t txVoltageDecivolts,
                                   bool txVoltageValid)
{
    if (!displayReady_ || activePage_ != UiPage::Dashboard) return;
    for (size_t index = 0; index < CRSF_CHANNEL_COUNT; ++index) {
        if (dashboardChannelBars_[index] &&
            dashboardChannelValues_[index] != lastDashboardChannelValues_[index]) {
            lv_bar_set_value(dashboardChannelBars_[index],
                             dashboardChannelValues_[index], LV_ANIM_OFF);
            lastDashboardChannelValues_[index] = dashboardChannelValues_[index];
        }
    }
    lastDisplayedCrsf_ = crsfValue;
    if (dashboardSafetyLabel_ && dashboardSafetyPanel_ &&
        safetyStatus != lastSafetyStatus_) {
        const char *text = "CUT";
        uint32_t textColour = cAmber;
        uint32_t panelColour = cSafetyCut;
        uint32_t borderColour = cAmber;
        switch (safetyStatus) {
            case OutputSafetyStatus::Armed:
                text = "LIVE"; textColour = cGreen;
                panelColour = cSafetyArmed; borderColour = cGreen; break;
            case OutputSafetyStatus::NeutralRequired:
                text = "NEUTRAL..."; panelColour = cSafetyWaiting; break;
            case OutputSafetyStatus::Calibration:
                text = "CALIBRATE"; panelColour = cPanel2; break;
            case OutputSafetyStatus::InputFault:
                text = "INPUT FAULT"; textColour = cRed;
                panelColour = cSafetyFault; borderColour = cRed; break;
            case OutputSafetyStatus::ControlStale:
                text = "STALE"; textColour = cRed;
                panelColour = cSafetyFault; borderColour = cRed; break;
            case OutputSafetyStatus::ChannelDisabled:
                text = "CH OFF"; panelColour = cSafetyCut; break;
            case OutputSafetyStatus::Cut: break;
        }
        lv_label_set_text(dashboardSafetyLabel_, text);
        lv_obj_set_style_text_color(dashboardSafetyLabel_, colour(textColour), 0);
        lv_obj_set_style_bg_color(dashboardSafetyPanel_, colour(panelColour), 0);
        lv_obj_set_style_border_color(dashboardSafetyPanel_, colour(borderColour), 0);
        lastSafetyStatus_ = safetyStatus;
    }
    if (dashboardLinkLabel_ &&
        (telemetry.linkValid != lastLinkValid_ ||
         telemetry.uplinkLinkQuality != lastLinkQuality_ ||
        telemetry.uplinkRssiDbm != lastRssiDbm_)) {
        if (telemetry.linkValid) {
            lv_label_set_text_fmt(dashboardLinkLabel_, "%u%%  %d dBm",
                                  telemetry.uplinkLinkQuality,
                                  telemetry.uplinkRssiDbm);
            lv_obj_set_style_text_color(dashboardLinkLabel_,
                colour(telemetry.uplinkLinkQuality >= 80 ? cGreen : cAmber), 0);
        } else {
            lv_label_set_text(dashboardLinkLabel_, "NO LINK");
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
    makeHeader("JR R1 CONTROLLER");
    static const char *labels[] = {"MODELS", "CHANNELS", "RESPONSE", "CONTROL MAP",
        "DRIVE MODES", "TIMERS", "LINK / TELEM", "SETUP"};
    for (uint8_t index = 0; index < 8; ++index) {
        const bool enabled = mainMenuItemEnabled(index);
        mainMenuButtons_[index] = makeButton(
                   10 + (index % 2) * 235, 52 + (index / 2) * 54, 225, 46,
                   labels[index], 16,
                   enabled ? cPanel2 : cBlack,
                   enabled ? (index == 1 ? cCyan : cWhite) : cDim,
                   enabled ? cLine : cDim);
    }
    if (!mainMenuItemEnabled(mainMenuFocusIndex_)) mainMenuFocusIndex_ = 1;
    refreshMainMenuFocus();
    lv_obj_t *note = makeLabel(lv_scr_act(), "DIMMED ITEMS: COMING SOON", 0, 284, 12, cDim);
    lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 284);
}

void Channel2Ui::refreshMainMenuFocus()
{
    for (uint8_t index = 0; index < 8; ++index) {
        if (mainMenuButtons_[index] == nullptr) continue;
        const bool focused = index == mainMenuFocusIndex_ &&
                             mainMenuItemEnabled(index);
        lv_obj_set_style_border_color(mainMenuButtons_[index],
            colour(focused ? cCyan : (mainMenuItemEnabled(index) ? cLine : cDim)), 0);
        lv_obj_set_style_border_width(mainMenuButtons_[index], focused ? 3 : 1, 0);
    }
}

void Channel2Ui::moveMainMenuFocus(int8_t direction)
{
    if (activePage_ != UiPage::MainMenu) return;
    uint8_t candidate = mainMenuFocusIndex_;
    for (uint8_t attempts = 0; attempts < 8; ++attempts) {
        candidate = static_cast<uint8_t>(
            (static_cast<int>(candidate) + direction + 8) % 8);
        if (mainMenuItemEnabled(candidate)) {
            mainMenuFocusIndex_ = candidate;
            refreshMainMenuFocus();
            playKeyClick();
            return;
        }
    }
}

void Channel2Ui::activateMainMenuFocus()
{
    if (activePage_ != UiPage::MainMenu ||
        !mainMenuItemEnabled(mainMenuFocusIndex_)) return;
    playKeyClick();
    switch (mainMenuFocusIndex_) {
        case 1: drawChannelsPage(); break;
        case 2:
            throttleResponseReturnPage_ = UiPage::MainMenu;
            drawThrottleResponsePage();
            break;
        case 3: drawControlAssignmentsPage(); break;
        case 4:
            driveModesReturnPage_ = UiPage::MainMenu;
            drawDriveModesPage();
            break;
        case 6: drawElrsBindingPage(); break;
        case 7: drawSetupPage(); break;
        default: break;
    }
}

void Channel2Ui::beginPageFocus()
{
    pageFocusCount_ = 0;
    pageFocusIndex_ = 0;
    pageFocusIndicator_ = nullptr;
}

void Channel2Ui::addPageActionTarget(int16_t x, int16_t y, int16_t width,
                                     int16_t height,
                                     PageControlHandler handler,
                                     uint8_t argument)
{
    if (pageFocusCount_ >= kMaximumPageFocusTargets) return;
    PageFocusTarget &target = pageFocusTargets_[pageFocusCount_++];
    target = PageFocusTarget{};
    target.focus.x = x;
    target.focus.y = y;
    target.focus.width = width;
    target.focus.height = height;
    target.activate = target.focus;
    target.handler = handler;
    target.argument = argument;
}

void Channel2Ui::addPageEditorTarget(
    int16_t x, int16_t y, int16_t width, int16_t height,
    int16_t decreaseX, int16_t decreaseY,
    int16_t decreaseWidth, int16_t decreaseHeight,
    int16_t increaseX, int16_t increaseY,
    int16_t increaseWidth, int16_t increaseHeight,
    PageControlHandler handler, uint8_t argument)
{
    if (pageFocusCount_ >= kMaximumPageFocusTargets) return;
    PageFocusTarget &target = pageFocusTargets_[pageFocusCount_++];
    target = PageFocusTarget{};
    target.focus.x = x;
    target.focus.y = y;
    target.focus.width = width;
    target.focus.height = height;
    target.decrease.x = decreaseX;
    target.decrease.y = decreaseY;
    target.decrease.width = decreaseWidth;
    target.decrease.height = decreaseHeight;
    target.increase.x = increaseX;
    target.increase.y = increaseY;
    target.increase.width = increaseWidth;
    target.increase.height = increaseHeight;
    target.handler = handler;
    target.argument = argument;
}

void Channel2Ui::finishPageFocus()
{
    if (pageFocusCount_ == 0) return;
    pageFocusIndicator_ = lv_obj_create(lv_scr_act());
    lv_obj_set_style_bg_opa(pageFocusIndicator_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(pageFocusIndicator_, colour(cCyan), 0);
    lv_obj_set_style_border_width(pageFocusIndicator_, 3, 0);
    lv_obj_set_style_radius(pageFocusIndicator_, 7, 0);
    lv_obj_set_style_pad_all(pageFocusIndicator_, 0, 0);
    lv_obj_clear_flag(pageFocusIndicator_, LV_OBJ_FLAG_SCROLLABLE);
    refreshPageFocus();
}

void Channel2Ui::refreshPageFocus()
{
    if (pageFocusIndicator_ == nullptr || pageFocusCount_ == 0) return;
    if (pageFocusIndex_ >= pageFocusCount_) pageFocusIndex_ = 0;
    const PageFocusTarget &target = pageFocusTargets_[pageFocusIndex_];
    position(pageFocusIndicator_, target.focus.x, target.focus.y,
             target.focus.width, target.focus.height);
    lv_obj_move_foreground(pageFocusIndicator_);
}

void Channel2Ui::movePageFocus(int8_t direction)
{
    if (pageFocusCount_ == 0) return;
    pageFocusIndex_ = static_cast<uint8_t>(
        (static_cast<int>(pageFocusIndex_) + (direction < 0 ? -1 : 1) +
         pageFocusCount_) % pageFocusCount_);
    refreshPageFocus();
    playKeyClick();
}

void Channel2Ui::activatePageFocus()
{
    if (pageFocusCount_ == 0 || pageFocusIndex_ >= pageFocusCount_) return;
    if (dispatchPageControl(pageFocusIndex_, PageControlEvent::Activate)) return;
    playBuzzer(35);
}

bool Channel2Ui::adjustPageFocus(int8_t direction)
{
    if (pageFocusCount_ == 0 || pageFocusIndex_ >= pageFocusCount_) return false;
    if (dispatchPageControl(pageFocusIndex_, direction < 0
            ? PageControlEvent::Decrease : PageControlEvent::Increase))
        return true;
    playBuzzer(35);
    return false;
}

bool Channel2Ui::dispatchPageControl(uint8_t index, PageControlEvent event)
{
    if (index >= pageFocusCount_) return false;
    const PageFocusTarget target = pageFocusTargets_[index];
    if (target.handler == nullptr) return false;
    const uint8_t focused = pageFocusIndex_;
    (this->*target.handler)(target.argument, event);
    if (pageFocusCount_ != 0) {
        pageFocusIndex_ = min<uint8_t>(focused, pageFocusCount_ - 1);
        refreshPageFocus();
    }
    return true;
}

bool Channel2Ui::dispatchPageControlTouch(int16_t x, int16_t y)
{
    const auto contains = [x, y](const PageFocusTarget::Rectangle &rectangle) {
        return rectangle.width > 0 && rectangle.height > 0 &&
               x >= rectangle.x && x < rectangle.x + rectangle.width &&
               y >= rectangle.y && y < rectangle.y + rectangle.height;
    };
    for (uint8_t index = 0; index < pageFocusCount_; ++index) {
        const PageFocusTarget &target = pageFocusTargets_[index];
        if (target.handler == nullptr) continue;
        PageControlEvent event;
        if (contains(target.decrease)) event = PageControlEvent::Decrease;
        else if (contains(target.increase)) event = PageControlEvent::Increase;
        else if (contains(target.activate)) event = PageControlEvent::Activate;
        else continue;
        pageFocusIndex_ = index;
        refreshPageFocus();
        return dispatchPageControl(index, event);
    }
    return false;
}

void Channel2Ui::controlChannelRoles(uint8_t row, PageControlEvent event)
{
    const int16_t x = event == PageControlEvent::Decrease ? 375 : 430;
    handleChannelRolesTouch(x, 100 + row * 68);
}

void Channel2Ui::controlTxBattery(uint8_t row, PageControlEvent event)
{
    if (row == 0 && event == PageControlEvent::Decrease) {
        uint8_t chemistry = static_cast<uint8_t>(txBatterySettings_.chemistry);
        chemistry = static_cast<uint8_t>((chemistry + 3) % 4);
        txBatterySettings_.chemistry = static_cast<BatteryChemistry>(chemistry);
        txBatterySettings_.cellCount = min(txBatterySettings_.cellCount,
            maximumCellsFor(txBatterySettings_.chemistry));
        txBatterySettings_.lowAlarmDecivolts = defaultLowAlarmDecivolts(
            txBatterySettings_.chemistry, txBatterySettings_.cellCount);
        saveTxBatterySettings();
        playKeyClick();
        drawTxBatteryPage();
        return;
    }
    const int16_t x = event == PageControlEvent::Decrease ? 375 : 430;
    handleTxBatteryTouch(x, 78 + row * 50);
}

void Channel2Ui::controlRxBattery(uint8_t row, PageControlEvent event)
{
    if (row == 0 && event == PageControlEvent::Decrease) {
        uint8_t chemistry = static_cast<uint8_t>(rxBatterySettings_.chemistry);
        chemistry = static_cast<uint8_t>((chemistry + 3) % 4);
        rxBatterySettings_.chemistry = static_cast<BatteryChemistry>(chemistry);
        rxBatterySettings_.cellCount = min(rxBatterySettings_.cellCount,
            maximumCellsFor(rxBatterySettings_.chemistry));
        rxBatterySettings_.lowAlarmDecivolts = defaultLowAlarmDecivolts(
            rxBatterySettings_.chemistry, rxBatterySettings_.cellCount);
        saveRxBatterySettings();
        playKeyClick();
        drawRxBatteryPage();
        return;
    }
    const int16_t x = event == PageControlEvent::Decrease ? 375 : 430;
    handleRxBatteryTouch(x, 78 + row * 50);
}

void Channel2Ui::controlDisplay(uint8_t row, PageControlEvent event)
{
    const int16_t x = event == PageControlEvent::Decrease ? 375 : 430;
    handleDisplayTouch(x, 78 + row * 50);
}

void Channel2Ui::controlBindingEditor(uint8_t field, PageControlEvent event)
{
    if (field == 0) {
        handleBindingEditorTouch(400, 106);
    } else if (field >= 1 && field <= 3) {
        const int16_t x = event == PageControlEvent::Decrease ? 375 : 430;
        handleBindingEditorTouch(x, 145 + (field - 1) * 39);
    } else if (field == 4) {
        handleBindingEditorTouch(80, 290);
    } else if (field == 5) {
        handleBindingEditorTouch(220, 290);
    } else if (field == 6) {
        handleBindingEditorTouch(390, 290);
    }
}

void Channel2Ui::controlChannels(uint8_t item, PageControlEvent)
{
    if (item < 8) {
        selectedChannel_ = channelsBank_ * 8 + item + 1;
        playKeyClick();
        drawChannelPlaceholder();
    } else if (item == 8 && channelsBank_ > 0) {
        --channelsBank_; playKeyClick(); drawChannelsPage();
    } else if (item == 9 && channelsBank_ < 1) {
        ++channelsBank_; playKeyClick(); drawChannelsPage();
    } else if (item == 10) {
        playKeyClick(); drawChannelRolesPage();
    } else playBuzzer(35);
}

void Channel2Ui::controlAssignments(uint8_t item, PageControlEvent)
{
    constexpr uint8_t kRows = 5;
    constexpr uint8_t kBanks = (kPhysicalControlCount + kRows - 1) / kRows;
    if (item < kRows) {
        handleControlAssignmentsTouch(240, 50 + item * 40 + 17);
    } else if (item == 5 && assignmentBank_ > 0) {
        --assignmentBank_; playKeyClick(); drawControlAssignmentsPage();
    } else if (item == 6) {
        playKeyClick(); drawChannelRolesPage();
    } else if (item == 7) {
        inputMonitorBank_ = 0; playKeyClick(); drawInputMonitorPage();
    } else if (item == 8 && assignmentBank_ + 1 < kBanks) {
        ++assignmentBank_; playKeyClick(); drawControlAssignmentsPage();
    } else playBuzzer(35);
}

void Channel2Ui::controlDriveMode(uint8_t item, PageControlEvent event)
{
    if (item < 2) {
        handleDriveModesTouch(item == 0 ? 150 : 330, 95);
        return;
    }
    const int16_t x = event == PageControlEvent::Decrease ? 375 : 430;
    handleDriveModesTouch(x, 185);
}

void Channel2Ui::controlSetup(uint8_t item, PageControlEvent event)
{
    if (item < 4) {
        const int16_t x = event == PageControlEvent::Decrease ? 375 : 430;
        handleSetupTouch(x, 78 + item * 50);
    } else {
        static const int16_t x[] = {54, 160, 282, 408};
        handleSetupTouch(x[item - 4], 287);
    }
}

void Channel2Ui::controlThrottleSetting(uint8_t row, PageControlEvent event)
{
    const int8_t direction = event == PageControlEvent::Decrease ? -1 : 1;
    bool changed = false;
    if (row == 0) {
        settings_.brakeEndpointPercent = constrain(
            static_cast<int>(settings_.brakeEndpointPercent) + direction, 50, 100);
        changed = true;
    } else if (row == 1) {
        settings_.subtrimPercent = constrain(
            static_cast<int>(settings_.subtrimPercent) + direction, -20, 20);
        changed = true;
    } else if (row == 2) {
        settings_.forwardEndpointPercent = constrain(
            static_cast<int>(settings_.forwardEndpointPercent) + direction, 50, 100);
        changed = true;
    } else if (row == 3) {
        settings_.reversed = !settings_.reversed;
        changed = true;
    }
    if (!changed) return;
    playKeyClick();
    scheduleSettingsSave();
    refreshSettingRowValue(row);
}

void Channel2Ui::controlThrottleResponse(uint8_t row, PageControlEvent event)
{
    const int16_t x = event == PageControlEvent::Decrease ? 375 : 430;
    handleThrottleResponseTouch(x, 78 + row * 50);
}

void Channel2Ui::controlChannelSpeed(uint8_t row, PageControlEvent event)
{
    const int16_t x = event == PageControlEvent::Decrease ? 375 : 430;
    handleChannelSpeedTouch(x, 109 + row * 76);
}

void Channel2Ui::navigateBack()
{
    playKeyClick();
    switch (activePage_) {
        case UiPage::Dashboard: break;
        case UiPage::Telemetry: drawDashboard(); break;
        case UiPage::MainMenu: drawDashboard(); break;
        case UiPage::Channels: drawMainMenu(); break;
        case UiPage::ChannelRoles: drawChannelsPage(); break;
        case UiPage::Channel2:
        case UiPage::ChannelPlaceholder: drawChannelsPage(); break;
        case UiPage::ThrottleResponse:
            if (throttleResponseReturnPage_ == UiPage::Channel2) drawPage();
            else if (throttleResponseReturnPage_ == UiPage::Dashboard) drawDashboard();
            else drawMainMenu();
            break;
        case UiPage::ChannelSpeed: drawThrottleResponsePage(); break;
        case UiPage::ControlAssignments: drawMainMenu(); break;
        case UiPage::BindingEditor:
        case UiPage::InputMonitor: drawControlAssignmentsPage(); break;
        case UiPage::DriveModes:
            if (driveModesReturnPage_ == UiPage::Dashboard) drawDashboard();
            else drawMainMenu();
            break;
        case UiPage::Setup: drawMainMenu(); break;
        case UiPage::TxBatterySetup:
        case UiPage::RxBatterySetup:
        case UiPage::DisplaySetup: drawSetupPage(); break;
        case UiPage::ElrsBinding: drawMainMenu(); break;
        case UiPage::Calibration: drawPage(); break;
    }
}

void Channel2Ui::drawChannelsPage()
{
    activePage_ = UiPage::Channels;
    startScreen();
    resetLvglObjects();
    makeHeader("CHANNELS", "BACK");
    beginPageFocus();
    makeButton(258, 5, 78, 34, "ROLES", 11, cPanel2, cCyan);
    addPageActionTarget(258, 5, 78, 34,
                        &Channel2Ui::controlChannels, 10);
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
        addPageActionTarget(10 + (index % 2) * 235,
                            52 + (index / 2) * 52, 225, 44,
                            &Channel2Ui::controlChannels, index);
    }
    makeButton(12, 270, 58, 40, "<", 24,
               channelsBank_ == 0 ? cBlack : cPanel,
               channelsBank_ == 0 ? cDim : cWhite);
    addPageActionTarget(12, 270, 58, 40,
                        &Channel2Ui::controlChannels, 8);
    makeButton(410, 270, 58, 40, ">", 24,
               channelsBank_ == 1 ? cBlack : cPanel,
               channelsBank_ == 1 ? cDim : cWhite);
    addPageActionTarget(410, 270, 58, 40,
                        &Channel2Ui::controlChannels, 9);
    char page[8];
    snprintf(page, sizeof(page), "%u / 2", channelsBank_ + 1);
    lv_obj_t *pageLabel = makeLabel(lv_scr_act(), page, 0, 280, 16, cDim);
    lv_obj_align(pageLabel, LV_ALIGN_TOP_MID, 0, 280);
    finishPageFocus();
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
    makeHeader("ASSIGNMENTS", "BACK");
    beginPageFocus();

    constexpr uint8_t kRows = 5;
    constexpr uint8_t kBanks =
        (kPhysicalControlCount + kRows - 1) / kRows;
    const size_t first = assignmentBank_ * kRows;
    for (uint8_t row = 0; row < kRows; ++row) {
        const size_t inputIndex = first + row;
        if (inputIndex >= kPhysicalControlCount) break;
        const PhysicalControlDescriptor &control =
            physicalControlDescriptor(inputIndex);
        const int16_t y = 50 + row * 40;
        lv_obj_t *panel = makePanel(10, y, 460, 35);
        makeLabel(panel, control.name, 8, 5, 12, cWhite);
        char summary[48];
        uint32_t summaryColour = cDim;
        if (control.kind == PhysicalControlKind::Proportional) {
            const bool steering = control.id == PhysicalControlId::SteeringWheel;
            const bool enabled = steering ? channelRoleSettings_.steeringEnabled :
                                            channelRoleSettings_.throttleEnabled;
            const uint8_t channel = steering ? channelRoleSettings_.steeringChannel :
                                               channelRoleSettings_.throttleChannel;
            snprintf(summary, sizeof(summary), "ROLE CH%u  %s", channel,
                     enabled ? "ON" : "OFF");
            summaryColour = enabled ? cCyan : cAmber;
        } else {
            const int bindingIndex = bindingIndexForInput(control.id);
            if (bindingIndex < 0) {
                snprintf(summary, sizeof(summary), "UNASSIGNED");
            } else {
                const ControlBinding &binding = bindings_[bindingIndex];
                if (binding.target == AssignmentTarget::ModelParameter &&
                    isChannelParameterTarget(binding.targetId)) {
                    snprintf(summary, sizeof(summary), "%s CH%u %s",
                        binding.enabled ? "ON" : "OFF",
                        channelParameterTargetChannel(binding.targetId),
                        channelParameterKindName(
                            channelParameterTargetKind(binding.targetId)));
                } else if (binding.target == AssignmentTarget::TransmitterFunction) {
                    snprintf(summary, sizeof(summary), "%s  %s",
                             binding.enabled ? "ON" : "OFF",
                             transmitterFunctionName(static_cast<TransmitterFunction>(
                                 binding.targetId)));
                } else if (binding.target == AssignmentTarget::DriveMode) {
                    snprintf(summary, sizeof(summary), "%s  %s",
                             binding.enabled ? "ON" : "OFF",
                             driveModeSelectionName(static_cast<DriveModeSelection>(
                                 binding.targetId)));
                } else {
                    snprintf(summary, sizeof(summary), "%s CH%u  %s",
                             binding.enabled ? "ON" : "OFF", binding.crsfChannel,
                             controlBehaviourName(binding.behaviour));
                }
                summaryColour = binding.enabled ? cGreen : cAmber;
            }
        }
        makeLabel(panel, summary, 215, 5, 11, summaryColour);
        makeLabel(panel, ">", 438, 4, 16, cDim);
        addPageActionTarget(10, y, 460, 35,
                            &Channel2Ui::controlAssignments, row);
    }

    makeButton(10, 263, 50, 42, "<", 22,
               assignmentBank_ == 0 ? cBlack : cPanel,
               assignmentBank_ == 0 ? cDim : cWhite);
    addPageActionTarget(10, 263, 50, 42,
                        &Channel2Ui::controlAssignments, 5);
    makeButton(70, 263, 94, 42, "ROLES", 12, cPanel2, cCyan);
    addPageActionTarget(70, 263, 94, 42,
                        &Channel2Ui::controlAssignments, 6);
    makeButton(174, 263, 132, 42, "INPUT MON", 12, cPanel2, cCyan);
    addPageActionTarget(174, 263, 132, 42,
                        &Channel2Ui::controlAssignments, 7);
    char page[12];
    snprintf(page, sizeof(page), "%u/%u", assignmentBank_ + 1, kBanks);
    makeLabel(lv_scr_act(), page, 329, 276, 12, cDim);
    makeButton(420, 263, 50, 42, ">", 22,
               assignmentBank_ + 1 >= kBanks ? cBlack : cPanel,
               assignmentBank_ + 1 >= kBanks ? cDim : cWhite);
    addPageActionTarget(420, 263, 50, 42,
                        &Channel2Ui::controlAssignments, 8);
    finishPageFocus();
}

void Channel2Ui::drawBindingEditorPage()
{
    activePage_ = UiPage::BindingEditor;
    startScreen();
    resetLvglObjects();
    makeHeader("EDIT ASSIGNMENT", "BACK");
    beginPageFocus();

    const PhysicalControlDescriptor &control = physicalControlDescriptor(
        static_cast<size_t>(selectedBindingInput_));
    lv_obj_t *namePanel = makePanel(12, 48, 456, 36);
    makeLabel(namePanel, control.name, 12, 7, 15, cCyan);
    makeLabel(namePanel, r1InputEndpointLabel(selectedBindingInput_), 300, 9, 10, cDim);

    const bool parameter = bindingDraft_.target == AssignmentTarget::ModelParameter &&
        isChannelParameterTarget(bindingDraft_.targetId);
    const int16_t ys[] = {89, 128, 167, 206};
    const char *detailLabel =
        bindingDraft_.target == AssignmentTarget::TransmitterFunction ? "FUNCTION" :
        bindingDraft_.target == AssignmentTarget::DriveMode ? "SELECTION" : "CHANNEL";
    const char *labels[] = {"ENABLED", "TARGET", detailLabel,
                            parameter ? "PARAMETER" : "MODE"};
    for (uint8_t row = 0; row < 4; ++row) {
        lv_obj_t *panel = makePanel(12, ys[row], 456, 34);
        makeLabel(panel, labels[row], 10, 8, 12, cDim);
    }
    makeButton(344, 91, 108, 30, bindingDraft_.enabled ? "ON" : "OFF", 13,
               bindingDraft_.enabled ? cGreen : cPanel2,
               bindingDraft_.enabled ? cBlack : cWhite);
    addPageActionTarget(344, 91, 108, 30,
                        &Channel2Ui::controlBindingEditor, 0);

    makeLabel(lv_scr_act(), assignmentTargetName(bindingDraft_.target),
              220, 137, 12, bindingDraft_.target == AssignmentTarget::CrsfChannel
                  ? cWhite : cCyan);
    makeButton(350, 130, 42, 30, "<", 20);
    makeButton(408, 130, 42, 30, ">", 20);
    addPageEditorTarget(210, 128, 246, 34,
                        350, 130, 42, 30, 408, 130, 42, 30,
                        &Channel2Ui::controlBindingEditor, 1);

    char detail[32];
    if (bindingDraft_.target == AssignmentTarget::TransmitterFunction) {
        snprintf(detail, sizeof(detail), "%s", transmitterFunctionName(
            static_cast<TransmitterFunction>(bindingDraft_.targetId)));
    } else if (bindingDraft_.target == AssignmentTarget::DriveMode) {
        snprintf(detail, sizeof(detail), "%s", driveModeSelectionName(
            static_cast<DriveModeSelection>(bindingDraft_.targetId)));
    } else {
        const uint8_t selectedChannel = parameter
            ? channelParameterTargetChannel(bindingDraft_.targetId)
            : bindingDraft_.crsfChannel;
        snprintf(detail, sizeof(detail), "CH%u", selectedChannel);
    }
    makeLabel(lv_scr_act(), detail, 220, 176, 12, cWhite);
    makeButton(350, 169, 42, 30, "-", 20);
    makeButton(408, 169, 42, 30, "+", 20);
    addPageEditorTarget(210, 167, 246, 34,
                        350, 169, 42, 30, 408, 169, 42, 30,
                        &Channel2Ui::controlBindingEditor, 2);
    makeLabel(lv_scr_act(), parameter
                  ? channelParameterKindName(
                        channelParameterTargetKind(bindingDraft_.targetId))
                  : controlBehaviourName(bindingDraft_.behaviour),
              220, 215, 12, cWhite);
    makeButton(350, 208, 42, 30, "<", 20);
    makeButton(408, 208, 42, 30, ">", 20);
    addPageEditorTarget(210, 206, 246, 34,
                        350, 208, 42, 30, 408, 208, 42, 30,
                        &Channel2Ui::controlBindingEditor, 3);

    if (bindingError_[0] != '\0')
        makeLabel(lv_scr_act(), bindingError_, 14, 246, 11, cRed);
    else
        makeLabel(lv_scr_act(), parameter ? "Step: 1%; edits force CUT" :
                  "Assignment edits force CUT and neutral re-arm",
                  14, 246, 11, cAmber);
    makeButton(12, 274, 138, 34, "CANCEL", 13, cPanel);
    addPageActionTarget(12, 274, 138, 34,
                        &Channel2Ui::controlBindingEditor, 4);
    if (bindingIndexForInput(selectedBindingInput_) >= 0) {
        makeButton(160, 274, 138, 34, "REMOVE", 13, cAmber, cBlack);
        addPageActionTarget(160, 274, 138, 34,
                            &Channel2Ui::controlBindingEditor, 5);
    }
    makeButton(308, 274, 160, 34, "SAVE", 13, cGreen, cBlack);
    addPageActionTarget(308, 274, 160, 34,
                        &Channel2Ui::controlBindingEditor, 6);
    finishPageFocus();
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
        makeLabel(panel, control.name, 8, 5, 12, cWhite);
        makeLabel(panel, r1InputEndpointLabel(control.id), 150, 6, 11, cDim);
        inputMonitorStateLabels_[row] = makeLabel(panel, "--", 285, 5, 12, cWhite);
        inputMonitorEventLabels_[row] = makeLabel(panel, "--", 370, 6, 11, cDim);
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
    refreshInputMonitorRows();
}

void Channel2Ui::refreshInputMonitorRows()
{
    constexpr uint8_t kRowsPerBank = 5;
    const size_t first = inputMonitorBank_ * kRowsPerBank;
    const uint32_t now = millis();
    for (uint8_t row = 0; row < kRowsPerBank; ++row) {
        const size_t index = first + row;
        if (index >= kPhysicalControlCount || inputMonitorStateLabels_[row] == nullptr)
            continue;
        const PhysicalControlDescriptor &control = physicalControlDescriptor(index);
        const PhysicalInputState &state = physicalInputStates_[index];
        char logical[24];
        if (!state.mapped) {
            snprintf(logical, sizeof(logical), "UNMAPPED");
        } else if (state.faulted) {
            snprintf(logical, sizeof(logical), "FAULT %u/%u",
                     state.rawPrimary, state.rawSecondary);
        } else if (control.kind == PhysicalControlKind::Proportional) {
            snprintf(logical, sizeof(logical), "%+d", state.value);
        } else if (control.kind == PhysicalControlKind::SpringCentredTrim) {
            const char *position = state.stablePrimary ? "-" :
                                   state.stableSecondary ? "+" : "CENTRE";
            snprintf(logical, sizeof(logical), "%s %u/%u", position,
                     state.rawPrimary, state.rawSecondary);
        } else {
            snprintf(logical, sizeof(logical), "%s %u>%u",
                     state.stablePrimary ? "ON" : "OFF",
                     state.rawPrimary, state.stablePrimary);
        }
        lv_label_set_text(inputMonitorStateLabels_[row], logical);
        lv_obj_set_style_text_color(inputMonitorStateLabels_[row],
            colour(state.faulted ? cRed : state.mapped ? cGreen : cAmber), 0);

        char event[24];
        if (state.lastEventMs == 0) {
            snprintf(event, sizeof(event), "NO EVENT");
        } else {
            const uint32_t age = now - state.lastEventMs;
            snprintf(event, sizeof(event), "%s %lus",
                     inputEventName(state.lastEvent),
                     static_cast<unsigned long>(age / 1000));
        }
        lv_label_set_text(inputMonitorEventLabels_[row], event);
    }
}

void Channel2Ui::drawChannelRolesPage()
{
    activePage_ = UiPage::ChannelRoles;
    startScreen();
    resetLvglObjects();
    makeHeader("CHANNEL ROLES", "BACK");
    beginPageFocus();
    drawChannelRoleRow(0);
    addPageEditorTarget(18, 72, 444, 56,
                        350, 80, 44, 40, 404, 80, 44, 40,
                        &Channel2Ui::controlChannelRoles, 0);
    drawChannelRoleRow(1);
    addPageEditorTarget(18, 140, 444, 56,
                        350, 148, 44, 40, 404, 148, 44, 40,
                        &Channel2Ui::controlChannelRoles, 1);
    lv_obj_t *note = makeLabel(lv_scr_act(),
        "Changing roles disarms throttle output.\n"
        "Steering stays centred until its pot is connected.",
        0, 222, 14, cAmber);
    lv_obj_set_style_text_align(note, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 222);
    finishPageFocus();
}

void Channel2Ui::drawChannelPlaceholder()
{
    activePage_ = UiPage::ChannelPlaceholder;
    startScreen();
    resetLvglObjects();
    char title[20];
    snprintf(title, sizeof(title), "CHANNEL %u", selectedChannel_);
    makeHeader(title, "BACK");
    const char *role = selectedChannel_ == channelRoleSettings_.steeringChannel
        ? "STEERING" : selectedChannel_ == channelRoleSettings_.throttleChannel
        ? "THROTTLE" : "AUXILIARY";
    char roleText[32];
    snprintf(roleText, sizeof(roleText), "ROLE: %s", role);
    makeLabel(lv_scr_act(), roleText, 18, 52, 13, cCyan);

    const ChannelParameterKind kinds[] = {
        ChannelParameterKind::Trim, ChannelParameterKind::Subtrim};
    for (uint8_t row = 0; row < 2; ++row) {
        const int16_t y = 76 + row * 66;
        lv_obj_t *panel = makePanel(12, y, 456, 56);
        makeLabel(panel, channelParameterKindName(kinds[row]), 10, 8, 13, cDim);
        const int bindingIndex = bindingIndexForChannelParameter(
            selectedChannel_, kinds[row]);
        const char *control = bindingIndex >= 0
            ? physicalControlDescriptor(
                static_cast<size_t>(bindings_[bindingIndex].input)).name
            : "NONE";
        makeLabel(panel, control, 108, 7, 13,
                  bindingIndex >= 0 ? cWhite : cAmber);
        char value[20];
        snprintf(value, sizeof(value), "%+.1f%%",
                 channelParameterValue(selectedChannel_, kinds[row]) / 10.0f);
        makeLabel(panel, value, 260, 31, 12, cCyan);
        makeButton(352, y + 8, 42, 40, "<", 20);
        makeButton(406, y + 8, 42, 40, ">", 20);
    }
    makeLabel(lv_scr_act(), "A trim control can own one output or parameter",
              18, 214, 11, cAmber);
    if (selectedChannel_ == channelRoleSettings_.throttleChannel)
        makeButton(122, 249, 236, 46, "THROTTLE SETUP", 15, cPanel2, cCyan);
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
    channel2SettingValueLabels_[row] = makeLabel(
        panel, value, 212, 12, 16,
        row == 3 && settings_.reversed ? cAmber : cWhite);
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
        channel2DirectionToggle_ = lv_obj_create(panel);
        position(channel2DirectionToggle_, 343, 3, 106, 36);
        lv_obj_set_style_bg_color(channel2DirectionToggle_,
                                  colour(settings_.reversed ? cAmber : cPanel2), 0);
        lv_obj_set_style_border_color(channel2DirectionToggle_, colour(cLine), 0);
        lv_obj_set_style_border_width(channel2DirectionToggle_, 1, 0);
        lv_obj_set_style_radius(channel2DirectionToggle_, 5, 0);
        lv_obj_set_style_pad_all(channel2DirectionToggle_, 0, 0);
        channel2DirectionToggleLabel_ = makeLabel(
            channel2DirectionToggle_, "TOGGLE", 0, 0, 14,
            settings_.reversed ? cBlack : cWhite);
        lv_obj_center(channel2DirectionToggleLabel_);
    }
}

void Channel2Ui::refreshSettingRowValue(uint8_t row)
{
    if (row >= 4 || channel2SettingValueLabels_[row] == nullptr) return;
    if (row == 0)
        lv_label_set_text_fmt(channel2SettingValueLabels_[row], "%u%%",
                              settings_.brakeEndpointPercent);
    else if (row == 1)
        lv_label_set_text_fmt(channel2SettingValueLabels_[row], "%+d%%",
                              settings_.subtrimPercent);
    else if (row == 2)
        lv_label_set_text_fmt(channel2SettingValueLabels_[row], "%u%%",
                              settings_.forwardEndpointPercent);
    else {
        lv_label_set_text(channel2SettingValueLabels_[row],
                          settings_.reversed ? "REVERSE" : "NORMAL");
        lv_obj_set_style_text_color(channel2SettingValueLabels_[row],
            colour(settings_.reversed ? cAmber : cWhite), 0);
        if (channel2DirectionToggle_ != nullptr)
            lv_obj_set_style_bg_color(channel2DirectionToggle_,
                colour(settings_.reversed ? cAmber : cPanel2), 0);
        if (channel2DirectionToggleLabel_ != nullptr)
            lv_obj_set_style_text_color(channel2DirectionToggleLabel_,
                colour(settings_.reversed ? cBlack : cWhite), 0);
    }
}

void Channel2Ui::drawPage()
{
    activePage_ = UiPage::Channel2;
    startScreen();
    resetLvglObjects();
    beginPageFocus();
    char title[24];
    snprintf(title, sizeof(title), "CH%u THROTTLE", channelRoleSettings_.throttleChannel);
    makeHeader(title);
    makeButton(190, 5, 68, 34, "RESP", 11, cPanel2, cCyan);
    makeButton(266, 5, 70, 34, "BACK", 11);
    makeButton(344, 5, 78, 34, "CAL", 11);
    for (int row = 0; row < 4; ++row) {
        drawSettingRow(row);
        addPageEditorTarget(10, 56 + row * 50, 460, 44,
                            353, 59 + row * 50, 48, 36,
                            411, 59 + row * 50, 48, 36,
                            &Channel2Ui::controlThrottleSetting, row);
    }
    char channelLabel[16];
    snprintf(channelLabel, sizeof(channelLabel), "CH%u", channelRoleSettings_.throttleChannel);
    makeLabel(lv_scr_act(), channelLabel, 12, 283, 16, cCyan);
    channel2ValueLabel_ = makeLabel(lv_scr_act(), "992", 422, 283, 14, cGreen);
    channel2LiveBar_ = makeBar(lv_scr_act(), 60, 287, 350, 14,
                               kCrsfMinimum, kCrsfMaximum, cGreen);
    lv_bar_set_value(channel2LiveBar_, kCrsfMid, LV_ANIM_OFF);
    makeLabel(lv_scr_act(), settings_.reversed ? "FORWARD" : "BRAKE/REV", 60, 304, 12, cDim);
    makeLabel(lv_scr_act(), settings_.reversed ? "BRAKE/REV" : "FORWARD", 338, 304, 12, cDim);
    lastDisplayedCrsf_ = UINT16_MAX;
    finishPageFocus();
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
    beginPageFocus();
    makeHeader("THROTTLE RESPONSE", "BACK");
    makeButton(258, 5, 78, 34, "SPEED", 11, cPanel2, cCyan);
    for (int row = 0; row < 4; ++row) {
        drawThrottleResponseRow(row);
        addPageEditorTarget(10, 56 + row * 50, 460, 44,
                            353, 59 + row * 50, 48, 36,
                            411, 59 + row * 50, 48, 36,
                            &Channel2Ui::controlThrottleResponse, row);
    }
    lv_obj_t *note = makeLabel(lv_scr_act(),
        "+EXPO = SOFTER AROUND NEUTRAL", 0, 280, 12, cDim);
    lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 280);
    finishPageFocus();
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
    beginPageFocus();
    makeHeader("THROTTLE CHANNEL SPEED", "BACK");
    drawChannelSpeedRow(0);
    addPageEditorTarget(14, 78, 452, 62,
                        350, 98, 42, 34, 402, 98, 42, 34,
                        &Channel2Ui::controlChannelSpeed, 0);
    drawChannelSpeedRow(1);
    addPageEditorTarget(14, 154, 452, 62,
                        350, 174, 42, 34, 402, 174, 42, 34,
                        &Channel2Ui::controlChannelSpeed, 1);
    lv_obj_t *note = makeLabel(lv_scr_act(),
        "0 ms = direct. Time is neutral-to-endpoint slew.", 0, 250, 14, cDim);
    lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 250);
    finishPageFocus();
}

void Channel2Ui::drawDriveModesPage()
{
    activePage_ = UiPage::DriveModes;
    startScreen();
    resetLvglObjects();
    makeHeader("DRIVE MODE", "BACK");
    beginPageFocus();
    const char *labels[] = {"WALK", "NORMAL"};
    for (uint8_t index = 0; index < 2; ++index) {
        const DriveMode mode = static_cast<DriveMode>(index);
        const bool active = driveModeSettings_.activeMode == mode;
        makeButton(76 + index * 174, 64, 156, 62, labels[index], 18,
                   active ? cGreen : cPanel2,
                   active ? cBlack : cWhite);
        addPageActionTarget(76 + index * 174, 64, 156, 62,
                            &Channel2Ui::controlDriveMode, index);
    }
    lv_obj_t *panel = makePanel(16, 150, 448, 66);
    makeLabel(panel, "FORWARD LIMIT", 14, 20, 18, cCyan);
    char value[12]; snprintf(value, sizeof(value), "%u%%", driveModeSettings_.walkLimitPercent);
    makeLabel(panel, value, 260, 20, 18);
    makeButton(358, 164, 42, 38, "-", 22);
    makeButton(410, 164, 42, 38, "+", 22);
    addPageEditorTarget(16, 150, 448, 66,
                        358, 164, 42, 38, 410, 164, 42, 38,
                        &Channel2Ui::controlDriveMode, 2);
    makeLabel(lv_scr_act(),
        "WALK limits forward only; brake/reverse stays at 100%.\nChanging mode forces CUT; release trigger before arming.",
        20, 242, 14, cAmber);
    finishPageFocus();
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
    beginPageFocus();
    for (int row = 0; row < 4; ++row) drawSetupRow(row);
    addPageActionTarget(353, 60, 106, 36,
                        &Channel2Ui::controlSetup, 0);
    addPageActionTarget(353, 110, 106, 36,
                        &Channel2Ui::controlSetup, 1);
    addPageEditorTarget(10, 156, 460, 44,
                        353, 159, 48, 36, 411, 159, 48, 36,
                        &Channel2Ui::controlSetup, 2);
    addPageEditorTarget(10, 206, 460, 44,
                        353, 209, 48, 36, 411, 209, 48, 36,
                        &Channel2Ui::controlSetup, 3);
    makeButton(10, 265, 88, 44, "TEST", 14, cPanel2, cAmber);
    addPageActionTarget(10, 265, 88, 44,
                        &Channel2Ui::controlSetup, 4);
    makeButton(106, 265, 110, 44, "DISPLAY", 13, cPanel2, cCyan);
    addPageActionTarget(106, 265, 110, 44,
                        &Channel2Ui::controlSetup, 5);
    makeButton(224, 265, 116, 44, "TX BAT", 13, cPanel2, cCyan);
    addPageActionTarget(224, 265, 116, 44,
                        &Channel2Ui::controlSetup, 6);
    makeButton(348, 265, 122, 44, "RX BAT", 13, cPanel2, cCyan);
    addPageActionTarget(348, 265, 122, 44,
                        &Channel2Ui::controlSetup, 7);
    finishPageFocus();
}

void Channel2Ui::drawElrsBindingPage()
{
    activePage_ = UiPage::ElrsBinding;
    startScreen();
    resetLvglObjects();
    makeHeader("ELRS BINDING", "BACK");

    lv_obj_t *phrasePanel = makePanel(8, 47, 464, 43);
    char phraseText[64];
    snprintf(phraseText, sizeof(phraseText), "PHRASE: %s",
             elrsBindPhrase_[0] == '\0' ? "<blank / unbound>" : elrsBindPhrase_);
    makeLabel(phrasePanel, phraseText, 8, 3, 13, cWhite);
    makeLabel(phrasePanel, elrsStatus_, 8, 22, 11,
              strstr(elrsStatus_, "MATCH") != nullptr ? cGreen : cAmber);

    const char *actions[] = {"SET TX", "SET RX", "UID TX", "UID RX", "CLEAR"};
    for (uint8_t index = 0; index < 5; ++index) {
        const bool unavailable = (index == 1 || index == 3) &&
                                 !elrsReceiverLinkAvailable_;
        makeButton(8 + index * 93, 94, 87, 32, actions[index], 11,
                   unavailable ? cBlack : cPanel2,
                   unavailable ? cDim : (index < 2 ? cAmber : cCyan));
    }

    const char *rows[] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm-_"};
    const uint8_t lengths[] = {10, 10, 9, 9};
    for (uint8_t row = 0; row < 4; ++row) {
        constexpr int16_t width = 42;
        const int16_t startX = (kScreenWidth - lengths[row] * width) / 2;
        for (uint8_t column = 0; column < lengths[row]; ++column) {
            char label[2] = {rows[row][column], '\0'};
            if (elrsKeyboardUppercase_ && label[0] >= 'a' && label[0] <= 'z')
                label[0] = static_cast<char>(label[0] - 'a' + 'A');
            makeButton(startX + column * width, 130 + row * 38,
                       width - 3, 34, label, 14, cPanel, cWhite);
        }
    }
    makeButton(43, 282, 92, 32,
               elrsKeyboardUppercase_ ? "lower" : "UPPER", 12);
    makeButton(143, 282, 194, 32, "SPACE", 12);
    makeButton(345, 282, 92, 32, "<--", 14, cPanel2, cAmber);
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
    beginPageFocus();
    for (int row = 0; row < 4; ++row) {
        drawDisplayRow(row);
        if (row < 2) {
            addPageEditorTarget(10, 56 + row * 50, 460, 44,
                                353, 60 + row * 50, 48, 36,
                                411, 60 + row * 50, 48, 36,
                                &Channel2Ui::controlDisplay, row);
        } else {
            addPageActionTarget(10, 56 + row * 50, 460, 44,
                                &Channel2Ui::controlDisplay, row);
        }
    }
    makeLabel(lv_scr_act(), "UI dimming is visual; full backlight saving needs a BL switch.",
              20, 280, 12, cDim);
    finishPageFocus();
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
    beginPageFocus();
    for (int row = 0; row < 4; ++row) {
        drawTxBatteryRow(row);
        if (row == 1 || row == 2) {
            addPageEditorTarget(10, 56 + row * 50, 460, 44,
                                343, 59 + row * 50, 48, 36,
                                401, 59 + row * 50, 48, 36,
                                &Channel2Ui::controlTxBattery, row);
        } else {
            addPageActionTarget(343, 59 + row * 50, 106, 36,
                                &Channel2Ui::controlTxBattery, row);
        }
    }
    lv_obj_t *note = makeLabel(lv_scr_act(),
        "GPIO5 via 47k/10k divider", 0, 280, 12, cDim);
    lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 280);
    finishPageFocus();
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
    beginPageFocus();
    for (int row = 0; row < 4; ++row) {
        drawRxBatteryRow(row);
        if (row == 1 || row == 2) {
            addPageEditorTarget(10, 56 + row * 50, 460, 44,
                                353, 60 + row * 50, 48, 36,
                                411, 60 + row * 50, 48, 36,
                                &Channel2Ui::controlRxBattery, row);
        } else {
            addPageActionTarget(353, 60 + row * 50, 106, 36,
                                &Channel2Ui::controlRxBattery, row);
        }
    }
    lv_obj_t *note = makeLabel(lv_scr_act(),
        "Voltage received from ER6G telemetry", 0, 280, 12, cDim);
    lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 280);
    finishPageFocus();
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
    makeLabel(panel, "MIN AVG", 16, 10, 14, cDim);
    makeLabel(panel, "LIVE", 180, 10, 14, cDim);
    makeLabel(panel, "MAX AVG", 326, 10, 14, cDim);
    calibrationMinimumLabel_ = makeLabel(panel, "----", 10, 42, 24);
    calibrationLiveLabel_ = makeLabel(panel, "----", 168, 42, 24);
    calibrationMaximumLabel_ = makeLabel(panel, "----", 326, 42, 24);
    drawCalibrationValues(latestRawAdc_);
}

void Channel2Ui::drawCalibrationValues(uint16_t adc)
{
    (void)adc;
    if (!triggerInput_.calibrationActive() || !calibrationLiveLabel_) return;
    const ProportionalCalibrationProgress progress =
        triggerInput_.calibrationProgress();
    lv_label_set_text_fmt(calibrationMinimumLabel_, "%4u", progress.minimum);
    lv_label_set_text_fmt(calibrationLiveLabel_, "%4u", progress.live);
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
#ifdef JR_R1_SIMULATOR
    (void)command;
    return 0;
#else
#if DISPLAY_PROFILE == DISPLAY_PROFILE_BTT_TFT35_SPI
    Wire.beginTransmission(NS2009_I2C_ADDRESS);
    Wire.write(command);
    if (Wire.endTransmission() != 0) return 0;
    if (Wire.requestFrom(NS2009_I2C_ADDRESS, static_cast<uint8_t>(2)) != 2) return 0;
    return static_cast<uint16_t>((Wire.read() << 4) | (Wire.read() >> 4));
#else
    (void)command;
    return 0;
#endif
#endif
}

bool Channel2Ui::readGt911Register(uint16_t reg, uint8_t *data, size_t length)
{
#ifdef JR_R1_SIMULATOR
    (void)reg; (void)data; (void)length;
    return false;
#else
#if DISPLAY_PROFILE == DISPLAY_PROFILE_WAVESHARE_RPI_35F
    Wire.beginTransmission(GT911_I2C_ADDRESS);
    Wire.write(static_cast<uint8_t>(reg >> 8));
    Wire.write(static_cast<uint8_t>(reg));
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(GT911_I2C_ADDRESS, length) != length) {
        while (Wire.available()) Wire.read();
        return false;
    }
    for (size_t index = 0; index < length; ++index)
        data[index] = static_cast<uint8_t>(Wire.read());
    return true;
#else
    (void)reg; (void)data; (void)length;
    return false;
#endif
#endif
}

bool Channel2Ui::writeGt911Register(uint16_t reg, uint8_t value)
{
#ifdef JR_R1_SIMULATOR
    (void)reg; (void)value;
    return false;
#else
#if DISPLAY_PROFILE == DISPLAY_PROFILE_WAVESHARE_RPI_35F
    Wire.beginTransmission(GT911_I2C_ADDRESS);
    Wire.write(static_cast<uint8_t>(reg >> 8));
    Wire.write(static_cast<uint8_t>(reg));
    Wire.write(value);
    return Wire.endTransmission() == 0;
#else
    (void)reg; (void)value;
    return false;
#endif
#endif
}

bool Channel2Ui::initialiseTouchController()
{
#ifdef JR_R1_SIMULATOR
    return true;
#else
#if DISPLAY_PROFILE == DISPLAY_PROFILE_WAVESHARE_RPI_35F
    // Goodix address-select/reset sequence used by Waveshare. Holding INT low
    // when reset is released selects address 0x5d.
    pinMode(TOUCH_IRQ_PIN, OUTPUT);
    pinMode(TOUCH_RESET_PIN, OUTPUT);
    digitalWrite(TOUCH_IRQ_PIN, LOW);
    digitalWrite(TOUCH_RESET_PIN, LOW);
    delay(11);
    digitalWrite(TOUCH_IRQ_PIN, LOW);
    delayMicroseconds(110);
    digitalWrite(TOUCH_RESET_PIN, HIGH);
    delay(6);
    digitalWrite(TOUCH_IRQ_PIN, LOW);
    delay(55);
    pinMode(TOUCH_IRQ_PIN, INPUT_PULLUP);
    delay(100);

    uint8_t productId[4] = {};
    const bool ready = readGt911Register(0x8140, productId, sizeof(productId));
    if (ready) {
        Serial.printf("TOUCH: GT911 address=0x%02X product=%c%c%c%c\n",
                      GT911_I2C_ADDRESS, productId[0], productId[1],
                      productId[2], productId[3]);
        writeGt911Register(GT911_POINTS_INFO_REGISTER, 0);
    }
    return ready;
#else
    pinMode(TOUCH_IRQ_PIN, INPUT_PULLUP);
    Wire.beginTransmission(NS2009_I2C_ADDRESS);
    const bool ready = Wire.endTransmission() == 0;
    Serial.printf("TOUCH: NS2009 address=0x%02X %s\n", NS2009_I2C_ADDRESS,
                  ready ? "ready" : "not found");
    return ready;
#endif
#endif
}

void Channel2Ui::applyBacklight(uint8_t percent)
{
#ifdef JR_R1_SIMULATOR
    simulatorSetBrightness(percent);
#else
    if (!DISPLAY_HAS_PWM_BACKLIGHT || displaySleeping_) return;
    percent = constrain(percent, 0, 100);
    const uint8_t duty = static_cast<uint8_t>((percent * 255U + 50U) / 100U);
    ledcWrite(TFT_BACKLIGHT_LEDC_CHANNEL, duty);
#endif
}

bool Channel2Ui::readTouch(int16_t &x, int16_t &y, uint16_t &rawX,
                           uint16_t &rawY, uint16_t &rawZ)
{
#ifdef JR_R1_SIMULATOR
    return simulatorReadTouch(x, y, rawX, rawY, rawZ);
#else
#if DISPLAY_PROFILE == DISPLAY_PROFILE_WAVESHARE_RPI_35F
    uint8_t status = 0;
    if (!readGt911Register(GT911_POINTS_INFO_REGISTER, &status, 1)) return false;
    if ((status & 0x80U) == 0) return false;

    const uint8_t touches = status & 0x0fU;
    if (touches == 0 || touches > 5) {
        writeGt911Register(GT911_POINTS_INFO_REGISTER, 0);
        return false;
    }

    uint8_t point[7] = {};
    const bool pointReady = readGt911Register(
        GT911_FIRST_POINT_REGISTER, point, sizeof(point));
    writeGt911Register(GT911_POINTS_INFO_REGISTER, 0);
    if (!pointReady) return false;

    rawX = static_cast<uint16_t>(point[1] | (point[2] << 8U));
    rawY = static_cast<uint16_t>(point[3] | (point[4] << 8U));
    rawZ = static_cast<uint16_t>(point[5] | (point[6] << 8U));

    // Match the Waveshare example's rotation 3: landscape with the connector
    // orientation used by the ST7796 display profile.
    x = constrain(static_cast<int32_t>(kScreenWidth - 1 - rawY),
                  0L, static_cast<int32_t>(kScreenWidth - 1));
    y = constrain(static_cast<int32_t>(rawX),
                  0L, static_cast<int32_t>(kScreenHeight - 1));
    return true;
#else
    if (digitalRead(TOUCH_IRQ_PIN) != LOW) return false;
    rawZ = readTouchRegister(kNsReadZ);
    if (rawZ <= kTouchPressureThreshold || rawZ == 0x0fff) return false;
    rawX = readTouchRegister(kNsReadX);
    rawY = readTouchRegister(kNsReadY);
    x = constrain(map(rawY, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX, 0, kScreenWidth - 1), 0, kScreenWidth - 1);
    y = constrain(map(rawX, TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX, kScreenHeight - 1, 0), 0, kScreenHeight - 1);
    return true;
#endif
#endif
}

void Channel2Ui::handleDashboardTouch(int16_t x, int16_t y)
{
    if ((y < 45 && x >= 415) || (y >= 245 && x < 120)) { playKeyClick(); drawMainMenu(); }
    else if (y >= 245 && x >= 120 && x < 236) { playKeyClick(); drawTelemetryPage(); }
    else if (y >= 245 && x >= 236 && x < 352) {
        driveModesReturnPage_ = UiPage::Dashboard;
        playKeyClick(); drawDriveModesPage();
    }
    else if (y >= 245 && x >= 352) {
        driveModesReturnPage_ = UiPage::Dashboard;
        playKeyClick(); drawDriveModesPage();
    }
    else if (x >= 310 && y >= 45 && y < 140) {
        if (!channelRoleSettings_.throttleEnabled) {
            playKeyClick();
            driveModesReturnPage_ = UiPage::Dashboard;
            drawDriveModesPage();
        } else {
            armRequested_ = !armRequested_;
            safetyResetRequested_ = true;
            playBuzzer(armRequested_ ? 80 : 180);
            lastSafetyStatus_ = OutputSafetyStatus::ControlStale;
        }
    }
}

void Channel2Ui::handleTelemetryTouch(int16_t x, int16_t y)
{
    if (headerActionTouched(x, y)) {
        playKeyClick();
        drawDashboard();
    }
}

void Channel2Ui::handleMainMenuTouch(int16_t x, int16_t y)
{
    if (x < 5 || x >= 475 || y < 52 || y >= 270) return;
    const int item = ((y - 52) / 54) * 2 + (x >= 240 ? 1 : 0);
    if (item < 0 || item >= 8 || !mainMenuItemEnabled(item)) return;
    mainMenuFocusIndex_ = static_cast<uint8_t>(item);
    activateMainMenuFocus();
}

void Channel2Ui::handleChannelsTouch(int16_t x, int16_t y)
{
    if (y < 48 && x >= 250 && x < 340) {
        playKeyClick(); drawChannelRolesPage(); return;
    }
    if (headerActionTouched(x, y)) { playKeyClick(); drawMainMenu(); return; }
    if (y >= 270) {
        if (x < 90 && channelsBank_ > 0) { --channelsBank_; playKeyClick(); drawChannelsPage(); }
        else if (x >= 390 && channelsBank_ < 1) { ++channelsBank_; playKeyClick(); drawChannelsPage(); }
        return;
    }
    if (y < 52 || y >= 264) return;
    selectedChannel_ = channelsBank_ * 8 + ((y - 52) / 52) * 2 + (x >= 240 ? 1 : 0) + 1;
    playKeyClick();
    drawChannelPlaceholder();
}

void Channel2Ui::handleChannelConfigTouch(int16_t x, int16_t y)
{
    if (headerActionTouched(x, y)) {
        playKeyClick(); drawChannelsPage(); return;
    }
    if (selectedChannel_ == channelRoleSettings_.throttleChannel &&
        y >= 244 && y < 304 && x >= 110 && x < 370) {
        playKeyClick(); drawPage(); return;
    }
    if (x < 340 || y < 72 || y >= 212) return;
    const uint8_t row = static_cast<uint8_t>((y - 76) / 66);
    if (row > 1) return;
    const ChannelParameterKind kind = row == 0
        ? ChannelParameterKind::Trim : ChannelParameterKind::Subtrim;
    const PhysicalControlId next = adjacentChannelParameterControl(
        selectedChannel_, kind, x < 400 ? -1 : 1);
    if (assignChannelParameterControl(selectedChannel_, kind, next)) {
        playKeyClick();
        drawChannelPlaceholder();
    } else {
        playBuzzer(180);
    }
}

void Channel2Ui::handleChannelRolesTouch(int16_t x, int16_t y)
{
    if (headerActionTouched(x, y)) { playKeyClick(); drawChannelsPage(); return; }
    if (y < 72 || y >= 196 || x < 340) return;
    const int row = y < 140 ? 0 : 1;
    const int8_t direction = x < 400 ? -1 : 1;
    uint8_t &roleChannel = row == 0 ? channelRoleSettings_.steeringChannel :
                                      channelRoleSettings_.throttleChannel;
    const uint8_t otherRole = row == 0 ? channelRoleSettings_.throttleChannel :
                                        channelRoleSettings_.steeringChannel;
    for (uint8_t attempts = 0; attempts < 16; ++attempts) {
        roleChannel = adjacentChannel(roleChannel, direction, otherRole);
        if (!bindingUsesChannel(roleChannel, PhysicalControlId::Count)) break;
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
    if (headerActionTouched(x, y)) {
        playKeyClick();
        if (throttleResponseReturnPage_ == UiPage::Channel2) drawPage();
        else if (throttleResponseReturnPage_ == UiPage::Dashboard) drawDashboard();
        else drawMainMenu();
        return;
    }
    if (y < 48 && x >= 250 && x < 340) { playKeyClick(); drawChannelSpeedPage(); return; }
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
    if (headerActionTouched(x, y)) { playKeyClick(); drawThrottleResponsePage(); return; }
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
    if (headerActionTouched(x, y)) { playKeyClick(); drawMainMenu(); return; }
    constexpr uint8_t kRows = 5;
    constexpr uint8_t kBanks = (kPhysicalControlCount + kRows - 1) / kRows;
    if (y >= 258) {
        if (x < 60 && assignmentBank_ > 0) {
            --assignmentBank_; playKeyClick(); drawControlAssignmentsPage();
        } else if (x >= 70 && x < 164) {
            playKeyClick(); drawChannelRolesPage();
        } else if (x >= 174 && x < 306) {
            inputMonitorBank_ = 0; playKeyClick(); drawInputMonitorPage();
        } else if (x >= 420 && assignmentBank_ + 1 < kBanks) {
            ++assignmentBank_; playKeyClick(); drawControlAssignmentsPage();
        }
        return;
    }
    if (y < 50 || y >= 250) return;
    const size_t inputIndex = assignmentBank_ * kRows + (y - 50) / 40;
    if (inputIndex >= kPhysicalControlCount) return;
    const PhysicalControlDescriptor &control = physicalControlDescriptor(inputIndex);
    playKeyClick();
    if (control.kind == PhysicalControlKind::Proportional) {
        drawChannelRolesPage();
        return;
    }
    selectedBindingInput_ = control.id;
    const int existing = bindingIndexForInput(control.id);
    bindingDraft_ = existing >= 0 ? bindings_[existing] :
                                    defaultBindingForInput(control.id);
    if (existing < 0)
        bindingDraft_.crsfChannel = adjacentBindingChannel(2, 1);
    bindingError_[0] = '\0';
    drawBindingEditorPage();
}

void Channel2Ui::handleBindingEditorTouch(int16_t x, int16_t y)
{
    if (headerActionTouched(x, y) || (y >= 270 && x < 150)) {
        playKeyClick(); drawControlAssignmentsPage(); return;
    }
    const PhysicalControlKind kind = physicalControlDescriptor(
        static_cast<size_t>(selectedBindingInput_)).kind;
    const bool parameter = bindingDraft_.target == AssignmentTarget::ModelParameter &&
        isChannelParameterTarget(bindingDraft_.targetId);
    if (y >= 88 && y < 124 && x >= 330) {
        bindingDraft_.enabled = !bindingDraft_.enabled;
    } else if (y >= 127 && y < 163 && x >= 330) {
        const int8_t direction = x < 400 ? -1 : 1;
        const AssignmentTarget next = adjacentBindingTarget(
            kind, bindingDraft_.target, direction);
        if (next == AssignmentTarget::CrsfChannel) {
            bindingDraft_.target = AssignmentTarget::CrsfChannel;
            bindingDraft_.targetId = 0;
            bindingDraft_.crsfChannel = adjacentBindingChannel(2, 1);
            bindingDraft_.behaviour = kind == PhysicalControlKind::MaintainedSwitch
                ? ControlBehaviour::TwoPosition
                : kind == PhysicalControlKind::SpringCentredTrim
                ? ControlBehaviour::SpringReturnThreePosition
                : ControlBehaviour::Momentary;
            bindingDraft_.event = kind == PhysicalControlKind::SpringCentredTrim
                ? InputEvent::PositivePress : InputEvent::Pressed;
            bindingDraft_.minimum = -1000;
            bindingDraft_.maximum = 1000;
            bindingDraft_.step = 50;
            bindingDraft_.initial = 0;
            bindingDraft_.failsafe = 0;
            bindingDraft_.persistent = false;
        } else if (next == AssignmentTarget::ModelParameter) {
            const uint8_t channel = selectedBindingInput_ ==
                    PhysicalControlId::ThrottleTrim
                ? channelRoleSettings_.throttleChannel
                : selectedBindingInput_ == PhysicalControlId::Aux3Trim
                ? static_cast<uint8_t>(3)
                : channelRoleSettings_.steeringChannel;
            bindingDraft_.target = AssignmentTarget::ModelParameter;
            bindingDraft_.targetId = encodeChannelParameterTarget(
                channel, ChannelParameterKind::Trim);
            bindingDraft_.crsfChannel = 0;
            bindingDraft_.behaviour = ControlBehaviour::Stepped;
            bindingDraft_.event = InputEvent::PositivePress;
            bindingDraft_.minimum = -kChannelParameterLimitPermille;
            bindingDraft_.maximum = kChannelParameterLimitPermille;
            bindingDraft_.step = 10;
            bindingDraft_.initial = channelParameterValue(
                channel, ChannelParameterKind::Trim);
            bindingDraft_.failsafe = bindingDraft_.initial;
            bindingDraft_.persistent = true;
        } else if (next == AssignmentTarget::TransmitterFunction) {
            bindingDraft_.target = next;
            bindingDraft_.targetId = static_cast<uint16_t>(
                TransmitterFunction::OpenMenu);
            bindingDraft_.crsfChannel = 0;
            bindingDraft_.behaviour = kind == PhysicalControlKind::MaintainedSwitch
                ? ControlBehaviour::TwoPosition : ControlBehaviour::Momentary;
            bindingDraft_.event = InputEvent::Pressed;
            bindingDraft_.minimum = -1000;
            bindingDraft_.maximum = 1000;
            bindingDraft_.step = 50;
            bindingDraft_.initial = -1000;
            bindingDraft_.failsafe = -1000;
            bindingDraft_.persistent = false;
        } else if (next == AssignmentTarget::DriveMode) {
            bindingDraft_.target = next;
            bindingDraft_.targetId = static_cast<uint16_t>(
                DriveModeSelection::WalkNormal);
            bindingDraft_.crsfChannel = 0;
            bindingDraft_.behaviour = ControlBehaviour::TwoPosition;
            bindingDraft_.event = InputEvent::Pressed;
            bindingDraft_.minimum = -1000;
            bindingDraft_.maximum = 1000;
            bindingDraft_.step = 50;
            bindingDraft_.initial = -1000;
            bindingDraft_.failsafe = -1000;
            bindingDraft_.persistent = false;
        }
    } else if (y >= 166 && y < 202 && x >= 330) {
        const int8_t direction = x < 400 ? -1 : 1;
        if (parameter) {
            uint8_t channel = channelParameterTargetChannel(bindingDraft_.targetId);
            channel = static_cast<uint8_t>(
                ((static_cast<int>(channel) - 1 + direction + 16) % 16) + 1);
            const ChannelParameterKind parameterKind = channelParameterTargetKind(
                bindingDraft_.targetId);
            bindingDraft_.targetId = encodeChannelParameterTarget(channel,
                                                                   parameterKind);
            bindingDraft_.initial = channelParameterValue(channel, parameterKind);
            bindingDraft_.failsafe = bindingDraft_.initial;
        } else if (bindingDraft_.target == AssignmentTarget::TransmitterFunction) {
            size_t selected = 0;
            for (size_t index = 0;
                 index < sizeof(kEditableTransmitterFunctions) /
                             sizeof(kEditableTransmitterFunctions[0]); ++index)
                if (static_cast<uint16_t>(kEditableTransmitterFunctions[index]) ==
                    bindingDraft_.targetId) selected = index;
            const size_t count = sizeof(kEditableTransmitterFunctions) /
                                 sizeof(kEditableTransmitterFunctions[0]);
            selected = static_cast<size_t>((static_cast<int>(selected) + direction +
                                            static_cast<int>(count)) %
                                           static_cast<int>(count));
            bindingDraft_.targetId = static_cast<uint16_t>(
                kEditableTransmitterFunctions[selected]);
            if (kind == PhysicalControlKind::MaintainedSwitch) {
                bindingDraft_.behaviour = ControlBehaviour::TwoPosition;
            } else {
                bindingDraft_.behaviour =
                    (kEditableTransmitterFunctions[selected] ==
                         TransmitterFunction::DisplayOnOff ||
                     kEditableTransmitterFunctions[selected] ==
                         TransmitterFunction::ArmCut)
                    ? ControlBehaviour::Toggle : ControlBehaviour::Momentary;
            }
        } else if (bindingDraft_.target == AssignmentTarget::DriveMode) {
            size_t selected = 0;
            for (size_t index = 0;
                 index < sizeof(kEditableDriveModeSelections) /
                             sizeof(kEditableDriveModeSelections[0]); ++index)
                if (static_cast<uint16_t>(kEditableDriveModeSelections[index]) ==
                    bindingDraft_.targetId) selected = index;
            const size_t count = sizeof(kEditableDriveModeSelections) /
                                 sizeof(kEditableDriveModeSelections[0]);
            selected = static_cast<size_t>((static_cast<int>(selected) + direction +
                                            static_cast<int>(count)) %
                                           static_cast<int>(count));
            bindingDraft_.targetId = static_cast<uint16_t>(
                kEditableDriveModeSelections[selected]);
        } else {
            bindingDraft_.crsfChannel = adjacentBindingChannel(
                bindingDraft_.crsfChannel, direction);
        }
    } else if (y >= 205 && y < 241 && x >= 330) {
        if (parameter) {
            const uint8_t channel = channelParameterTargetChannel(
                bindingDraft_.targetId);
            const ChannelParameterKind next =
                channelParameterTargetKind(bindingDraft_.targetId) ==
                        ChannelParameterKind::Trim
                    ? ChannelParameterKind::Subtrim : ChannelParameterKind::Trim;
            bindingDraft_.targetId = encodeChannelParameterTarget(channel, next);
            bindingDraft_.initial = channelParameterValue(channel, next);
            bindingDraft_.failsafe = bindingDraft_.initial;
        } else {
            bindingDraft_.behaviour = adjacentBindingBehaviour(
                kind, bindingDraft_.behaviour, x < 400 ? -1 : 1);
        }
    } else if (y >= 270 && x >= 160 && x < 300 &&
               bindingIndexForInput(selectedBindingInput_) >= 0) {
        if (removeSelectedBinding()) {
            playBuzzer(80); drawControlAssignmentsPage();
        }
        return;
    } else if (y >= 270 && x >= 300) {
        if (saveBindingDraft()) {
            playBuzzer(80); drawControlAssignmentsPage();
        } else {
            playBuzzer(180); drawBindingEditorPage();
        }
        return;
    } else return;
    bindingError_[0] = '\0';
    playKeyClick();
    drawBindingEditorPage();
}

void Channel2Ui::handleInputMonitorTouch(int16_t x, int16_t y)
{
    constexpr uint8_t kRowsPerBank = 5;
    constexpr uint8_t kBankCount =
        (kPhysicalControlCount + kRowsPerBank - 1) / kRowsPerBank;
    if (headerActionTouched(x, y)) {
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
    if (headerActionTouched(x, y)) {
        playKeyClick();
        if (driveModesReturnPage_ == UiPage::Dashboard) drawDashboard();
        else drawMainMenu();
        return;
    }
    if (y >= 60 && y < 132) {
        if (x < 70 || x >= 415) return;
        const uint8_t index = x < 245 ? 0 : 1;
        driveModeSettings_.activeMode = static_cast<DriveMode>(index);
        armRequested_ = false;
        safetyResetRequested_ = true;
        playBuzzer(80);
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
    if (headerActionTouched(x, y)) { playKeyClick(); drawMainMenu(); return; }
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

void Channel2Ui::appendElrsKeyboardCharacter(char character)
{
    const size_t length = strlen(elrsBindPhrase_);
    if (length >= kElrsMaximumBindPhraseLength) {
        snprintf(elrsStatus_, sizeof(elrsStatus_), "Maximum is 51 characters");
        return;
    }
    elrsBindPhrase_[length] = character;
    elrsBindPhrase_[length + 1] = '\0';
    snprintf(elrsStatus_, sizeof(elrsStatus_), "Edited locally; not sent yet");
}

void Channel2Ui::queueElrsRequest(ElrsManagementAction action,
                                  ElrsManagementTarget target)
{
    if (target == ElrsManagementTarget::Receiver && !elrsReceiverLinkAvailable_) {
        snprintf(elrsStatus_, sizeof(elrsStatus_), "RX unavailable: no live RF link");
        return;
    }
    if (action == ElrsManagementAction::SetBindPhrase &&
        elrsBindPhrase_[0] == '\0') {
        snprintf(elrsStatus_, sizeof(elrsStatus_), "Blank would unbind; enter a phrase");
        return;
    }
    if (elrsRequestPending_) {
        snprintf(elrsStatus_, sizeof(elrsStatus_), "Management request already queued");
        return;
    }
    elrsPendingRequest_ = ElrsManagementRequest{};
    elrsPendingRequest_.action = action;
    elrsPendingRequest_.target = target;
    elrsPendingRequest_.phraseLength = static_cast<uint8_t>(strlen(elrsBindPhrase_));
    memcpy(elrsPendingRequest_.phrase, elrsBindPhrase_,
           elrsPendingRequest_.phraseLength + 1);
    elrsRequestPending_ = true;
    armRequested_ = false;
    safetyResetRequested_ = true;
    if (action == ElrsManagementAction::SetBindPhrase) saveElrsSettings();
    snprintf(elrsStatus_, sizeof(elrsStatus_), "%s queued; outputs CUT",
             action == ElrsManagementAction::SetBindPhrase ? "Bindphrase" : "UID read");
}

void Channel2Ui::handleElrsBindingTouch(int16_t x, int16_t y)
{
    if (headerActionTouched(x, y)) { playKeyClick(); drawMainMenu(); return; }
    if (y >= 94 && y < 128) {
        const uint8_t action = constrain(x / 93, 0, 4);
        if (action == 0) queueElrsRequest(ElrsManagementAction::SetBindPhrase,
                                         ElrsManagementTarget::Transmitter);
        else if (action == 1) queueElrsRequest(ElrsManagementAction::SetBindPhrase,
                                              ElrsManagementTarget::Receiver);
        else if (action == 2) queueElrsRequest(ElrsManagementAction::ReadUid,
                                              ElrsManagementTarget::Transmitter);
        else if (action == 3) queueElrsRequest(ElrsManagementAction::ReadUid,
                                              ElrsManagementTarget::Receiver);
        else {
            elrsBindPhrase_[0] = '\0';
            snprintf(elrsStatus_, sizeof(elrsStatus_), "Entry cleared; module unchanged");
        }
        playKeyClick(); drawElrsBindingPage(); return;
    }

    const char *rows[] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm-_"};
    const uint8_t lengths[] = {10, 10, 9, 9};
    if (y >= 130 && y < 282) {
        const uint8_t row = (y - 130) / 38;
        if (row < 4) {
            constexpr int16_t width = 42;
            const int16_t startX = (kScreenWidth - lengths[row] * width) / 2;
            if (x >= startX) {
                const uint8_t column = (x - startX) / width;
                if (column < lengths[row]) {
                    char character = rows[row][column];
                    if (elrsKeyboardUppercase_ && character >= 'a' && character <= 'z')
                        character = static_cast<char>(character - 'a' + 'A');
                    appendElrsKeyboardCharacter(character);
                    playKeyClick(); drawElrsBindingPage(); return;
                }
            }
        }
    }
    if (y >= 282) {
        if (x < 140) elrsKeyboardUppercase_ = !elrsKeyboardUppercase_;
        else if (x < 340) appendElrsKeyboardCharacter(' ');
        else {
            const size_t length = strlen(elrsBindPhrase_);
            if (length != 0) elrsBindPhrase_[length - 1] = '\0';
            snprintf(elrsStatus_, sizeof(elrsStatus_), "Edited locally; not sent yet");
        }
        playKeyClick(); drawElrsBindingPage();
    }
}

void Channel2Ui::handleDisplayTouch(int16_t x, int16_t y)
{
    if (headerActionTouched(x, y)) { playKeyClick(); drawSetupPage(); return; }
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
        applyBacklight(uiBrightnessPercent);
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
    if (headerActionTouched(x, y)) { playKeyClick(); drawSetupPage(); return; }
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
    if (headerActionTouched(x, y)) { playKeyClick(); drawSetupPage(); return; }
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
    if (activePage_ != UiPage::Dashboard && headerHomeTouched(x, y)) {
        if (triggerInput_.calibrationActive()) {
            triggerInput_.cancelCalibration();
            armRequested_ = false;
            safetyResetRequested_ = true;
            Serial.println("CAL: cancelled by Home; previous calibration retained");
        }
        playKeyClick();
        drawDashboard();
        return;
    }
    if (triggerInput_.calibrationActive()) {
        if (headerActionTouched(x, y)) cancelCalibration();
        return;
    }
    if (dispatchPageControlTouch(x, y)) return;
    if (activePage_ == UiPage::Dashboard) { handleDashboardTouch(x, y); return; }
    if (activePage_ == UiPage::Telemetry) { handleTelemetryTouch(x, y); return; }
    if (activePage_ == UiPage::MainMenu) { handleMainMenuTouch(x, y); return; }
    if (activePage_ == UiPage::Channels) { handleChannelsTouch(x, y); return; }
    if (activePage_ == UiPage::ChannelRoles) { handleChannelRolesTouch(x, y); return; }
    if (activePage_ == UiPage::ThrottleResponse) { handleThrottleResponseTouch(x, y); return; }
    if (activePage_ == UiPage::ChannelSpeed) { handleChannelSpeedTouch(x, y); return; }
    if (activePage_ == UiPage::ControlAssignments) { handleControlAssignmentsTouch(x, y); return; }
    if (activePage_ == UiPage::BindingEditor) { handleBindingEditorTouch(x, y); return; }
    if (activePage_ == UiPage::InputMonitor) { handleInputMonitorTouch(x, y); return; }
    if (activePage_ == UiPage::DriveModes) { handleDriveModesTouch(x, y); return; }
    if (activePage_ == UiPage::ChannelPlaceholder) {
        handleChannelConfigTouch(x, y); return;
    }
    if (activePage_ == UiPage::Setup) { handleSetupTouch(x, y); return; }
    if (activePage_ == UiPage::TxBatterySetup) { handleTxBatteryTouch(x, y); return; }
    if (activePage_ == UiPage::RxBatterySetup) { handleRxBatteryTouch(x, y); return; }
    if (activePage_ == UiPage::DisplaySetup) { handleDisplayTouch(x, y); return; }
    if (activePage_ == UiPage::ElrsBinding) { handleElrsBindingTouch(x, y); return; }
    if (y < 48 && x >= 186 && x < 262) {
        throttleResponseReturnPage_ = UiPage::Channel2;
        playKeyClick(); drawThrottleResponsePage(); return;
    }
    if (y < 48 && x >= 262 && x < 340) { playKeyClick(); drawChannelsPage(); return; }
    if (headerActionTouched(x, y)) { playKeyClick(); startCalibration(latestRawAdc_); return; }
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
    if (changed) {
        playKeyClick();
        scheduleSettingsSave();
        refreshSettingRowValue(row);
    }
}

void Channel2Ui::updateTouch()
{
    int16_t x = 0, y = 0;
    uint16_t rawX = 0, rawY = 0, rawZ = 0;
    const uint32_t now = millis();
#ifdef JR_R1_SIMULATOR
    const bool irqActive = simulatorTouchDown();
#else
    const bool irqActive = digitalRead(TOUCH_IRQ_PIN) == LOW;
#endif
    if (displaySleeping_) {
        if (static_cast<int32_t>(now - nextDisplayWakePollMs_) < 0) return;
        nextDisplayWakePollMs_ = now + kDisplayWakePollMs;

        // GT911 INT is pulse-oriented on this module, so a normal loop can
        // miss it. Its status register remains latched until readTouch()
        // clears it, making the register the reliable wake source.
        int16_t wakeX = 0, wakeY = 0;
        uint16_t wakeRawX = 0, wakeRawY = 0, wakeRawZ = 0;
        const bool wakeTouch = readTouch(wakeX, wakeY, wakeRawX, wakeRawY,
                                         wakeRawZ);
        if (!wakeTouch) {
            // The GT911 can emit a delayed IRQ pulse after the touch data has
            // already reported release. Require a continuously idle interval
            // so that pulse remains part of the old gesture rather than being
            // mistaken for a new wake touch.
            if (displayWakeIdleSinceMs_ == 0) displayWakeIdleSinceMs_ = now;
            if (now - displayWakeIdleSinceMs_ >= kDisplayWakeArmMs)
                displayWakeArmed_ = true;
        } else if (displayWakeArmed_) {
            wakeDisplay();
            discardTouchUntilRelease_ = true;
            touchWasDown_ = false;
            touchReleaseSinceMs_ = 0;
            touchIgnoreUntilMs_ = now + 250;
        } else {
            displayWakeIdleSinceMs_ = 0;
        }
        return;
    }
    if (discardTouchUntilRelease_) {
        bool stillTouched = irqActive;
        if (DISPLAY_USES_GT911) {
            int16_t discardX = 0, discardY = 0;
            uint16_t discardRawX = 0, discardRawY = 0, discardRawZ = 0;
            stillTouched = readTouch(discardX, discardY, discardRawX,
                                     discardRawY, discardRawZ);
        }
        if (!stillTouched) {
            discardTouchUntilRelease_ = false;
            touchIgnoreUntilMs_ = now + kTouchRetriggerGuardMs;
        }
        return;
    }
    const bool touched = readTouch(x, y, rawX, rawY, rawZ);

    if (touched) {
        if (!touchWasDown_ &&
            static_cast<int32_t>(now - touchIgnoreUntilMs_) < 0) {
            // Never promote a press that began during the guard period after
            // the timer expires. It may be contact bounce or the tail of the
            // gesture that just changed pages, so consume it through release.
            discardTouchUntilRelease_ = true;
            touchReleaseSinceMs_ = 0;
            return;
        }
        // A low-pressure or failed I2C sample while IRQ remains asserted is not
        // a release. Clear the pending-release timer whenever valid pressure
        // returns and keep treating this as the same physical touch.
        touchReleaseSinceMs_ = 0;
        if (!touchWasDown_) {
            touchWasDown_ = true;
            lastTouchX_ = x;
            lastTouchY_ = y;
            Serial.printf("TOUCH down page=%s raw=(%u,%u,%u) screen=(%d,%d)\n",
                          uiPageName(activePage_), rawX, rawY, rawZ, x, y);
        } else {
            // Resistive coordinates often wander as pressure falls at lift-off.
            // A modest low-pass average keeps the hit near the body of the tap
            // instead of blindly using the final, least reliable sample.
            lastTouchX_ = static_cast<int16_t>((lastTouchX_ * 3 + x) / 4);
            lastTouchY_ = static_cast<int16_t>((lastTouchY_ * 3 + y) / 4);
        }
        return;
    }

    if (!touchWasDown_) return;
    if (!DISPLAY_USES_GT911 && irqActive) {
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
                        uint16_t txVoltageDecivolts, bool txVoltageValid,
                        const CrsfChannelOutputModel &channelOutputs,
                        bool channelOutputsLive)
{
    latestRawAdc_ = rawAdc;
    const uint16_t enabledMask = channelOutputs.enabledMask();
    channelOutputs.resolve(channelOutputsLive, dashboardChannelValues_);
    if (enabledMask != dashboardEnabledChannelMask_) {
        dashboardEnabledChannelMask_ = enabledMask;
        if (activePage_ == UiPage::Dashboard) drawDashboard();
    }
    bool redrawElrsPage = false;
    if (elrsReceiverLinkAvailable_ != telemetry.linkValid) {
        elrsReceiverLinkAvailable_ = telemetry.linkValid;
        redrawElrsPage = activePage_ == UiPage::ElrsBinding;
    }
    if (telemetry.transmitterUidValid &&
        telemetry.transmitterUidUpdatedMs != lastTransmitterUidUpdateMs_) {
        lastTransmitterUidUpdateMs_ = telemetry.transmitterUidUpdatedMs;
        uint8_t expected[kElrsUidLength];
        calculateElrsBindPhraseUid(elrsBindPhrase_, expected);
        char uidText[24];
        formatUid(telemetry.transmitterUid, uidText, sizeof(uidText));
        snprintf(elrsStatus_, sizeof(elrsStatus_), "TX UID %s  %s", uidText,
                 memcmp(expected, telemetry.transmitterUid, kElrsUidLength) == 0
                    ? "MATCH" : "DIFFERS");
        redrawElrsPage = activePage_ == UiPage::ElrsBinding;
    }
    if (telemetry.receiverUidValid &&
        telemetry.receiverUidUpdatedMs != lastReceiverUidUpdateMs_) {
        lastReceiverUidUpdateMs_ = telemetry.receiverUidUpdatedMs;
        uint8_t expected[kElrsUidLength];
        calculateElrsBindPhraseUid(elrsBindPhrase_, expected);
        char uidText[24];
        formatUid(telemetry.receiverUid, uidText, sizeof(uidText));
        snprintf(elrsStatus_, sizeof(elrsStatus_), "RX UID %s  %s", uidText,
                 memcmp(expected, telemetry.receiverUid, kElrsUidLength) == 0
                    ? "MATCH" : "DIFFERS");
        redrawElrsPage = activePage_ == UiPage::ElrsBinding;
    }
    if (redrawElrsPage) drawElrsBindingPage();
    updateTouch();
    if (triggerInput_.calibrationActive()) updateCalibration(rawAdc);

    const uint32_t now = millis();
    if (settingsSavePending_ &&
        static_cast<int32_t>(now - settingsSaveDueMs_) >= 0) {
        saveSettings();
    }
    if (channelParameterSavePending_ &&
        static_cast<int32_t>(now - channelParameterSaveDueMs_) >= 0) {
        saveChannelParameters();
    }
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
        else if (activePage_ == UiPage::Telemetry) {
            drawTelemetryLive(telemetry, txVoltageDecivolts, txVoltageValid);
        }
        else if (activePage_ == UiPage::Channel2) drawLiveIndicator(outputPermille, crsfValue);
        else if (activePage_ == UiPage::InputMonitor) refreshInputMonitorRows();
    }
    lv_timer_handler();
}
