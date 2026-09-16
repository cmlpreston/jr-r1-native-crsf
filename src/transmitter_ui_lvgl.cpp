#include "transmitter_ui.h"

#include <Preferences.h>
#include "boot_diagnostics.h"
#include <cmath>
#ifndef JR_R1_SIMULATOR
#include <Arduino_GFX_Library.h>
#include <SPI.h>
#include <Wire.h>
#ifdef JR_R1_XIAO_PCB
#include "pcb_io.h"
#endif
#else
#include "simulator_platform.h"
#endif

#include "config.h"
#include "binding_resolver.h"
#include "firmware_identity.h"
#include "r1_input_map.h"

LV_IMG_DECLARE(hyper7_icon);
LV_IMG_DECLARE(hyper7_widget_icon);
LV_IMG_DECLARE(jr_propo_boot_logo);

namespace {

#ifdef JR_R1_UI_PORTRAIT
constexpr int16_t kScreenWidth = DISPLAY_LANDSCAPE_HEIGHT;
constexpr int16_t kScreenHeight = DISPLAY_LANDSCAPE_WIDTH;
constexpr uint8_t kWaveshareDisplayRotation = 2;
#else
constexpr int16_t kScreenWidth = DISPLAY_LANDSCAPE_WIDTH;
constexpr int16_t kScreenHeight = DISPLAY_LANDSCAPE_HEIGHT;
constexpr uint8_t kWaveshareDisplayRotation = 3;
#endif
constexpr bool portraitUi = kScreenWidth < kScreenHeight;
constexpr uint16_t kCrsfMid = 992;
// Standard CRSF/servo travel. Values 0..2047 are representable, but 172..1811
// maps to the conventional approximately 988..2012 us receiver PWM range.
// UI bars therefore use the standard travel so full trigger looks full-scale.
constexpr uint16_t kCrsfMinimum = 172;
constexpr uint16_t kCrsfMaximum = 1811;
constexpr int16_t kExtendedTravelPermille = 1200;
constexpr uint32_t kUiUpdateMs = 80;
constexpr uint32_t kBootSplashDurationMs = 3000;
constexpr uint32_t kLogoSaverMoveMs = 15000;
constexpr uint8_t kLogoSaverBrightnessPercent = 25;
constexpr int16_t kLogoSaverProportionalWakePermille = 40;
constexpr uint16_t kTouchPressureThreshold = 40;
constexpr uint32_t kTouchReleaseDebounceMs = 25;
constexpr uint32_t kTouchRetriggerGuardMs = 100;
constexpr uint32_t kDashboardArmHoldMs = 800;
constexpr uint32_t kDisplayWakeArmMs = 250;
constexpr uint32_t kDisplayWakePollMs = 20;
constexpr uint32_t kSettingsSaveDelayMs = 750;
constexpr uint16_t kBindingProfileVersion = 1;
constexpr uint16_t kChannelParameterVersion = 1;
constexpr uint16_t kResponseProfileVersion = 1;
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
constexpr uint8_t kResponseEndpointStepPercent = 1;
constexpr int8_t kResponseSubtrimStepPercent = 1;
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
uint8_t uiDarkColourScheme = 0;

struct DarkColourScheme {
    const char *name;
    uint32_t background, surface, header, selection, accent, text, muted, line;
};
constexpr DarkColourScheme darkColourSchemes[] = {
    {"Graphite / teal", 0x151b20, 0x202a30, 0x10191d, 0x28433f, 0x58d2c3, 0xedf3f2, 0xaab7bd, 0x3b4a52},
    {"Midnight / ice blue", 0x111827, 0x1d2a3b, 0x0b1220, 0x293e56, 0x82c5ff, 0xeef5ff, 0xb1bfd0, 0x3c4e64},
    {"Warm charcoal / ivory", 0x1c1a18, 0x2c2925, 0x141311, 0x433c30, 0xe1cfab, 0xf3eee6, 0xbfb6a9, 0x4e4840},
    {"Slate / soft lilac", 0x1b1b26, 0x292a39, 0x14151e, 0x413a55, 0xc4b1f6, 0xf0eef8, 0xb9b7ca, 0x48495f},
};
constexpr uint8_t darkColourSchemeCount = sizeof(darkColourSchemes) / sizeof(darkColourSchemes[0]);
const char *colourSchemeName(bool night, uint8_t scheme)
{
    return night ? darkColourSchemes[scheme < darkColourSchemeCount ? scheme : 0].name : "Light";
}

constexpr TransmitterFunction kEditableTransmitterFunctions[] = {
    TransmitterFunction::TimerStartStop,
    TransmitterFunction::TimerReset,
    TransmitterFunction::ResetSpeedStatistics,
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
    return index == 0 || index == 1 || index == 2 || index == 3 || index == 4 ||
           index == 5 || index == 6 || index == 7;
}

const char *modelNamespace(uint8_t slot, const char *legacy,
                           const char *suffix, char *buffer, size_t capacity)
{
    if (slot == 0) return legacy;
    snprintf(buffer, capacity, "r1m%u-%s", slot + 1, suffix);
    return buffer;
}

#ifndef JR_R1_SIMULATOR
Arduino_DataBus *displayBus = new Arduino_ESP32SPI(
    TFT_DC_PIN, TFT_CS_PIN, TFT_SCK_PIN, TFT_MOSI_PIN, TFT_MISO_PIN);
#if DISPLAY_PROFILE == DISPLAY_PROFILE_WAVESHARE_RPI_35F
Arduino_GFX *display = new Arduino_ST7796(
    displayBus, TFT_RESET_PIN, kWaveshareDisplayRotation, true, 320, 480);
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
    if (!uiNightMode) {
        if (rgb == cBlack) rgb = 0xe6e9e7;
        else if (rgb == cHeader) rgb = 0x1b3039;
        else if (rgb == cPanel) rgb = 0xf7f9f7;
        else if (rgb == cPanel2) rgb = 0xd4e8e9;
        else if (rgb == cLine) rgb = 0xbcc6c8;
        else if (rgb == cWhite) rgb = 0x17262e;
        else if (rgb == cDim) rgb = 0x53636c;
        else if (rgb == cCyan) rgb = 0x00707d;
        else if (rgb == cGreen) rgb = 0x176b50;
        else if (rgb == cAmber) rgb = 0x875b0b;
        else if (rgb == cSafetyCut) rgb = 0xf1dca9;
        else if (rgb == cSafetyArmed) rgb = 0xd2ebdf;
        else if (rgb == cSafetyWaiting) rgb = 0xf1dca9;
        else if (rgb == cSafetyFault) rgb = 0xffded7;
    } else {
        const auto &scheme = darkColourSchemes[uiDarkColourScheme < darkColourSchemeCount ? uiDarkColourScheme : 0];
        if (rgb == cBlack) rgb = scheme.background;
        else if (rgb == cHeader) rgb = scheme.header;
        else if (rgb == cPanel) rgb = scheme.surface;
        else if (rgb == cPanel2) rgb = scheme.selection;
        else if (rgb == cLine) rgb = scheme.line;
        else if (rgb == cWhite) rgb = scheme.text;
        else if (rgb == cDim) rgb = scheme.muted;
        else if (rgb == cCyan) rgb = scheme.accent;
    }
    uint16_t scale = DISPLAY_HAS_PWM_BACKLIGHT ? 100 : uiBrightnessPercent;
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
        case 48: return &lv_font_montserrat_48;
        case 32: return &lv_font_montserrat_32;
        case 28: return &lv_font_montserrat_28;
        case 24: return &lv_font_montserrat_24;
        case 22: return &lv_font_montserrat_22;
        case 20: return &lv_font_montserrat_20;
        case 18: return &lv_font_montserrat_18;
        case 16: return &lv_font_montserrat_16;
        case 10: return &lv_font_montserrat_10;
        case 11: case 12: case 13: return &lv_font_montserrat_12;
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
                    uint8_t radius = 2)
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
    // Keep the JR wordmark red in headers and firmware identity labels.
    if (text && strncmp(text, "JR ", 3) == 0) {
        lv_label_set_recolor(label, true);
        lv_label_set_text_fmt(label, "#ff4b43 JR#%s", text + 2);
    } else {
        lv_label_set_text(label, text);
    }
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
    lv_obj_t *button = makePanel(x, y, width, height, fill, border, 2);
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
#ifdef JR_R1_UI_PORTRAIT
    auto titleLabel = makeLabel(header, title, 10, 11, 16, 0xe7edef);
    lv_obj_set_width(titleLabel, 190);
    lv_label_set_long_mode(titleLabel, LV_LABEL_LONG_DOT);
    if (action != nullptr) makeButton(206, 5, 66, 34, action, 11);
    makeButton(278, 5, 36, 34, LV_SYMBOL_HOME, 16);
#else
    auto titleLabel = makeLabel(header, title, 14, 12, 18, 0xe7edef);
    lv_obj_set_width(titleLabel, action ? 318 : 400);
    lv_label_set_long_mode(titleLabel, LV_LABEL_LONG_DOT);
    if (action != nullptr) makeButton(344, 5, 78, 34, action, 12);
    makeButton(430, 5, 42, 34, LV_SYMBOL_HOME, 18);
#endif
}

bool headerActionTouched(int16_t x, int16_t y)
{
#ifdef JR_R1_UI_PORTRAIT
    return y < 48 && x >= 202 && x < 276;
#else
    return y < 48 && x >= 340 && x < 428;
#endif
}

bool headerHomeTouched(int16_t x, int16_t y)
{
#ifdef JR_R1_UI_PORTRAIT
    return y < 48 && x >= 276;
#else
    return y < 48 && x >= 428;
#endif
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

lv_obj_t *makeMarkerTrack(lv_obj_t *parent, int16_t x, int16_t y,
                          int16_t width, int16_t height,
                          uint32_t markerColour, lv_obj_t **markerOut)
{
    lv_obj_t *track = lv_obj_create(parent);
    position(track, x, y, width, height);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(track, 0, 0);
    lv_obj_set_style_bg_color(track, colour(cPanel2), 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(track, colour(cLine), 0);
    lv_obj_set_style_border_width(track, 1, 0);
    lv_obj_set_style_radius(track, 3, 0);

    constexpr int16_t kMarkerWidth = 4;
    lv_obj_t *marker = lv_obj_create(track);
    position(marker, (width - kMarkerWidth) / 2, 2,
             kMarkerWidth, max<int16_t>(4, height - 4));
    lv_obj_clear_flag(marker, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(marker, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(marker, 0, 0);
    lv_obj_set_style_bg_color(marker, colour(markerColour), 0);
    lv_obj_set_style_bg_opa(marker, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(marker, 0, 0);
    lv_obj_set_style_radius(marker, 1, 0);
    if (markerOut != nullptr) *markerOut = marker;
    return track;
}

lv_obj_t *makeVerticalMarkerTrack(lv_obj_t *parent, int16_t x, int16_t y,
                                  int16_t width, int16_t height,
                                  uint32_t markerColour,
                                  lv_obj_t **markerOut)
{
    lv_obj_t *track = lv_obj_create(parent);
    position(track, x, y, width, height);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(track, 0, 0);
    lv_obj_set_style_bg_color(track, colour(cPanel2), 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(track, colour(cLine), 0);
    lv_obj_set_style_border_width(track, 1, 0);
    lv_obj_set_style_radius(track, 3, 0);

    constexpr int16_t kMarkerHeight = 4;
    lv_obj_t *marker = lv_obj_create(track);
    position(marker, 2, (height - kMarkerHeight) / 2,
             max<int16_t>(4, width - 4), kMarkerHeight);
    lv_obj_clear_flag(marker, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(marker, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(marker, 0, 0);
    lv_obj_set_style_bg_color(marker, colour(markerColour), 0);
    lv_obj_set_style_bg_opa(marker, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(marker, 0, 0);
    lv_obj_set_style_radius(marker, 1, 0);
    if (markerOut != nullptr) *markerOut = marker;
    return track;
}

lv_obj_t *makeToggleTrack(lv_obj_t *parent, int16_t x, int16_t y,
                          int16_t width, int16_t height,
                          uint32_t markerColour, lv_obj_t **markerOut)
{
    lv_obj_t *track = lv_obj_create(parent);
    position(track, x, y, width, height);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(track, 0, 0);
    lv_obj_set_style_bg_color(track, colour(cPanel2), 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(track, colour(cLine), 0);
    lv_obj_set_style_border_width(track, 1, 0);
    lv_obj_set_style_radius(track, max<int16_t>(3, width / 2), 0);

    const int16_t thumbWidth = max<int16_t>(4, width - 4);
    constexpr int16_t kThumbHeight = 12;
    lv_obj_t *thumb = lv_obj_create(track);
    position(thumb, 2, height - kThumbHeight - 2,
             thumbWidth, kThumbHeight);
    lv_obj_clear_flag(thumb, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(thumb, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(thumb, 0, 0);
    lv_obj_set_style_bg_color(thumb, colour(markerColour), 0);
    lv_obj_set_style_bg_opa(thumb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(thumb, 0, 0);
    lv_obj_set_style_radius(thumb, max<int16_t>(2, thumbWidth / 2), 0);
    if (markerOut != nullptr) *markerOut = thumb;
    return track;
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
        case UiPage::Tune: return "tune";
        case UiPage::ValueEditor: return "value-editor";
        case UiPage::Gps: return "gps";
        case UiPage::ChannelMonitor: return "channel-monitor";
        case UiPage::SensorDetail: return "sensor-detail";
        case UiPage::WidgetSelect: return "widget-select";
        case UiPage::WidgetEditor: return "widget-editor";
        case UiPage::BatteryMenu: return "batteries";
        case UiPage::RadioDiagnostics: return "radio-diagnostics";
        case UiPage::BootSplash: return "boot";
        case UiPage::LogoSaver: return "logo-saver";
        case UiPage::Dashboard: return "dashboard";
        case UiPage::StartupWarning: return "startup-warning";
        case UiPage::Telemetry: return "telemetry";
        case UiPage::MainMenu: return "main-menu";
        case UiPage::ModelSelect: return "models";
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
        case UiPage::ColourScheme: return "colour-scheme";
        case UiPage::FirmwareInfo: return "firmware-info";
        case UiPage::ElrsBinding: return "elrs-binding";
        case UiPage::ElrsTxConfig: return "elrs-tx-config";
        case UiPage::EscProgramming: return "esc-programming";
        case UiPage::Calibration: return "calibration";
    }
    return "unknown";
}

} // namespace

#include "transmitter_ui_design.inc"
#include "transmitter_ui_portrait.inc"
#ifdef JR_R1_SIMULATOR
#include "../simulator/ui_audit.inc"
#endif

bool TransmitterUi::begin()
{
    loadModelMemorySettings();
    loadChannelRoleSettings();
    loadSettings();
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
#ifdef JR_R1_XIAO_PCB
    // Both expanders must be configured before GT911 address selection or
    // Arduino_GFX startup. Keypad pull-ups are enabled here, before sampling.
    bootDiagnosticsStage("expanders");
    const bool pcbReady = pcbIoBegin();
    bootDiagnosticsResult(BootCheck::Keypad, pcbKeypad.online());
    bootDiagnosticsResult(BootCheck::Controls, pcbControls.online());
    Serial.printf("PCB IO: K1=0x20 %s K2=0x21 %s\n",
                  pcbKeypad.online() ? "ONLINE" : "OFFLINE",
                  pcbControls.online() ? "ONLINE" : "OFFLINE");
    if (pcbReady) bootDiagnosticsStage("touch");
    const bool touchReady = pcbReady && initialiseTouchController();
    if (pcbReady) bootDiagnosticsResult(BootCheck::Touch, touchReady);
    if (pcbReady) bootDiagnosticsStage("LCD-reset");
    const bool lcdResetReady = pcbReady && pcbResetDisplay();
    if (pcbReady) bootDiagnosticsResult(BootCheck::LcdReset, lcdResetReady);
#else
    const bool touchReady = initialiseTouchController();
#endif

    if (DISPLAY_HAS_PWM_BACKLIGHT) {
        ledcSetup(TFT_BACKLIGHT_LEDC_CHANNEL, TFT_BACKLIGHT_PWM_FREQUENCY,
                  TFT_BACKLIGHT_PWM_BITS);
        ledcAttachPin(TFT_BACKLIGHT_PIN, TFT_BACKLIGHT_LEDC_CHANNEL);
        ledcWrite(TFT_BACKLIGHT_LEDC_CHANNEL, 0);
    }

    // Initialise the shared hardware SPI handle only on profiles that route
    // LCD MISO. The production Waveshare cable is deliberately write-only.
    if (DISPLAY_HAS_SPI_READBACK)
        screenshotSpi.begin(TFT_SCK_PIN, TFT_MISO_PIN, TFT_MOSI_PIN, TFT_CS_PIN);
#ifdef JR_R1_XIAO_PCB
    if (lcdResetReady) bootDiagnosticsStage("display-driver");
    displayReady_ = lcdResetReady && display->begin(TFT_SPI_FREQUENCY);
    if (lcdResetReady) bootDiagnosticsResult(BootCheck::Display, displayReady_);
#else
    displayReady_ = display->begin(TFT_SPI_FREQUENCY);
#endif
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

    bootDiagnosticsStage("LVGL");
    lv_init();
    lv_disp_draw_buf_init(&drawBuffer, lineBuffer, nullptr,
                          kScreenWidth * 16);
    lv_disp_drv_init(&displayDriver);
    displayDriver.hor_res = kScreenWidth;
    displayDriver.ver_res = kScreenHeight;
    displayDriver.flush_cb = flushDisplay;
    displayDriver.draw_buf = &drawBuffer;
    lv_disp_drv_register(&displayDriver);

    drawBootSplash();
    lastInteractionMs_ = millis();
    lv_refr_now(nullptr);
    Serial.printf("DISPLAY: %s, %dx%d, SPI=%lu Hz, touch=%s, buffer=%u rows\n",
                  DISPLAY_PROFILE_NAME, kScreenWidth, kScreenHeight,
                  static_cast<unsigned long>(TFT_SPI_FREQUENCY),
                  touchReady ? "ready" : "offline", 16U);
    if (!touchReady)
        Serial.println("DISPLAY: touch controller did not respond; LCD remains usable");
    playBuzzer(80);
#if defined(JR_R1_XIAO_PCB) && !defined(JR_R1_SIMULATOR)
    bootDiagnosticsResult(BootCheck::Ui, touchReady);
    return touchReady;
#else
    return true;
#endif
}

ChannelResponseProfile &TransmitterUi::profileForChannel(uint8_t channel)
{
    channel = constrain(channel, 1, static_cast<int>(CRSF_CHANNEL_COUNT));
    return channelProfiles_[channel - 1];
}

const ChannelResponseProfile &TransmitterUi::profileForChannel(uint8_t channel) const
{
    channel = constrain(channel, 1, static_cast<int>(CRSF_CHANNEL_COUNT));
    return channelProfiles_[channel - 1];
}

ChannelResponseProfile &TransmitterUi::throttleProfile()
{
    return profileForChannel(channelRoleSettings_.throttleChannel);
}

const ChannelResponseProfile &TransmitterUi::throttleProfile() const
{
    return profileForChannel(channelRoleSettings_.throttleChannel);
}

ChannelResponseProfile &TransmitterUi::selectedResponseProfile()
{
    return profileForChannel(selectedChannel_);
}

const ChannelResponseProfile &TransmitterUi::selectedResponseProfile() const
{
    return profileForChannel(selectedChannel_);
}

bool TransmitterUi::channelHasProportionalControl(uint8_t channel) const
{
    if (channel == channelRoleSettings_.steeringChannel ||
        channel == channelRoleSettings_.throttleChannel)
        return true;
    for (size_t index = 0; index < bindingCount_; ++index) {
        const ControlBinding &binding = bindings_[index];
        if (!binding.enabled || binding.target != AssignmentTarget::CrsfChannel ||
            binding.crsfChannel != channel)
            continue;
        const PhysicalControlDescriptor &control = physicalControlDescriptor(
            static_cast<size_t>(binding.input));
        if (control.kind == PhysicalControlKind::Proportional) return true;
    }
    return false;
}

int16_t TransmitterUi::proportionalInputValue(uint8_t channel,
                                           int16_t throttleFallback) const
{
    if (channel == channelRoleSettings_.throttleChannel)
        return throttleFallback;
    PhysicalControlId input = channel == channelRoleSettings_.steeringChannel
        ? PhysicalControlId::SteeringWheel : PhysicalControlId::Count;
    for (size_t index = 0; index < bindingCount_; ++index) {
        const ControlBinding &binding = bindings_[index];
        if (binding.enabled && binding.target == AssignmentTarget::CrsfChannel &&
            binding.crsfChannel == channel &&
            physicalControlDescriptor(static_cast<size_t>(binding.input)).kind ==
                PhysicalControlKind::Proportional) {
            input = binding.input;
            break;
        }
    }
    if (input == PhysicalControlId::Count) return 0;
    return physicalInputStates_[static_cast<size_t>(input)].value;
}

bool TransmitterUi::captureScreenshot(Stream &output)
{
    if (!displayReady_) return false;
#ifndef JR_R1_SIMULATOR
    if (!DISPLAY_HAS_SPI_READBACK) {
        output.print("TFTSHOT-ERROR display readback is not wired on this profile\n");
        output.flush();
        return false;
    }
#endif
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
        if (TFT_CS_PIN >= 0) digitalWrite(TFT_CS_PIN, LOW);

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

        if (TFT_CS_PIN >= 0) digitalWrite(TFT_CS_PIN, HIGH);
        screenshotSpi.endTransaction();
        output.write(screenshotRow, sizeof(screenshotRow));
        delay(0);
    }
#endif
    output.print("\nTFTSHOT-END\n");
    output.flush();
    return true;
}

void TransmitterUi::resetLvglObjects()
{
    for (auto &view : widgetViews_) view = DashboardWidgetView{};
    for (auto &value : designValues_) value = nullptr;
    for (auto &value : designDetails_) value = nullptr;
    for (auto &value : designBars_) value = nullptr;
    designStatus_ = designLink_ = designCurve_ = designMarker_ = nullptr;
    logoSaverLogo_ = nullptr;
    logoSaverDesignation_ = nullptr;
    dashboardThrottleBar_ = nullptr;
    pageFocusIndicator_ = nullptr;
    pageFocusCount_ = 0;
    pageFocusIndex_ = 0;
    for (lv_obj_t *&button : mainMenuButtons_) button = nullptr;
    for (lv_obj_t *&bar : dashboardChannelBars_) bar = nullptr;
    for (lv_obj_t *&marker : dashboardChannelMarkers_) marker = nullptr;
    for (lv_obj_t *&label : dashboardChannelValueLabels_) label = nullptr;
    for (DashboardChannelWidgetKind &kind : dashboardChannelWidgetKinds_)
        kind = DashboardChannelWidgetKind::None;
    dashboardSafetyPanel_ = nullptr;
    dashboardSafetyLabel_ = nullptr;
    dashboardSafetyIcon_ = nullptr;
    dashboardSafetyLockBody_ = nullptr;
    dashboardSafetyLockShackle_ = nullptr;
    dashboardHeader_ = nullptr;
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
    throttleGraphCurve_ = nullptr;
    throttleGraphMarker_ = nullptr;
    throttleGraphLiveLabel_ = nullptr;
    calibrationMinimumLabel_ = nullptr;
    calibrationLiveLabel_ = nullptr;
    calibrationMaximumLabel_ = nullptr;
    for (lv_obj_t *&label : inputMonitorStateLabels_) label = nullptr;
    for (lv_obj_t *&label : inputMonitorEventLabels_) label = nullptr;
}

void TransmitterUi::setPhysicalInputStates(const PhysicalInputStateStore &states)
{
    bool dismissLogoSaver = false;
    for (size_t index = 0; index < kPhysicalControlCount; ++index) {
        const PhysicalInputState &state = states.state(
            static_cast<PhysicalControlId>(index));
        if (activePage_ == UiPage::LogoSaver) {
            const PhysicalControlKind kind = physicalControlDescriptor(index).kind;
            if (kind == PhysicalControlKind::Proportional) {
                dismissLogoSaver = dismissLogoSaver ||
                    abs(static_cast<int>(state.value) -
                        static_cast<int>(logoSaverInputValues_[index])) >=
                        kLogoSaverProportionalWakePermille;
            } else {
                dismissLogoSaver = dismissLogoSaver ||
                    state.stablePrimary != logoSaverInputPrimary_[index] ||
                    state.stableSecondary != logoSaverInputSecondary_[index] ||
                    state.faulted != logoSaverInputFaulted_[index];
            }
        }
        physicalInputStates_[index] = state;
    }
    if (dismissLogoSaver) leaveLogoSaver();
}

void TransmitterUi::setStartupPreflight(bool complete, uint32_t unsafeMask)
{
    const bool changed = startupPreflightComplete_ != complete ||
                         startupWarningMask_ != unsafeMask;
    startupPreflightComplete_ = complete;
    startupWarningMask_ = unsafeMask;
    if (complete) {
        if (activePage_ == UiPage::BootSplash) return;
        if (activePage_ == UiPage::StartupWarning) {
            playBuzzer(80);
            drawDashboard();
        }
        return;
    }
    // A preflight fault is not dismissible and must never preserve an arm
    // request made before switch positions were known.
    armRequested_ = false;
    safetyResetRequested_ = true;
    if (activePage_ == UiPage::BootSplash) return;
    if (changed || activePage_ != UiPage::StartupWarning) {
        playBuzzer(180);
        drawStartupWarningPage();
    }
}

void TransmitterUi::loadModelMemorySettings()
{
    for (uint8_t slot = 0; slot < kModelMemoryCount; ++slot) {
        if (slot == 0) {
            snprintf(modelNames_[slot], kModelNameCapacity, "HYPER 7");
        } else {
            snprintf(modelNames_[slot], kModelNameCapacity, "MODEL %u", slot + 1);
        }
    }
    Preferences preferences;
    preferences.begin("jr-r1-models", true);
    activeModelSlot_ = preferences.getUChar("active", 0);
    if (preferences.getBytesLength("names") == sizeof(modelNames_))
        preferences.getBytes("names", modelNames_, sizeof(modelNames_));
    for (auto &layout : dashboardLayouts_) layout = defaultDashboardLayout();
    if (preferences.getBytesLength("widgets-v3") == sizeof(dashboardLayouts_)) {
        preferences.getBytes("widgets-v3", dashboardLayouts_, sizeof(dashboardLayouts_));
        for (auto &layout : dashboardLayouts_) sanitizeDashboardLayout(layout);
    } else if (preferences.getBytesLength("widgets-v2") == kModelMemoryCount * 4 * 6) {
        uint8_t legacy[kModelMemoryCount][4][6] = {};
        preferences.getBytes("widgets-v2", legacy, sizeof(legacy));
        for (uint8_t model=0;model<kModelMemoryCount;++model) {
            for (uint8_t cell=0;cell<4;++cell)
                dashboardLayouts_[model].cells[cell] = dashboardWidgetFromV2(cell,legacy[model][cell]);
            sanitizeDashboardLayout(dashboardLayouts_[model]);
        }
    } else if (preferences.getBytesLength("widgets-v1") == kModelMemoryCount) {
        uint8_t legacy[kModelMemoryCount] = {};
        preferences.getBytes("widgets-v1", legacy, sizeof(legacy));
        for (uint8_t i=0;i<kModelMemoryCount;++i) {
            if (legacy[i]>=1 && legacy[i]<=6) {
                dashboardLayouts_[i].cells[0].kind = DashboardWidgetKind::Sensor;
                dashboardLayouts_[i].cells[0].source = legacy[i]-1;
            } else if (legacy[i]==7) dashboardLayouts_[i].cells[0].kind = DashboardWidgetKind::Map;
        }
    }
    preferences.end();
    if (activeModelSlot_ >= kModelMemoryCount) activeModelSlot_ = 0;
    for (uint8_t slot = 0; slot < kModelMemoryCount; ++slot)
        modelNames_[slot][kModelNameCapacity - 1] = '\0';
}

void TransmitterUi::saveModelMemorySettings()
{
    Preferences preferences;
    preferences.begin("jr-r1-models", false);
    preferences.putUChar("active", activeModelSlot_);
    preferences.putBytes("names", modelNames_, sizeof(modelNames_));
    preferences.putBytes("widgets-v3", dashboardLayouts_, sizeof(dashboardLayouts_));
    preferences.end();
}

void TransmitterUi::saveCurrentModelMemory()
{
    saveChannelRoleSettings();
    saveSettings();
    saveRxBatterySettings();
    saveDriveModeSettings();
    saveChannelParameters();
    saveBindings();
}

bool TransmitterUi::selectModelMemory(uint8_t slot)
{
    if (slot >= kModelMemoryCount || slot == activeModelSlot_) return true;
    const int throttleDelta = static_cast<int>(latestRawAdc_) -
                              static_cast<int>(triggerInput_.centre());
    if (armRequested_ || abs(throttleDelta) > ARM_NEUTRAL_ADC_WINDOW) {
        snprintf(modelSelectStatus_, sizeof(modelSelectStatus_),
                 "CUT and centre throttle before changing model");
        playBuzzer(180);
        return false;
    }

    armRequested_ = false;
    safetyResetRequested_ = true;
    saveCurrentModelMemory();
    runTimerRunning_ = false; runTimerElapsedMs_ = 0;
    resetSpeedStatistics();
    gpsCount_ = sensorHistoryCount_ = 0;
    gpsLastSampleMs_ = designTelemetry_.gpsUpdatedMs;
    memset(sensorHistoryValid_, 0, sizeof(sensorHistoryValid_));
    activeModelSlot_ = slot;
    saveModelMemorySettings();
    loadChannelRoleSettings();
    loadSettings();
    loadRxBatterySettings();
    loadDriveModeSettings();
    loadChannelParameters();
    loadBindings();
    selectedChannel_ = channelRoleSettings_.throttleChannel;
    bindingsChanged_ = true;
    channelRolesChanged_ = true;
    snprintf(modelSelectStatus_, sizeof(modelSelectStatus_),
             "Model %u loaded; re-arm required", activeModelSlot_ + 1);
    Serial.printf("MODEL: selected %u '%s'; outputs CUT until re-armed\n",
                  activeModelSlot_ + 1, modelNames_[activeModelSlot_]);
    return true;
}

void TransmitterUi::loadSettings()
{
    for (ChannelResponseProfile &profile : channelProfiles_)
        profile = ChannelResponseProfile{};
    Preferences preferences;
    ProportionalCalibration legacyCalibration {
        THROTTLE_ADC_MIN, THROTTLE_ADC_CENTER, THROTTLE_ADC_MAX};
    if (activeModelSlot_ == 0) {
        preferences.begin("jr-r1-ch2", true);
        throttleProfile().negativeEndpointPercent = preferences.getUChar("brake_ep", 100);
        throttleProfile().positiveEndpointPercent = preferences.getUChar("throt_ep", 100);
        throttleProfile().negativeRatePercent = preferences.getUChar("brk_rate", 100);
        throttleProfile().positiveRatePercent = preferences.getUChar("fwd_rate", 100);
        throttleProfile().negativeExpoPercent = preferences.getChar("brk_expo", 0);
        throttleProfile().positiveExpoPercent = preferences.getChar("fwd_expo", 0);
        throttleProfile().negativeRampMs = preferences.getUShort("brk_speed", 0);
        throttleProfile().positiveRampMs = preferences.getUShort("fwd_speed", 0);
        throttleProfile().subtrimPercent = preferences.getChar("trim", 0);
        throttleProfile().reversed = preferences.getBool("reverse", false);
        legacyCalibration = {
            preferences.getUShort("adc_min", THROTTLE_ADC_MIN),
            preferences.getUShort("adc_ctr", THROTTLE_ADC_CENTER),
            preferences.getUShort("adc_max", THROTTLE_ADC_MAX),
        };
        preferences.end();
    }
    // Read the old channel-scoped keys as migration defaults. Calibration is
    // now stored against the physical trigger input, not its assigned output.
    // New builds keep one response profile per CRSF channel. If no versioned
    // profile block exists, the legacy CH2 settings above become the profile
    // for the currently assigned throttle channel and every other channel
    // starts linear at full travel.
    char responseNamespace[16];
    preferences.begin(modelNamespace(activeModelSlot_, "jr-r1-response",
                                     "response", responseNamespace,
                                     sizeof(responseNamespace)), true);
    if (preferences.getUShort("version", 0) == kResponseProfileVersion &&
        preferences.getBytesLength("profiles") == sizeof(channelProfiles_)) {
        preferences.getBytes("profiles", channelProfiles_,
                             sizeof(channelProfiles_));
    }
    preferences.end();

    preferences.begin("jr-r1-trigger", true);
    ProportionalCalibration calibration {
        preferences.getUShort("adc_min", legacyCalibration.minimum),
        preferences.getUShort("adc_ctr", legacyCalibration.centre),
        preferences.getUShort("adc_max", legacyCalibration.maximum),
    };
    preferences.end();

    for (ChannelResponseProfile &profile : channelProfiles_)
        sanitizeChannelResponseProfile(profile);
    if (!calibration.valid()) {
        calibration = {THROTTLE_ADC_MIN, THROTTLE_ADC_CENTER, THROTTLE_ADC_MAX};
    }
    triggerInput_.setCalibration(calibration);

    preferences.begin("jr-r1-steering", true);
    ProportionalCalibration steeringCalibration {
        preferences.getUShort("adc_min", STEERING_ADC_MIN),
        preferences.getUShort("adc_ctr", STEERING_ADC_CENTER),
        preferences.getUShort("adc_max", STEERING_ADC_MAX),
    };
    preferences.end();
    if (!steeringCalibration.valid()) {
        steeringCalibration = {
            STEERING_ADC_MIN, STEERING_ADC_CENTER, STEERING_ADC_MAX};
    }
    steeringInput_.setCalibration(steeringCalibration);
}

void TransmitterUi::saveSettings()
{
    Preferences preferences;
    if (activeModelSlot_ == 0) {
        preferences.begin("jr-r1-ch2", false);
        preferences.putUChar("brake_ep", throttleProfile().negativeEndpointPercent);
        preferences.putUChar("throt_ep", throttleProfile().positiveEndpointPercent);
        preferences.putUChar("brk_rate", throttleProfile().negativeRatePercent);
        preferences.putUChar("fwd_rate", throttleProfile().positiveRatePercent);
        preferences.putChar("brk_expo", throttleProfile().negativeExpoPercent);
        preferences.putChar("fwd_expo", throttleProfile().positiveExpoPercent);
        preferences.putUShort("brk_speed", throttleProfile().negativeRampMs);
        preferences.putUShort("fwd_speed", throttleProfile().positiveRampMs);
        preferences.putChar("trim", throttleProfile().subtrimPercent);
        preferences.putBool("reverse", throttleProfile().reversed);
        preferences.end();
    }

    char responseNamespace[16];
    preferences.begin(modelNamespace(activeModelSlot_, "jr-r1-response",
                                     "response", responseNamespace,
                                     sizeof(responseNamespace)), false);
    preferences.putUShort("version", kResponseProfileVersion);
    preferences.putBytes("profiles", channelProfiles_, sizeof(channelProfiles_));
    preferences.end();
    settingsSavePending_ = false;
}

void TransmitterUi::scheduleSettingsSave()
{
    settingsSavePending_ = true;
    settingsSaveDueMs_ = millis() + kSettingsSaveDelayMs;
}

void TransmitterUi::loadChannelRoleSettings()
{
    Preferences preferences;
    char name[16];
    preferences.begin(modelNamespace(activeModelSlot_, "jr-r1-model1",
                                     "roles", name, sizeof(name)), true);
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

void TransmitterUi::saveChannelRoleSettings()
{
    Preferences preferences;
    char name[16];
    preferences.begin(modelNamespace(activeModelSlot_, "jr-r1-model1",
                                     "roles", name, sizeof(name)), false);
    preferences.putUChar("steer_ch", channelRoleSettings_.steeringChannel);
    preferences.putUChar("throt_ch", channelRoleSettings_.throttleChannel);
    preferences.putBool("steer_en", channelRoleSettings_.steeringEnabled);
    preferences.putBool("throt_en", channelRoleSettings_.throttleEnabled);
    preferences.end();
}

bool TransmitterUi::consumeChannelRolesChanged()
{
    const bool changed = channelRolesChanged_;
    channelRolesChanged_ = false;
    return changed;
}

size_t TransmitterUi::copyBindings(ControlBinding *destination,
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

bool TransmitterUi::consumeBindingsChanged()
{
    const bool changed = bindingsChanged_;
    bindingsChanged_ = false;
    return changed;
}

bool TransmitterUi::consumeSafetyResetRequested()
{
    const bool requested = safetyResetRequested_;
    safetyResetRequested_ = false;
    return requested;
}

bool TransmitterUi::consumeElrsManagementRequest(ElrsManagementRequest &request)
{
    if (!elrsRequestPending_) return false;
    request = elrsPendingRequest_;
    elrsRequestPending_ = false;
    return true;
}

void TransmitterUi::saveCalibration()
{
    const ProportionalCalibration &calibration = calibratingSteering_
        ? steeringInput_.calibration() : triggerInput_.calibration();
    Preferences preferences;
    preferences.begin(calibratingSteering_
        ? "jr-r1-steering" : "jr-r1-trigger", false);
    preferences.putUShort("adc_min", calibration.minimum);
    preferences.putUShort("adc_ctr", calibration.centre);
    preferences.putUShort("adc_max", calibration.maximum);
    preferences.end();
}

void TransmitterUi::loadSystemSettings()
{
    Preferences preferences;
    preferences.begin("jr-r1-system", true);
    systemSettings_.buzzerEnabled = preferences.getBool("buzz_en", true);
    systemSettings_.keyClicksEnabled = preferences.getBool("key_beep", true);
    systemSettings_.buzzerPitchHz = preferences.getUShort("pitch", 3200);
    systemSettings_.keyClickDurationMs = preferences.getUShort("click_ms", 10);
    systemSettings_.screenTimeoutSeconds = preferences.getUShort("scr_timeout", 60);
    systemSettings_.logoScreensaverEnabled = preferences.getBool("idle_logo", true);
    systemSettings_.displayBrightnessPercent = preferences.getUChar("brightness", 100);
    systemSettings_.nightMode = preferences.getBool("night", false);
    systemSettings_.darkColourScheme = preferences.getUChar("dark_scheme", 0);
    if (systemSettings_.darkColourScheme >= darkColourSchemeCount) systemSettings_.darkColourScheme = 0;
    uiNightMode = systemSettings_.nightMode;
    uiDarkColourScheme = systemSettings_.darkColourScheme;
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

void TransmitterUi::saveSystemSettings()
{
    Preferences preferences;
    preferences.begin("jr-r1-system", false);
    preferences.putBool("buzz_en", systemSettings_.buzzerEnabled);
    preferences.putBool("key_beep", systemSettings_.keyClicksEnabled);
    preferences.putUShort("pitch", systemSettings_.buzzerPitchHz);
    preferences.putUShort("click_ms", systemSettings_.keyClickDurationMs);
    preferences.putUShort("scr_timeout", systemSettings_.screenTimeoutSeconds);
    preferences.putBool("idle_logo", systemSettings_.logoScreensaverEnabled);
    preferences.putUChar("brightness", systemSettings_.displayBrightnessPercent);
    preferences.putBool("night", systemSettings_.nightMode);
    preferences.putUChar("dark_scheme", systemSettings_.darkColourScheme);
    preferences.end();
}

void TransmitterUi::loadTxBatterySettings()
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

void TransmitterUi::saveTxBatterySettings()
{
    Preferences preferences;
    preferences.begin("jr-r1-txbat", false);
    preferences.putUChar("chem", static_cast<uint8_t>(txBatterySettings_.chemistry));
    preferences.putUChar("cells", txBatterySettings_.cellCount);
    preferences.putUShort("low_dv", txBatterySettings_.lowAlarmDecivolts);
    preferences.putBool("sense_en", txBatterySettings_.voltageSenseEnabled);
    preferences.end();
}

void TransmitterUi::loadRxBatterySettings()
{
    Preferences preferences;
    char name[16];
    preferences.begin(modelNamespace(activeModelSlot_, "jr-r1-rxbat",
                                     "rxbat", name, sizeof(name)), true);
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

void TransmitterUi::saveRxBatterySettings()
{
    Preferences preferences;
    char name[16];
    preferences.begin(modelNamespace(activeModelSlot_, "jr-r1-rxbat",
                                     "rxbat", name, sizeof(name)), false);
    preferences.putUChar("chem", static_cast<uint8_t>(rxBatterySettings_.chemistry));
    preferences.putUChar("cells", rxBatterySettings_.cellCount);
    preferences.putUShort("low_dv", rxBatterySettings_.lowAlarmDecivolts);
    preferences.putBool("alarm_en", rxBatterySettings_.alarmEnabled);
    preferences.end();
}

void TransmitterUi::loadDriveModeSettings()
{
    Preferences preferences;
    char name[16];
    preferences.begin(modelNamespace(activeModelSlot_, "jr-r1-drive",
                                     "drive", name, sizeof(name)), true);
    driveModeSettings_.walkLimitPercent = preferences.getUChar("walk_limit", 35);
    preferences.end();
    driveModeSettings_.walkLimitPercent = constrain(
        driveModeSettings_.walkLimitPercent, 10, 80);
    // The profile selection is intentionally not retained yet. A power cycle
    // still starts safely because armRequested_ is always false.
    driveModeSettings_.activeMode = DriveMode::Normal;
}

void TransmitterUi::saveDriveModeSettings()
{
    Preferences preferences;
    char name[16];
    preferences.begin(modelNamespace(activeModelSlot_, "jr-r1-drive",
                                     "drive", name, sizeof(name)), false);
    preferences.putUChar("walk_limit", driveModeSettings_.walkLimitPercent);
    preferences.end();
}

void TransmitterUi::loadElrsSettings()
{
    Preferences preferences;
    preferences.begin("jr-r1-elrs", true);
    String phrase = preferences.getString("phrase", "");
    preferences.end();
    phrase.toCharArray(elrsBindPhrase_, sizeof(elrsBindPhrase_));
}

void TransmitterUi::saveElrsSettings()
{
    Preferences preferences;
    preferences.begin("jr-r1-elrs", false);
    preferences.putString("phrase", elrsBindPhrase_);
    preferences.end();
}

void TransmitterUi::restoreDefaultBindings()
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

void TransmitterUi::loadBindings()
{
    Preferences preferences;
    char name[16];
    preferences.begin(modelNamespace(activeModelSlot_, "jr-r1-bind",
                                     "bind", name, sizeof(name)), true);
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

void TransmitterUi::saveBindings()
{
    Preferences preferences;
    char name[16];
    preferences.begin(modelNamespace(activeModelSlot_, "jr-r1-bind",
                                     "bind", name, sizeof(name)), false);
    preferences.putUShort("version", kBindingProfileVersion);
    preferences.putUChar("count", static_cast<uint8_t>(bindingCount_));
    preferences.putBytes("items", bindings_, bindingCount_ * sizeof(ControlBinding));
    preferences.end();
}

void TransmitterUi::loadChannelParameters()
{
    Preferences preferences;
    char name[16];
    preferences.begin(modelNamespace(activeModelSlot_, "jr-r1-chparam",
                                     "param", name, sizeof(name)), true);
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

void TransmitterUi::saveChannelParameters()
{
    Preferences preferences;
    char name[16];
    preferences.begin(modelNamespace(activeModelSlot_, "jr-r1-chparam",
                                     "param", name, sizeof(name)), false);
    preferences.putUShort("version", kChannelParameterVersion);
    preferences.putBytes("values", channelParameters_, sizeof(channelParameters_));
    preferences.end();
    channelParameterSavePending_ = false;
}

void TransmitterUi::scheduleChannelParameterSave()
{
    channelParameterSavePending_ = true;
    channelParameterSaveDueMs_ = millis() + kSettingsSaveDelayMs;
}

int TransmitterUi::bindingIndexForInput(PhysicalControlId input) const
{
    for (size_t index = 0; index < bindingCount_; ++index)
        if (bindings_[index].input == input) return static_cast<int>(index);
    return -1;
}

int TransmitterUi::bindingIndexForChannelParameter(
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

PhysicalControlId TransmitterUi::adjacentChannelParameterControl(
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

bool TransmitterUi::assignChannelParameterControl(
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

ControlBinding TransmitterUi::defaultBindingForInput(PhysicalControlId input) const
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

uint8_t TransmitterUi::adjacentBindingChannel(uint8_t current,
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

ControlBehaviour TransmitterUi::adjacentBindingBehaviour(
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

AssignmentTarget TransmitterUi::adjacentBindingTarget(
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

bool TransmitterUi::bindingUsesChannel(uint8_t channel,
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

bool TransmitterUi::saveBindingDraft()
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

bool TransmitterUi::removeSelectedBinding()
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

void TransmitterUi::playBuzzer(uint16_t durationMs)
{
    if (!systemSettings_.buzzerEnabled) return;
#ifdef JR_R1_SIMULATOR
    simulatorBeep(systemSettings_.buzzerPitchHz, durationMs);
#else
    tone(BUZZER_PIN, systemSettings_.buzzerPitchHz, durationMs);
#endif
}

void TransmitterUi::playKeyClick()
{
    if (systemSettings_.keyClicksEnabled) playBuzzer(systemSettings_.keyClickDurationMs);
}

void TransmitterUi::stopBuzzer()
{
#ifdef JR_R1_SIMULATOR
    simulatorStopBeep();
#else
    noTone(BUZZER_PIN);
#endif
}

void TransmitterUi::sleepDisplay()
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

void TransmitterUi::wakeDisplay()
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

void TransmitterUi::redrawActivePage()
{
    if (designPage()) { drawDesignPage(activePage_); return; }
    switch (activePage_) {
        case UiPage::Tune: case UiPage::ValueEditor: case UiPage::Gps:
        case UiPage::ChannelMonitor: case UiPage::SensorDetail: case UiPage::WidgetSelect: case UiPage::WidgetEditor: case UiPage::BatteryMenu:
            drawDesignPage(activePage_); break;
        case UiPage::RadioDiagnostics: drawRadioDiagnostics(); break;
        case UiPage::BootSplash: drawBootSplash(); break;
        case UiPage::LogoSaver: drawLogoSaver(); break;
        case UiPage::Dashboard: drawDashboard(); break;
        case UiPage::StartupWarning: drawStartupWarningPage(); break;
        case UiPage::Telemetry: drawTelemetryPage(); break;
        case UiPage::MainMenu: drawMainMenu(); break;
        case UiPage::ModelSelect: drawModelSelectPage(); break;
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
        case UiPage::ColourScheme: drawColourSchemePage(); break;
        case UiPage::FirmwareInfo: drawFirmwareInfoPage(); break;
        case UiPage::ElrsBinding: drawElrsBindingPage(); break;
        case UiPage::ElrsTxConfig: drawElrsTxConfigPage(); break;
        case UiPage::EscProgramming: drawEscProgrammingPage(); break;
        case UiPage::Calibration: drawCalibrationPage(); break;
    }
}

void TransmitterUi::drawBootSplash()
{
    if (!displayReady_) return;
    activePage_ = UiPage::BootSplash;
    bootSplashStartedMs_ = millis();
    startScreen();
    resetLvglObjects();
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);

    lv_obj_t *logo = lv_img_create(lv_scr_act());
    lv_img_set_src(logo, &jr_propo_boot_logo);
#ifdef JR_R1_UI_PORTRAIT
    lv_img_set_zoom(logo, 176);
    lv_obj_align(logo, LV_ALIGN_TOP_MID, 0, 150);
#else
    lv_obj_align(logo, LV_ALIGN_TOP_MID, 0, 92);
#endif

    lv_obj_t *designation = makeLabel(
        lv_scr_act(), "R1 EVO", 0, 0, 18, 0xbec3c9);
    lv_obj_set_style_text_letter_space(designation, 5, 0);
#ifdef JR_R1_UI_PORTRAIT
    lv_obj_align(designation, LV_ALIGN_TOP_MID, 0, 252);
#else
    lv_obj_align(designation, LV_ALIGN_TOP_MID, 0, 204);
#endif
}

void TransmitterUi::positionLogoSaver()
{
    if (!logoSaverLogo_ || !logoSaverDesignation_) return;
#ifdef JR_R1_UI_PORTRAIT
    static constexpr int16_t xOffsets[] = {0, -4, 4, 0};
    static constexpr int16_t yPositions[] = {54, 150, 276, 192};
    const uint8_t position = logoSaverPosition_ % 4;
    lv_obj_align(logoSaverLogo_, LV_ALIGN_TOP_MID,
                 xOffsets[position], yPositions[position]);
    lv_obj_align(logoSaverDesignation_, LV_ALIGN_TOP_MID,
                 xOffsets[position], yPositions[position] + 100);
#else
    static constexpr int16_t xOffsets[] = {0, -10, 10, 0};
    static constexpr int16_t yPositions[] = {32, 92, 158, 110};
    const uint8_t position = logoSaverPosition_ % 4;
    lv_obj_align(logoSaverLogo_, LV_ALIGN_TOP_MID,
                 xOffsets[position], yPositions[position]);
    lv_obj_align(logoSaverDesignation_, LV_ALIGN_TOP_MID,
                 xOffsets[position], yPositions[position] + 112);
#endif
}

void TransmitterUi::drawLogoSaver()
{
    if (!displayReady_) return;
    activePage_ = UiPage::LogoSaver;
    startScreen();
    resetLvglObjects();
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);

    logoSaverLogo_ = lv_img_create(lv_scr_act());
    lv_img_set_src(logoSaverLogo_, &jr_propo_boot_logo);
#ifdef JR_R1_UI_PORTRAIT
    lv_img_set_zoom(logoSaverLogo_, 176);
#endif
    logoSaverDesignation_ = makeLabel(
        lv_scr_act(), "R1 EVO", 0, 0, 18, 0xbec3c9);
    lv_obj_set_style_text_letter_space(logoSaverDesignation_, 5, 0);
    logoSaverPosition_ = 0;
    positionLogoSaver();
    for (size_t index = 0; index < kPhysicalControlCount; ++index) {
        logoSaverInputValues_[index] = physicalInputStates_[index].value;
        logoSaverInputPrimary_[index] = physicalInputStates_[index].stablePrimary;
        logoSaverInputSecondary_[index] = physicalInputStates_[index].stableSecondary;
        logoSaverInputFaulted_[index] = physicalInputStates_[index].faulted;
    }
    nextLogoSaverMoveMs_ = millis() + kLogoSaverMoveMs;
    applyBacklight(min(systemSettings_.displayBrightnessPercent,
                       kLogoSaverBrightnessPercent));
    Serial.println("DISPLAY: logo saver active (25% maximum brightness)");
}

void TransmitterUi::leaveLogoSaver()
{
    if (activePage_ != UiPage::LogoSaver) return;
    applyBacklight(systemSettings_.displayBrightnessPercent);
    activePage_ = (logoSaverReturnPage_ == UiPage::BootSplash ||
                   logoSaverReturnPage_ == UiPage::LogoSaver)
        ? UiPage::Dashboard : logoSaverReturnPage_;
    lastInteractionMs_ = millis();
    redrawActivePage();
    Serial.println("DISPLAY: logo saver dismissed");
}

int16_t TransmitterUi::mapAdcToPermille(uint16_t adc) const
{
    return mapProportionalInputToPermille(throttleInputProfile(), adc);
}

ProportionalInputProfile TransmitterUi::throttleInputProfile() const
{
    ProportionalInputProfile profile;
    profile.calibration = triggerInput_.calibration();
    profile.deadbandAdc = ARM_NEUTRAL_ADC_WINDOW;
    profile.validMinimumAdc = THROTTLE_ADC_VALID_MIN;
    profile.validMaximumAdc = THROTTLE_ADC_VALID_MAX;
    // A 1/4 EMA at the 4 ms control period rejects ADC chatter without
    // noticeably softening deliberate fast steering/throttle movement.
    profile.filterShift = 2;
    return profile;
}

ProportionalInputProfile TransmitterUi::steeringInputProfile() const
{
    ProportionalInputProfile profile;
    profile.calibration = steeringInput_.calibration();
    profile.deadbandAdc = 8;
    profile.validMinimumAdc = 0;
    profile.validMaximumAdc = 4095;
    profile.filterShift = 2;
    return profile;
}

ControlConfigurationSnapshot TransmitterUi::controlConfigurationSnapshot() const
{
    ControlConfigurationSnapshot snapshot;
    snapshot.throttleInput = throttleInputProfile();
    snapshot.steeringInput = steeringInputProfile();
    for (size_t index = 0; index < CRSF_CHANNEL_COUNT; ++index) {
        snapshot.channelProfiles[index] = channelProfiles_[index];
        snapshot.channelParameters[index] = channelParameters_[index];
    }
    for (size_t index = 0; index < bindingCount_; ++index) {
        const ControlBinding &binding = bindings_[index];
        if (!binding.enabled || binding.target != AssignmentTarget::CrsfChannel ||
            binding.crsfChannel < 1 || binding.crsfChannel > CRSF_CHANNEL_COUNT ||
            physicalControlDescriptor(static_cast<size_t>(binding.input)).kind !=
                PhysicalControlKind::Proportional)
            continue;
        snapshot.proportionalOutputMask |= static_cast<uint16_t>(
            1U << (binding.crsfChannel - 1));
    }
    snapshot.steeringChannel = channelRoleSettings_.steeringChannel;
    snapshot.throttleChannel = channelRoleSettings_.throttleChannel;
    snapshot.steeringEnabled = channelRoleSettings_.steeringEnabled;
    snapshot.throttleEnabled = channelRoleSettings_.throttleEnabled;
    snapshot.armRequested = armRequested_;
    snapshot.calibrationActive = calibrationActive() ||
        activePage_ == UiPage::ElrsTxConfig;
    snapshot.driveMode = driveModeSettings_.activeMode;
    snapshot.walkLimitPercent = driveModeSettings_.walkLimitPercent;
    return snapshot;
}

int16_t TransmitterUi::channelParameterValue(uint8_t channel,
                                          ChannelParameterKind kind) const
{
    if (channel < 1 || channel > 16) return 0;
    const ChannelParameterValues &values = channelParameters_[channel - 1];
    return kind == ChannelParameterKind::Subtrim
        ? values.subtrimPermille : values.trimPermille;
}

bool TransmitterUi::applyChannelParameterAction(uint16_t targetId, int16_t value)
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

uint8_t TransmitterUi::activeControlContext() const
{
    return (activePage_ == UiPage::Dashboard || activePage_ == UiPage::Telemetry || activePage_ == UiPage::Gps || activePage_ == UiPage::ChannelMonitor || activePage_ == UiPage::SensorDetail || activePage_ == UiPage::RadioDiagnostics)
        ? ContextDashboard : ContextMenu;
}

bool TransmitterUi::navigateDashboardPage(int8_t direction)
{
    // Keep the ordered list in one place as driving pages are added. Direction
    // is meaningful once GPS/map and power pages join this initial pair.
    constexpr UiPage pages[] = {UiPage::Dashboard, UiPage::Telemetry};
    constexpr size_t count = sizeof(pages) / sizeof(pages[0]);
    size_t selected = count;
    for (size_t index = 0; index < count; ++index) {
        const UiPage current = activePage_ == UiPage::Gps || activePage_ == UiPage::ChannelMonitor || activePage_ == UiPage::SensorDetail || activePage_ == UiPage::RadioDiagnostics ? UiPage::Telemetry : activePage_;
        if (pages[index] == current) {
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

bool TransmitterUi::applyTransmitterFunctionAction(uint16_t targetId, int16_t value)
{
    if (targetId >= static_cast<uint16_t>(TransmitterFunction::Count)) return false;
    if (activePage_ == UiPage::LogoSaver && value > 0) {
        leaveLogoSaver();
        return true;
    }
    const TransmitterFunction function = static_cast<TransmitterFunction>(targetId);
    if (activePage_ == UiPage::BootSplash) return true;
    if (!startupPreflightComplete_) {
        // Keep processing raw input state, but do not let a bound key or
        // switch dismiss or bypass the non-dismissible startup check.
        if (function == TransmitterFunction::DisplayWake && value > 0)
            wakeDisplay();
        return true;
    }
    switch (function) {
        case TransmitterFunction::TimerStartStop:
            if (value > 0) toggleRunTimer();
            return true;
        case TransmitterFunction::ResetSpeedStatistics:
            if (value > 0) resetSpeedStatistics();
            return true;
        case TransmitterFunction::TimerReset:
            if (value > 0) { runTimerElapsedMs_ = 0; runTimerStartedMs_ = millis(); }
            return true;
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
                if (activePage_ == UiPage::MainMenu && !designPage()) moveMainMenuFocus(-1);
                else movePageFocus(-1);
            }
            return true;
        case TransmitterFunction::MenuNext:
            if (value > 0) {
                if (activePage_ == UiPage::MainMenu && !designPage()) moveMainMenuFocus(1);
                else movePageFocus(1);
            }
            return true;
        case TransmitterFunction::MenuSelect:
            if (value > 0) {
                if (activePage_ == UiPage::MainMenu && !designPage()) activateMainMenuFocus();
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

bool TransmitterUi::applyDriveModeAction(uint16_t targetId, int16_t value)
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

void TransmitterUi::drawDashboard()
{
    drawDesignPage(UiPage::Dashboard);
}

void TransmitterUi::drawStartupWarningPage()
{
    activePage_ = UiPage::StartupWarning;
    startScreen();
    resetLvglObjects();
    lv_obj_t *header = makePanel(0, 0, kScreenWidth, 44, cHeader, cHeader, 0);
    makeLabel(header, "STARTUP SAFETY CHECK", 14, 12, portraitUi ? 18 : 24);
    beginPageFocus(false);

    lv_obj_t *banner = makePanel(12, 54, kScreenWidth-24, portraitUi ? 90 : 58, cSafetyFault, cRed);
    makeLabel(banner, startupWarningMask_ == 0
                          ? "CHECKING CONTROLS..."
                          : (portraitUi ? "RETURN CONTROLS\nTO SAFE POSITION" : "RETURN CONTROLS TO SAFE POSITION"),
              18, 8, 16, startupWarningMask_ == 0 ? cAmber : cRed);
    makeLabel(banner, "Output remains CUT", 18, portraitUi ? 66 : 33, 12, cWhite);

    if (startupWarningMask_ == 0) {
        makeLabel(lv_scr_act(), "Waiting for stable input readings", portraitUi ? 14 : 72, portraitUi ? 176 : 154,
                  16, cDim);
    } else {
        uint8_t shown = 0;
        for (size_t index = 0; index < kPhysicalControlCount; ++index) {
            if ((startupWarningMask_ & (uint32_t{1} << index)) == 0) continue;
            const int16_t column = portraitUi ? 0 : shown >= 4 ? 1 : 0;
            const int16_t row = portraitUi ? shown : shown >= 4 ? shown - 4 : shown;
            const int16_t x = 18 + column * 230;
            const int16_t y = (portraitUi ? 155 : 126) + row * (portraitUi ? 34 : 38);
            lv_obj_t *panel = makePanel(x, y, portraitUi ? 284 : 214, 31, cPanel2, cAmber);
            makeLabel(panel, physicalControlDescriptor(index).name,
                      8, 7, 11, cWhite);
            ++shown;
            if (shown >= 8) break;
        }
    }
    makeLabel(lv_scr_act(), portraitUi ? "Release switches / centre trims\nto continue" : "Release switches / centre trims to continue",
              portraitUi ? 14 : 65, portraitUi ? 436 : 286, 12, cAmber);
    finishPageFocus();
}

void TransmitterUi::drawRadioDiagnostics()
{
    if (!displayReady_) return;
    activePage_ = UiPage::RadioDiagnostics;
    startScreen();
    resetLvglObjects();
    makeHeader("TELEMETRY", "BACK");
    beginPageFocus();

    lv_obj_t *link = makePanel(8, 52, portraitUi ? 304 : 226, 126);
    makeLabel(link, "RADIO LINK", 10, 7, 12, cCyan);
    telemetryLinkStateLabel_ = makeLabel(link, "NO DATA", 10, 27, 20, cDim);
    telemetryLinkQualityLabel_ = makeLabel(link, "LQ   ---%", 10, 56, 17, cDim);
    telemetryRssiLabel_ = makeLabel(link, "RSSI --- dBm", 10, 80, 15, cDim);
    telemetrySnrLabel_ = makeLabel(link, "SNR  -- dB", 10, 101, 13, cDim);

    lv_obj_t *power = makePanel(portraitUi ? 8 : 246, portraitUi ? 188 : 47, portraitUi ? 304 : 226, 126);
    makeLabel(power, "POWER / RF", 10, 7, 12, cCyan);
    telemetryTxVoltageLabel_ = makeLabel(power, "TX  --.- V", 10, 31, 18, cDim);
    telemetryRxVoltageLabel_ = makeLabel(power, "RX  --.- V", 10, 59, 18, cDim);
    telemetryRadioLabel_ = makeLabel(power, "ANT -   MODE -   PWR -", 10, 94, 12, cDim);

    lv_obj_t *stream = makePanel(8, portraitUi ? 324 : 181, kScreenWidth-16, portraitUi ? 120 : 98);
    makeLabel(stream, "CRSF STREAM", 10, 7, 12, cCyan);
    telemetryFramesLabel_ = makeLabel(stream, "FRAMES          0", 10, 31, 15, cWhite);
    telemetryCrcLabel_ = makeLabel(stream, "CRC ERR         0", portraitUi ? 10 : 245, portraitUi ? 78 : 31, portraitUi ? 12 : 15, cWhite);
    telemetryBytesLabel_ = makeLabel(stream, "BYTES           0", 10, 58, 15, cWhite);
    telemetryAgeLabel_ = makeLabel(stream, "AGE          ----", portraitUi ? 150 : 245, portraitUi ? 78 : 58, portraitUi ? 12 : 15, cDim);

    makeLabel(lv_scr_act(), "Live receiver telemetry via Ranger Nano", 12, portraitUi ? 446 : 293, 12, cDim);
    finishPageFocus();
}

void TransmitterUi::drawTelemetryPage()
{
    drawDesignPage(UiPage::Telemetry);
}

void TransmitterUi::drawTelemetryLive(const CrsfTelemetryState &telemetry,
                                   uint16_t txVoltageDecivolts,
                                   bool txVoltageValid)
{
    if (!displayReady_ || activePage_ != UiPage::RadioDiagnostics) return;

    if (telemetryLinkStateLabel_) {
        const bool everReceived = telemetry.linkUpdatedMs != 0;
        const bool explicitLoss = telemetry.linkValid &&
                                  telemetry.uplinkLinkQuality == 0;
        const char *stateText = !everReceived ? "NO DATA" :
            (!telemetry.linkValid ? "TELEM STALE" :
             (explicitLoss ? "NO LINK" : "LINK OK"));
        const uint32_t stateColour = !everReceived || !telemetry.linkValid
            ? cDim : (explicitLoss ? cRed : cGreen);
        lv_label_set_text(telemetryLinkStateLabel_, stateText);
        lv_obj_set_style_text_color(telemetryLinkStateLabel_,
                                    colour(stateColour), 0);
    }
    if (telemetryLinkQualityLabel_) {
        if (telemetry.linkValid) {
            lv_label_set_text_fmt(telemetryLinkQualityLabel_, "LQ   %3u%%",
                                  telemetry.uplinkLinkQuality);
            lv_obj_set_style_text_color(telemetryLinkQualityLabel_,
                colour(telemetry.uplinkLinkQuality >= 80 ? cGreen : cAmber), 0);
        } else if (telemetry.linkUpdatedMs == 0) {
            lv_label_set_text(telemetryLinkQualityLabel_, "LQ   ---%");
            lv_obj_set_style_text_color(telemetryLinkQualityLabel_, colour(cDim), 0);
        } else {
            lv_label_set_text_fmt(telemetryLinkQualityLabel_, "LQ*  %3u%%",
                                  telemetry.uplinkLinkQuality);
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

void TransmitterUi::drawDashboardLive(uint16_t crsfValue,
                                   OutputSafetyStatus safetyStatus,
                                   const CrsfTelemetryState &telemetry,
                                   uint16_t txVoltageDecivolts,
                                   bool txVoltageValid)
{
    updateDesign(telemetry, safetyStatus, txVoltageDecivolts, txVoltageValid);
    (void)crsfValue;
}

void TransmitterUi::drawMainMenu()
{
    drawDesignPage(UiPage::MainMenu);
}

void TransmitterUi::refreshMainMenuFocus()
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

void TransmitterUi::moveMainMenuFocus(int8_t direction)
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

void TransmitterUi::activateMainMenuFocus()
{
    if (activePage_ != UiPage::MainMenu ||
        !mainMenuItemEnabled(mainMenuFocusIndex_)) return;
    playKeyClick();
    switch (mainMenuFocusIndex_) {
        case 0: drawModelSelectPage(); break;
        case 1: drawChannelsPage(); break;
        case 2:
            selectedChannel_ = channelRoleSettings_.throttleChannel;
            throttleResponseReturnPage_ = UiPage::MainMenu;
            drawThrottleResponsePage();
            break;
        case 3: drawControlAssignmentsPage(); break;
        case 4:
            driveModesReturnPage_ = UiPage::MainMenu;
            drawDriveModesPage();
            break;
        case 5: drawEscProgrammingPage(); break;
        case 6:
            txConfig_.start(millis());
            txConfigFolder_ = txConfigOffset_ = txConfigSelected_ = 0;
            drawElrsTxConfigPage(); break;
        case 7: drawSetupPage(); break;
        default: break;
    }
}

void TransmitterUi::drawModelSelectPage()
{
    drawDesignPage(UiPage::ModelSelect);
}

void TransmitterUi::beginPageFocus(bool registerStandardHeader)
{
    pageFocusCount_ = 0;
    pageFocusIndex_ = 0;
    pageFocusIndicator_ = nullptr;
    pageStandardHeaderPending_ = registerStandardHeader;
}

void TransmitterUi::addPageActionTarget(int16_t x, int16_t y, int16_t width,
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

lv_obj_t *TransmitterUi::makePageActionButton(
    int16_t x, int16_t y, int16_t width, int16_t height,
    const char *text, uint8_t fontSize, uint32_t fill,
    uint32_t textColour, uint32_t border,
    PageControlHandler handler, uint8_t argument)
{
    lv_obj_t *button = makeButton(x, y, width, height, text, fontSize,
                                  fill, textColour, border);
    addPageActionTarget(x, y, width, height, handler, argument);
    return button;
}

void TransmitterUi::addPageEditorTarget(
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

void TransmitterUi::finishPageFocus()
{
    // Append standard Back/Home navigation after page content so the first
    // keypad focus remains the page's primary control, while every new
    // standard-header page gets an escape route automatically.
    if (pageStandardHeaderPending_) {
        addPageActionTarget(portraitUi ? 206 : 344, 5, portraitUi ? 66 : 78, 34,
                            &TransmitterUi::controlNavigation, 0);
        addPageActionTarget(portraitUi ? 278 : 430, 5, portraitUi ? 36 : 42, 34,
                            &TransmitterUi::controlNavigation, 1);
        pageStandardHeaderPending_ = false;
    }
    if (portraitUi) {
        auto strip=makePanel(0,460,320,20,cHeader,cHeader,0);
        makeLabel(strip,"JR",8,3,12,cRed);
        makeLabel(strip,"R1",29,3,12,0xe7edef);
        makeLabel(strip,"TOUCH CONTROL",111,4,10,0xa9bbc5);
        auto indicator=lv_obj_create(strip);position(indicator,300,8,12,4);
        lv_obj_set_style_bg_color(indicator,colour(cCyan),0);
        lv_obj_set_style_border_width(indicator,0,0);
    }
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

void TransmitterUi::refreshPageFocus()
{
    if (pageFocusIndicator_ == nullptr || pageFocusCount_ == 0) return;
    if (pageFocusIndex_ >= pageFocusCount_) pageFocusIndex_ = 0;
    const PageFocusTarget &target = pageFocusTargets_[pageFocusIndex_];
    position(pageFocusIndicator_, target.focus.x, target.focus.y,
             target.focus.width, target.focus.height);
    lv_obj_move_foreground(pageFocusIndicator_);
    if(pageFocusVisible_) lv_obj_clear_flag(pageFocusIndicator_,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(pageFocusIndicator_,LV_OBJ_FLAG_HIDDEN);
}

void TransmitterUi::movePageFocus(int8_t direction)
{
    pageFocusVisible_ = true;
    if (pageFocusCount_ == 0) {
        Serial.printf("UI: page %u has no registered keypad controls\n",
                      static_cast<unsigned>(activePage_));
        playBuzzer(35);
        return;
    }
    pageFocusIndex_ = static_cast<uint8_t>(
        (static_cast<int>(pageFocusIndex_) + (direction < 0 ? -1 : 1) +
         pageFocusCount_) % pageFocusCount_);
    refreshPageFocus();
    playKeyClick();
}

void TransmitterUi::activatePageFocus()
{
    pageFocusVisible_ = true;
    if (pageFocusCount_ == 0 || pageFocusIndex_ >= pageFocusCount_) return;
    if (dispatchPageControl(pageFocusIndex_, PageControlEvent::Activate)) return;
    playBuzzer(35);
}

bool TransmitterUi::adjustPageFocus(int8_t direction)
{
    // In the endpoint editor the handset +/- keys always edit the selected
    // side, even when focus is on a side selector, Apply or Cancel.
    if (steeringEndpointEditor()) {
        controlDesign(direction < 0 ? 30 : 31, PageControlEvent::Activate);
        return true;
    }
    pageFocusVisible_ = true;
    if (pageFocusCount_ == 0 || pageFocusIndex_ >= pageFocusCount_) return false;
    if (dispatchPageControl(pageFocusIndex_, direction < 0
            ? PageControlEvent::Decrease : PageControlEvent::Increase))
        return true;
    playBuzzer(35);
    return false;
}

bool TransmitterUi::dispatchPageControl(uint8_t index, PageControlEvent event)
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

bool TransmitterUi::dispatchPageControlTouch(int16_t x, int16_t y)
{
    pageFocusVisible_ = false;
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

void TransmitterUi::controlChannelRoles(uint8_t row, PageControlEvent event)
{
    const int16_t x = event == PageControlEvent::Decrease ? 375 : 430;
    handleChannelRolesTouch(x, 100 + row * 68);
}

void TransmitterUi::controlTxBattery(uint8_t row, PageControlEvent event)
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

void TransmitterUi::controlRxBattery(uint8_t row, PageControlEvent event)
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

void TransmitterUi::controlDisplay(uint8_t row, PageControlEvent event)
{
    if (row == 10 && displaySetupBank_ > 0) {
        --displaySetupBank_;
        playKeyClick();
        drawDisplayPage();
        return;
    }
    if (row == 11 && displaySetupBank_ < 1) {
        ++displaySetupBank_;
        playKeyClick();
        drawDisplayPage();
        return;
    }
    const int16_t x = event == PageControlEvent::Decrease ? 375 : 430;
    handleDisplayTouch(x, 78 + (row % 4) * 50);
}

void TransmitterUi::controlModels(uint8_t item, PageControlEvent)
{
    if (item < 5) {
        const uint8_t slot = modelSelectBank_ * 5 + item;
        if (slot == activeModelSlot_) return;
        playKeyClick();
        selectModelMemory(slot);
        drawModelSelectPage();
    } else if (item == 10 && modelSelectBank_ > 0) {
        --modelSelectBank_;
        modelSelectStatus_[0] = '\0';
        playKeyClick();
        drawModelSelectPage();
    } else if (item == 11 && modelSelectBank_ < 1) {
        ++modelSelectBank_;
        modelSelectStatus_[0] = '\0';
        playKeyClick();
        drawModelSelectPage();
    }
}

void TransmitterUi::controlBindingEditor(uint8_t field, PageControlEvent event)
{
    if (field == 0) {
        handleBindingEditorTouch(400, 101);
    } else if (field >= 1 && field <= 4) {
        const int16_t x = event == PageControlEvent::Decrease ? 375 : 430;
        handleBindingEditorTouch(x, 136 + (field - 1) * 34);
    } else if (field == 5) {
        handleBindingEditorTouch(80, 290);
    } else if (field == 6) {
        handleBindingEditorTouch(220, 290);
    } else if (field == 7) {
        handleBindingEditorTouch(390, 290);
    }
}

void TransmitterUi::controlChannels(uint8_t item, PageControlEvent)
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

void TransmitterUi::controlAssignments(uint8_t item, PageControlEvent)
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

void TransmitterUi::controlDriveMode(uint8_t item, PageControlEvent event)
{
    if (item < 2) {
        handleDriveModesTouch(item == 0 ? 150 : 330, 95);
        return;
    }
    const int16_t x = event == PageControlEvent::Decrease ? 375 : 430;
    handleDriveModesTouch(x, 185);
}

void TransmitterUi::controlSetup(uint8_t item, PageControlEvent event)
{
    if (item < 4) {
        const int16_t x = event == PageControlEvent::Decrease ? 375 : 430;
        handleSetupTouch(x, 78 + item * 50);
    } else if (item < 8) {
        static const int16_t x[] = {54, 160, 282, 408};
        handleSetupTouch(x[item - 4], 287);
    } else if (item == 8) {
        playKeyClick(); drawFirmwareInfoPage();
    } else playBuzzer(35);
}

void TransmitterUi::controlEscProgramming(uint8_t item, PageControlEvent)
{
    if (item < 3) {
        escProgrammingTab_ = item;
        playKeyClick();
        drawEscProgrammingPage();
        return;
    }

    // READ and WRITE are deliberately inert until the isolated electrical
    // interface and the selected ESC protocol have been validated.
    playBuzzer(35);
}

void TransmitterUi::controlThrottleSetting(uint8_t row, PageControlEvent event)
{
    const int8_t direction = event == PageControlEvent::Decrease ? -1 : 1;
    bool changed = false;
    if (row == 0) {
        throttleProfile().negativeEndpointPercent = constrain(
            static_cast<int>(throttleProfile().negativeEndpointPercent) + direction, 50, 100);
        changed = true;
    } else if (row == 1) {
        throttleProfile().subtrimPercent = constrain(
            static_cast<int>(throttleProfile().subtrimPercent) + direction, -20, 20);
        changed = true;
    } else if (row == 2) {
        throttleProfile().positiveEndpointPercent = constrain(
            static_cast<int>(throttleProfile().positiveEndpointPercent) + direction, 50, 100);
        changed = true;
    } else if (row == 3) {
        throttleProfile().reversed = !throttleProfile().reversed;
        changed = true;
    }
    if (!changed) return;
    playKeyClick();
    scheduleSettingsSave();
    refreshSettingRowValue(row);
}

void TransmitterUi::controlThrottleResponse(uint8_t row, PageControlEvent event)
{
    const int16_t x = event == PageControlEvent::Decrease ? 405 : 447;
    handleThrottleResponseTouch(x, 66 + row * 35);
}

void TransmitterUi::controlChannelSpeed(uint8_t row, PageControlEvent event)
{
    const int16_t x = event == PageControlEvent::Decrease ? 375 : 430;
    handleChannelSpeedTouch(x, 109 + row * 76);
}

void TransmitterUi::controlDashboard(uint8_t item, PageControlEvent)
{
#ifdef JR_R1_UI_PORTRAIT
    static const int16_t x[] = {295, 43, 121, 199, 277};
    static const int16_t y[] = {22, 437, 437, 437, 437};
#else
    static const int16_t x[] = {452, 62, 178, 294, 414};
    static const int16_t y[] = {20, 280, 280, 280, 280};
#endif
    if (item >= 5) return;
    handleDashboardTouch(x[item], y[item]);
}

void TransmitterUi::controlChannelConfig(uint8_t item, PageControlEvent event)
{
    if (item < 2) {
        const int16_t x = event == PageControlEvent::Decrease ? 373 : 427;
        handleChannelConfigTouch(x, 104 + item * 66);
    } else if (item == 2) {
        handleChannelConfigTouch(170, 272);
    } else if (item == 3) {
        handleChannelConfigTouch(340, 272);
    }
}

void TransmitterUi::controlInputMonitor(uint8_t item, PageControlEvent)
{
    handleInputMonitorTouch(item == 0 ? 40 : 440, 291);
}

void TransmitterUi::controlElrsBinding(uint8_t item, PageControlEvent)
{
    if (item < 5) {
        handleElrsBindingTouch(8 + item * 93 + 43, 110);
        return;
    }
    if (item < 43) {
        const uint8_t lengths[] = {10, 10, 9, 9};
        uint8_t key = item - 5;
        for (uint8_t row = 0; row < 4; ++row) {
            if (key < lengths[row]) {
                constexpr int16_t width = 42;
                const int16_t startX =
                    (kScreenWidth - lengths[row] * width) / 2;
                handleElrsBindingTouch(startX + key * width + width / 2,
                                       147 + row * 38);
                return;
            }
            key -= lengths[row];
        }
        return;
    }
    static const int16_t x[] = {89, 240, 391};
    handleElrsBindingTouch(x[item - 43], 298);
}

void TransmitterUi::controlThrottleHeader(uint8_t item, PageControlEvent)
{
    if (activePage_ == UiPage::Channel2) {
        if (item == 0) {
            selectedChannel_ = channelRoleSettings_.throttleChannel;
            throttleResponseReturnPage_ = UiPage::Channel2;
            playKeyClick(); drawThrottleResponsePage();
        } else {
            playKeyClick(); startCalibration(latestRawAdc_);
        }
    } else if (activePage_ == UiPage::ThrottleResponse) {
        playKeyClick(); drawChannelSpeedPage();
    }
}

void TransmitterUi::controlNavigation(uint8_t item, PageControlEvent)
{
    if (item == 0) navigateBack();
    else {
        if (calibrationActive())
            (calibratingSteering_ ? steeringInput_ : triggerInput_)
                .cancelCalibration();
        playKeyClick();
        drawDashboard();
    }
}

void TransmitterUi::navigateBack()
{
    if (designPage()) {
        if (activePage_ == UiPage::ValueEditor) drawDesignPage(editorReturn_);
        else if (activePage_ == UiPage::WidgetEditor) drawDesignPage(UiPage::WidgetSelect);
        else if (activePage_ == UiPage::SensorDetail || activePage_ == UiPage::Gps || activePage_ == UiPage::ChannelMonitor) drawTelemetryPage();
        else if (activePage_ == UiPage::WidgetSelect || activePage_ == UiPage::Channels || activePage_ == UiPage::ModelSelect) drawMainMenu();
        else drawDashboard();
        return;
    }
    playKeyClick();
    switch (activePage_) {
        case UiPage::Tune: case UiPage::ValueEditor: case UiPage::Gps:
        case UiPage::ChannelMonitor: case UiPage::SensorDetail: case UiPage::WidgetSelect: case UiPage::WidgetEditor: case UiPage::BatteryMenu:
        case UiPage::RadioDiagnostics: drawTelemetryPage(); break;
        case UiPage::BootSplash: break;
        case UiPage::LogoSaver: leaveLogoSaver(); break;
        case UiPage::Dashboard: break;
        case UiPage::StartupWarning: break;
        case UiPage::Telemetry: drawDashboard(); break;
        case UiPage::MainMenu: drawDashboard(); break;
        case UiPage::ModelSelect: drawMainMenu(); break;
        case UiPage::Channels: drawMainMenu(); break;
        case UiPage::ChannelRoles: drawChannelsPage(); break;
        case UiPage::Channel2:
        case UiPage::ChannelPlaceholder: drawChannelsPage(); break;
        case UiPage::ThrottleResponse:
            if (throttleResponseReturnPage_ == UiPage::Channel2) drawPage();
            else if (throttleResponseReturnPage_ == UiPage::Dashboard) drawDashboard();
            else if (throttleResponseReturnPage_ == UiPage::ChannelPlaceholder)
                drawChannelPlaceholder();
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
        case UiPage::ColourScheme: drawDisplayPage(); break;
        case UiPage::FirmwareInfo: drawSetupPage(); break;
        case UiPage::ElrsBinding: drawMainMenu(); break;
        case UiPage::ElrsTxConfig: leaveElrsTxConfig(); break;
        case UiPage::EscProgramming: drawMainMenu(); break;
        case UiPage::Calibration: cancelCalibration(); break;
    }
}

void TransmitterUi::drawChannelsPage()
{
    drawDesignPage(UiPage::Channels);
}

void TransmitterUi::drawChannelRoleRow(int row)
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

void TransmitterUi::drawControlAssignmentsPage()
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
        const int16_t y = 54 + row * (portraitUi ? 65 : 40);
        lv_obj_t *panel = makePanel(10, y, kScreenWidth-20, portraitUi ? 58 : 35);
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
        makeLabel(panel, summary, portraitUi ? 8 : 215, portraitUi ? 29 : 5, 11, summaryColour);
        makeLabel(panel, ">", kScreenWidth-42, 4, 16, cDim);
        addPageActionTarget(10, y, kScreenWidth-20, portraitUi ? 58 : 35,
                            &TransmitterUi::controlAssignments, row);
    }

    makeButton(10, portraitUi ? 410 : 263, 50, 42, "<", 22,
               assignmentBank_ == 0 ? cBlack : cPanel,
               assignmentBank_ == 0 ? cDim : cWhite);
    addPageActionTarget(10, portraitUi ? 410 : 263, 50, 42,
                        &TransmitterUi::controlAssignments, 5);
    makeButton(portraitUi ? 66 : 70, portraitUi ? 410 : 263, portraitUi ? 80 : 94, 42, "ROLES", 12, cPanel2, cCyan);
    addPageActionTarget(portraitUi ? 66 : 70, portraitUi ? 410 : 263, portraitUi ? 80 : 94, 42,
                        &TransmitterUi::controlAssignments, 6);
    makeButton(portraitUi ? 152 : 174, portraitUi ? 410 : 263, portraitUi ? 102 : 132, 42, "INPUT MON", 12, cPanel2, cCyan);
    addPageActionTarget(portraitUi ? 152 : 174, portraitUi ? 410 : 263, portraitUi ? 102 : 132, 42,
                        &TransmitterUi::controlAssignments, 7);
    char page[12];
    snprintf(page, sizeof(page), "%u/%u", assignmentBank_ + 1, kBanks);
    makeLabel(lv_scr_act(), page, portraitUi ? 140 : 329, portraitUi ? 396 : 276, 12, cDim);
    makeButton(portraitUi ? 260 : 420, portraitUi ? 410 : 263, 50, 42, ">", 22,
               assignmentBank_ + 1 >= kBanks ? cBlack : cPanel,
               assignmentBank_ + 1 >= kBanks ? cDim : cWhite);
    addPageActionTarget(portraitUi ? 260 : 420, portraitUi ? 410 : 263, 50, 42,
                        &TransmitterUi::controlAssignments, 8);
    finishPageFocus();
}

void TransmitterUi::drawBindingEditorPage()
{
    activePage_ = UiPage::BindingEditor;
    startScreen();
    resetLvglObjects();
    makeHeader("EDIT ASSIGNMENT", "BACK");
    beginPageFocus();

    const PhysicalControlDescriptor &control = physicalControlDescriptor(
        static_cast<size_t>(selectedBindingInput_));
    lv_obj_t *namePanel = makePanel(12, 48, kScreenWidth-24, portraitUi ? 52 : 36);
    makeLabel(namePanel, control.name, 12, 7, 14, cCyan);
    makeLabel(namePanel, r1InputEndpointLabel(selectedBindingInput_), portraitUi ? 12 : 300, portraitUi ? 30 : 9, 10, cDim);

    const bool parameter = bindingDraft_.target == AssignmentTarget::ModelParameter &&
        isChannelParameterTarget(bindingDraft_.targetId);
    const int16_t ys[] = {static_cast<int16_t>(portraitUi?106:86),static_cast<int16_t>(portraitUi?166:120),static_cast<int16_t>(portraitUi?226:154),static_cast<int16_t>(portraitUi?286:188),static_cast<int16_t>(portraitUi?346:222)};
    const char *detailLabel =
        bindingDraft_.target == AssignmentTarget::TransmitterFunction ? "FUNCTION" :
        bindingDraft_.target == AssignmentTarget::DriveMode ? "SELECTION" : "CHANNEL";
    const char *labels[] = {"ENABLED", "TARGET", detailLabel,
                            parameter ? "PARAMETER" : "MODE", "CONTEXT"};
    for (uint8_t row = 0; row < 5; ++row) {
        lv_obj_t *panel = makePanel(12, ys[row], kScreenWidth-24, portraitUi ? 54 : 30);
        makeLabel(panel, labels[row], 10, 6, 12, cDim);
    }
    makeButton(portraitUi ? 210 : 344, portraitUi ? 115 : 87, portraitUi ? 90 : 108, portraitUi ? 38 : 28, bindingDraft_.enabled ? "ON" : "OFF", 13,
               bindingDraft_.enabled ? cGreen : cPanel2,
               bindingDraft_.enabled ? cBlack : cWhite);
    addPageActionTarget(portraitUi ? 210 : 344, portraitUi ? 115 : 87, portraitUi ? 90 : 108, portraitUi ? 38 : 28,
                        &TransmitterUi::controlBindingEditor, 0);

    makeLabel(lv_scr_act(), assignmentTargetName(bindingDraft_.target),
              portraitUi ? 22 : 220, portraitUi ? 194 : 127, 12, bindingDraft_.target == AssignmentTarget::CrsfChannel
                  ? cWhite : cCyan);
    makeButton(portraitUi ? 210 : 350, portraitUi ? 174 : 121, 42, portraitUi ? 38 : 28, "<", 20);
    makeButton(portraitUi ? 258 : 408, portraitUi ? 174 : 121, 42, portraitUi ? 38 : 28, ">", 20);
    addPageEditorTarget(portraitUi ? 12 : 210, portraitUi ? 166 : 120, portraitUi ? 296 : 246, portraitUi ? 54 : 30,
                        portraitUi ? 210 : 350, portraitUi ? 174 : 121, 42, portraitUi ? 38 : 28, portraitUi ? 258 : 408, portraitUi ? 174 : 121, 42, portraitUi ? 38 : 28,
                        &TransmitterUi::controlBindingEditor, 1);

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
    makeLabel(lv_scr_act(), detail, portraitUi ? 22 : 220, portraitUi ? 254 : 161, 12, cWhite);
    makeButton(portraitUi ? 210 : 350, portraitUi ? 234 : 155, 42, portraitUi ? 38 : 28, "-", 20);
    makeButton(portraitUi ? 258 : 408, portraitUi ? 234 : 155, 42, portraitUi ? 38 : 28, "+", 20);
    addPageEditorTarget(portraitUi ? 12 : 210, portraitUi ? 226 : 154, portraitUi ? 296 : 246, portraitUi ? 54 : 30,
                        portraitUi ? 210 : 350, portraitUi ? 234 : 155, 42, portraitUi ? 38 : 28, portraitUi ? 258 : 408, portraitUi ? 234 : 155, 42, portraitUi ? 38 : 28,
                        &TransmitterUi::controlBindingEditor, 2);
    makeLabel(lv_scr_act(), parameter
                  ? channelParameterKindName(
                        channelParameterTargetKind(bindingDraft_.targetId))
                  : controlBehaviourName(bindingDraft_.behaviour),
              portraitUi ? 22 : 220, portraitUi ? 314 : 195, 12, cWhite);
    makeButton(portraitUi ? 210 : 350, portraitUi ? 294 : 189, 42, portraitUi ? 38 : 28, "<", 20);
    makeButton(portraitUi ? 258 : 408, portraitUi ? 294 : 189, 42, portraitUi ? 38 : 28, ">", 20);
    addPageEditorTarget(portraitUi ? 12 : 210, portraitUi ? 286 : 188, portraitUi ? 296 : 246, portraitUi ? 54 : 30,
                        portraitUi ? 210 : 350, portraitUi ? 294 : 189, 42, portraitUi ? 38 : 28, portraitUi ? 258 : 408, portraitUi ? 294 : 189, 42, portraitUi ? 38 : 28,
                        &TransmitterUi::controlBindingEditor, 3);

    makeLabel(lv_scr_act(), controlContextName(bindingDraft_.contexts),
              portraitUi ? 22 : 220, portraitUi ? 374 : 229, 12, cWhite);
    makeButton(portraitUi ? 210 : 350, portraitUi ? 354 : 223, 42, portraitUi ? 38 : 28, "<", 20);
    makeButton(portraitUi ? 258 : 408, portraitUi ? 354 : 223, 42, portraitUi ? 38 : 28, ">", 20);
    addPageEditorTarget(portraitUi ? 12 : 210, portraitUi ? 346 : 222, portraitUi ? 296 : 246, portraitUi ? 54 : 30,
                        portraitUi ? 210 : 350, portraitUi ? 354 : 223, 42, portraitUi ? 38 : 28, portraitUi ? 258 : 408, portraitUi ? 354 : 223, 42, portraitUi ? 38 : 28,
                        &TransmitterUi::controlBindingEditor, 4);

    if (bindingError_[0] != '\0')
        makeLabel(lv_scr_act(), bindingError_, 14, portraitUi ? 402 : 257, 10, cRed);
    else
        makeLabel(lv_scr_act(), parameter ? "Step: 1%; edits force CUT" :
                  portraitUi ? "Edits force CUT; re-arm at neutral" : "Assignment edits force CUT and neutral re-arm",
                  14, portraitUi ? 402 : 257, 10, cAmber);
    makeButton(12, portraitUi ? 416 : 274, portraitUi ? 92 : 138, portraitUi ? 40 : 34, "CANCEL", 13, cPanel);
    addPageActionTarget(12, portraitUi ? 416 : 274, portraitUi ? 92 : 138, portraitUi ? 40 : 34,
                        &TransmitterUi::controlBindingEditor, 5);
    if (bindingIndexForInput(selectedBindingInput_) >= 0) {
        makeButton(portraitUi ? 110 : 160, portraitUi ? 416 : 274, portraitUi ? 92 : 138, portraitUi ? 40 : 34, "REMOVE", 13, cAmber, cBlack);
        addPageActionTarget(portraitUi ? 110 : 160, portraitUi ? 416 : 274, portraitUi ? 92 : 138, portraitUi ? 40 : 34,
                            &TransmitterUi::controlBindingEditor, 6);
    }
    makeButton(portraitUi ? 208 : 308, portraitUi ? 416 : 274, portraitUi ? 100 : 160, portraitUi ? 40 : 34, "SAVE", 13, cGreen, cBlack);
    addPageActionTarget(portraitUi ? 208 : 308, portraitUi ? 416 : 274, portraitUi ? 100 : 160, portraitUi ? 40 : 34,
                        &TransmitterUi::controlBindingEditor, 7);
    if(portraitUi){
        for(uint32_t i=0;i<lv_obj_get_child_cnt(lv_scr_act());++i){
            auto obj=lv_obj_get_child(lv_scr_act(),i);
            if(lv_obj_check_type(obj,&lv_label_class)) {
                int y=lv_obj_get_y(obj);
                lv_obj_set_width(obj,y>=402?292:180);
                lv_label_set_long_mode(obj,LV_LABEL_LONG_DOT);
            }
        }
    }
    finishPageFocus();
}

void TransmitterUi::drawInputMonitorPage()
{
    activePage_ = UiPage::InputMonitor;
    startScreen();
    resetLvglObjects();
    makeHeader("INPUT MONITOR", "BACK");
    beginPageFocus();

    constexpr uint8_t kRowsPerBank = 5;
    constexpr uint8_t kBankCount =
        (kPhysicalControlCount + kRowsPerBank - 1) / kRowsPerBank;
    const size_t first = inputMonitorBank_ * kRowsPerBank;
    for (uint8_t row = 0; row < kRowsPerBank; ++row) {
        const size_t index = first + row;
        if (index >= kPhysicalControlCount) break;
        const PhysicalControlDescriptor &control = physicalControlDescriptor(index);
        const int16_t y = 54 + row * (portraitUi ? 67 : 43);
        lv_obj_t *panel = makePanel(10, y, kScreenWidth-20, portraitUi ? 60 : 37);
        makeLabel(panel, control.name, 8, 5, 12, cWhite);
        makeLabel(panel, r1InputEndpointLabel(control.id), portraitUi ? 8 : 150, portraitUi ? 33 : 6, 11, cDim);
        inputMonitorStateLabels_[row] = makeLabel(panel, "--", portraitUi ? 171 : 285, 5, 12, cWhite);
        inputMonitorEventLabels_[row] = makeLabel(panel, "--", portraitUi ? 171 : 370, portraitUi ? 33 : 6, 11, cDim);
    }

    makePageActionButton(12, portraitUi ? 410 : 274, 58, 34, "<", 22,
                         inputMonitorBank_ == 0 ? cBlack : cPanel,
                         inputMonitorBank_ == 0 ? cDim : cWhite, cLine,
                         &TransmitterUi::controlInputMonitor, 0);
    makePageActionButton(kScreenWidth-70, portraitUi ? 410 : 274, 58, 34, ">", 22,
                         inputMonitorBank_ + 1 >= kBankCount ? cBlack : cPanel,
                         inputMonitorBank_ + 1 >= kBankCount ? cDim : cWhite,
                         cLine, &TransmitterUi::controlInputMonitor, 1);
    char page[16];
    snprintf(page, sizeof(page), "%u / %u", inputMonitorBank_ + 1, kBankCount);
    lv_obj_t *pageLabel = makeLabel(lv_scr_act(), page, 0, portraitUi ? 420 : 284, 14, cDim);
    lv_obj_align(pageLabel, LV_ALIGN_TOP_MID, 0, portraitUi ? 420 : 284);
    refreshInputMonitorRows();
    finishPageFocus();
}

void TransmitterUi::refreshInputMonitorRows()
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

void TransmitterUi::drawChannelRolesPage()
{
    if (portraitUi) { drawPortraitDetails(UiPage::ChannelRoles); return; }
    activePage_ = UiPage::ChannelRoles;
    startScreen();
    resetLvglObjects();
    makeHeader("CHANNEL ROLES", "BACK");
    beginPageFocus();
    drawChannelRoleRow(0);
    addPageEditorTarget(18, 72, 444, 56,
                        350, 80, 44, 40, 404, 80, 44, 40,
                        &TransmitterUi::controlChannelRoles, 0);
    drawChannelRoleRow(1);
    addPageEditorTarget(18, 140, 444, 56,
                        350, 148, 44, 40, 404, 148, 44, 40,
                        &TransmitterUi::controlChannelRoles, 1);
    lv_obj_t *note = makeLabel(lv_scr_act(),
        "Changing roles disarms throttle output.\n"
        "Steering uses the proportional input on GPIO2.",
        0, 222, 14, cAmber);
    lv_obj_set_style_text_align(note, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 222);
    finishPageFocus();
}

void TransmitterUi::drawChannelPlaceholder()
{
    if (portraitUi) { drawPortraitDetails(UiPage::ChannelPlaceholder); return; }
    activePage_ = UiPage::ChannelPlaceholder;
    startScreen();
    resetLvglObjects();
    beginPageFocus();
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
        addPageEditorTarget(12, y, 456, 56,
                            352, y + 8, 42, 40,
                            406, y + 8, 42, 40,
                            &TransmitterUi::controlChannelConfig, row);
    }
    makeLabel(lv_scr_act(), "A trim control can own one output or parameter",
              18, 214, 11, cAmber);
    const bool throttle = selectedChannel_ == channelRoleSettings_.throttleChannel;
    const bool proportional = channelHasProportionalControl(selectedChannel_);
    if (throttle)
        makePageActionButton(52, 249, 176, 46, "BASIC / CAL", 14,
                             cPanel2, cWhite, cLine,
                             &TransmitterUi::controlChannelConfig, 2);
    else if (proportional)
        makePageActionButton(52, 249, 176, 46, "CALIBRATE", 14,
                             cPanel2, cWhite, cLine,
                             &TransmitterUi::controlChannelConfig, 2);
    if (proportional)
        makePageActionButton(238, 249, 190, 46, "RESPONSE / RAMP", 14,
                             cPanel2, cCyan, cLine,
                             &TransmitterUi::controlChannelConfig, 3);
    finishPageFocus();
}

void TransmitterUi::drawSettingRow(int row)
{
    static const char *labels[] = {"BRAKE/REV END", "SUB-TRIM", "FORWARD END", "DIRECTION"};
    const int16_t y = 56 + row * 50;
    lv_obj_t *panel = makePanel(10, y, 460, 44);
    makeLabel(panel, labels[row], 12, 12, 16, cCyan);
    char value[20];
    if (row == 0) snprintf(value, sizeof(value), "%u%%", throttleProfile().negativeEndpointPercent);
    else if (row == 1) snprintf(value, sizeof(value), "%+d%%", throttleProfile().subtrimPercent);
    else if (row == 2) snprintf(value, sizeof(value), "%u%%", throttleProfile().positiveEndpointPercent);
    else snprintf(value, sizeof(value), "%s", throttleProfile().reversed ? "REVERSE" : "NORMAL");
    channel2SettingValueLabels_[row] = makeLabel(
        panel, value, 212, 12, 16,
        row == 3 && throttleProfile().reversed ? cAmber : cWhite);
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
                                  colour(throttleProfile().reversed ? cAmber : cPanel2), 0);
        lv_obj_set_style_border_color(channel2DirectionToggle_, colour(cLine), 0);
        lv_obj_set_style_border_width(channel2DirectionToggle_, 1, 0);
        lv_obj_set_style_radius(channel2DirectionToggle_, 5, 0);
        lv_obj_set_style_pad_all(channel2DirectionToggle_, 0, 0);
        channel2DirectionToggleLabel_ = makeLabel(
            channel2DirectionToggle_, "TOGGLE", 0, 0, 14,
            throttleProfile().reversed ? cBlack : cWhite);
        lv_obj_center(channel2DirectionToggleLabel_);
    }
}

void TransmitterUi::refreshSettingRowValue(uint8_t row)
{
    if (row >= 4 || channel2SettingValueLabels_[row] == nullptr) return;
    if (row == 0)
        lv_label_set_text_fmt(channel2SettingValueLabels_[row], "%u%%",
                              throttleProfile().negativeEndpointPercent);
    else if (row == 1)
        lv_label_set_text_fmt(channel2SettingValueLabels_[row], "%+d%%",
                              throttleProfile().subtrimPercent);
    else if (row == 2)
        lv_label_set_text_fmt(channel2SettingValueLabels_[row], "%u%%",
                              throttleProfile().positiveEndpointPercent);
    else {
        lv_label_set_text(channel2SettingValueLabels_[row],
                          throttleProfile().reversed ? "REVERSE" : "NORMAL");
        lv_obj_set_style_text_color(channel2SettingValueLabels_[row],
            colour(throttleProfile().reversed ? cAmber : cWhite), 0);
        if (channel2DirectionToggle_ != nullptr)
            lv_obj_set_style_bg_color(channel2DirectionToggle_,
                colour(throttleProfile().reversed ? cAmber : cPanel2), 0);
        if (channel2DirectionToggleLabel_ != nullptr)
            lv_obj_set_style_text_color(channel2DirectionToggleLabel_,
                colour(throttleProfile().reversed ? cBlack : cWhite), 0);
    }
}

void TransmitterUi::drawPage()
{
    if (portraitUi) { drawPortraitDetails(UiPage::Channel2); return; }
    activePage_ = UiPage::Channel2;
    startScreen();
    resetLvglObjects();
    beginPageFocus(false);
    char title[24];
    snprintf(title, sizeof(title), "CH%u THROTTLE", channelRoleSettings_.throttleChannel);
    makeHeader(title);
    makeButton(190, 5, 68, 34, "RESP", 11, cPanel2, cCyan);
    makeButton(266, 5, 70, 34, "BACK", 11);
    makeButton(344, 5, 78, 34, "CAL", 11);
    addPageActionTarget(190, 5, 68, 34,
                        &TransmitterUi::controlThrottleHeader, 0);
    addPageActionTarget(266, 5, 70, 34,
                        &TransmitterUi::controlNavigation, 0);
    addPageActionTarget(344, 5, 78, 34,
                        &TransmitterUi::controlThrottleHeader, 1);
    addPageActionTarget(430, 5, 42, 34,
                        &TransmitterUi::controlNavigation, 1);
    for (int row = 0; row < 4; ++row) {
        drawSettingRow(row);
        addPageEditorTarget(10, 56 + row * 50, 460, 44,
                            353, 59 + row * 50, 48, 36,
                            411, 59 + row * 50, 48, 36,
                            &TransmitterUi::controlThrottleSetting, row);
    }
    char channelLabel[16];
    snprintf(channelLabel, sizeof(channelLabel), "CH%u", channelRoleSettings_.throttleChannel);
    makeLabel(lv_scr_act(), channelLabel, 12, 283, 16, cCyan);
    channel2ValueLabel_ = makeLabel(lv_scr_act(), "992", 422, 283, 14, cGreen);
    channel2LiveBar_ = makeBar(lv_scr_act(), 60, 287, 350, 14,
                               kCrsfMinimum, kCrsfMaximum, cGreen);
    lv_bar_set_value(channel2LiveBar_, kCrsfMid, LV_ANIM_OFF);
    makeLabel(lv_scr_act(), throttleProfile().reversed ? "FORWARD" : "BRAKE/REV", 60, 304, 12, cDim);
    makeLabel(lv_scr_act(), throttleProfile().reversed ? "BRAKE/REV" : "FORWARD", 338, 304, 12, cDim);
    lastDisplayedCrsf_ = UINT16_MAX;
    finishPageFocus();
}

void TransmitterUi::drawThrottleResponseRow(int row)
{
    const bool throttle = selectedChannel_ == channelRoleSettings_.throttleChannel;
    const bool steering = selectedChannel_ == channelRoleSettings_.steeringChannel;
    const char *positive = throttle ? "FWD" : steering ? "RIGHT" : "POS";
    const char *negative = throttle ? "BRK" : steering ? "LEFT" : "NEG";
    char labels[7][16];
    snprintf(labels[0], sizeof(labels[0]), "SUB-TRIM");
    snprintf(labels[1], sizeof(labels[1]), "%s END", positive);
    snprintf(labels[2], sizeof(labels[2]), "%s RATE", positive);
    snprintf(labels[3], sizeof(labels[3]), "%s EXPO", positive);
    snprintf(labels[4], sizeof(labels[4]), "%s END", negative);
    snprintf(labels[5], sizeof(labels[5]), "%s RATE", negative);
    snprintf(labels[6], sizeof(labels[6]), "%s EXPO", negative);
    const ChannelResponseProfile &profile = selectedResponseProfile();
    const int16_t y = 52 + row * 35;
    lv_obj_t *panel = makePanel(288, y, 184, 32);
    makeLabel(panel, labels[row], 7, 4, 10, cCyan);
    char value[20];
    if (row == 0) snprintf(value, sizeof(value), "%+d%%", profile.subtrimPercent);
    else if (row == 1) snprintf(value, sizeof(value), "%u%%", profile.positiveEndpointPercent);
    else if (row == 2) snprintf(value, sizeof(value), "%u%%", profile.positiveRatePercent);
    else if (row == 3) snprintf(value, sizeof(value), "%+d%%", profile.positiveExpoPercent);
    else if (row == 4) snprintf(value, sizeof(value), "%u%%", profile.negativeEndpointPercent);
    else if (row == 5) snprintf(value, sizeof(value), "%u%%", profile.negativeRatePercent);
    else snprintf(value, sizeof(value), "%+d%%", profile.negativeExpoPercent);
    makeLabel(panel, value, 101, 4, 10, cWhite);

    lv_obj_t *minus = lv_obj_create(panel); position(minus, 105, 14, 31, 15);
    lv_obj_set_style_bg_color(minus, colour(cPanel2), 0);
    lv_obj_set_style_border_color(minus, colour(cLine), 0);
    lv_obj_set_style_border_width(minus, 1, 0);
    lv_obj_set_style_radius(minus, 5, 0);
    lv_obj_set_style_pad_all(minus, 0, 0);
    lv_obj_t *ml = makeLabel(minus, "-", 0, 0, 13); lv_obj_center(ml);
    lv_obj_t *plus = lv_obj_create(panel); position(plus, 143, 14, 31, 15);
    lv_obj_set_style_bg_color(plus, colour(cPanel2), 0);
    lv_obj_set_style_border_color(plus, colour(cLine), 0);
    lv_obj_set_style_border_width(plus, 1, 0);
    lv_obj_set_style_radius(plus, 5, 0);
    lv_obj_set_style_pad_all(plus, 0, 0);
    lv_obj_t *pl = makeLabel(plus, "+", 0, 0, 13); lv_obj_center(pl);
}

void TransmitterUi::drawThrottleResponseGraph()
{
    const ChannelResponseProfile &profile = selectedResponseProfile();
    constexpr int16_t graphX = 23;
    constexpr int16_t graphY = 26;
    constexpr int16_t graphWidth = 236;
    constexpr int16_t graphHeight = portraitUi ? 110 : 188;
    lv_obj_t *panel = makePanel(portraitUi ? 24 : 8, 52, 272, portraitUi ? 159 : 250);
    makeLabel(panel, "LIVE RESPONSE", 10, 7, 12, cCyan);

    // A restrained grid is easier to read than the original monochrome plot,
    // while retaining its instantly recognisable input/output relationship.
    for (uint8_t division = 0; division <= 4; ++division) {
        const int16_t x = graphX + division * graphWidth / 4;
        const int16_t y = graphY + division * graphHeight / 4;
        lv_obj_t *vertical = lv_obj_create(panel);
        position(vertical, x, graphY, 1, graphHeight + 1);
        lv_obj_set_style_bg_color(vertical, colour(division == 2 ? cDim : cLine), 0);
        lv_obj_set_style_border_width(vertical, 0, 0);
        lv_obj_set_style_pad_all(vertical, 0, 0);
        lv_obj_t *horizontal = lv_obj_create(panel);
        position(horizontal, graphX, y, graphWidth + 1, 1);
        lv_obj_set_style_bg_color(horizontal, colour(division == 2 ? cDim : cLine), 0);
        lv_obj_set_style_border_width(horizontal, 0, 0);
        lv_obj_set_style_pad_all(horizontal, 0, 0);
    }

    // The output axis includes the transmitter's +/-120% extended travel so
    // sub-trim can translate the whole response without visually clipping it.
    const auto outputY = [&](int16_t outputPermille) -> lv_coord_t {
        outputPermille = static_cast<int16_t>(constrain(
            outputPermille, -kExtendedTravelPermille, kExtendedTravelPermille));
        return static_cast<lv_coord_t>(graphY + graphHeight -
            static_cast<int32_t>(outputPermille + kExtendedTravelPermille) *
            graphHeight / (2 * kExtendedTravelPermille));
    };
    throttleLinearPoints_[0] = {graphX, outputY(-1000)};
    throttleLinearPoints_[1] = {
        static_cast<lv_coord_t>(graphX + graphWidth), outputY(1000)};
    lv_obj_t *linear = lv_line_create(panel);
    lv_line_set_points(linear, throttleLinearPoints_, 2);
    lv_obj_set_style_line_color(linear, colour(cDim), 0);
    lv_obj_set_style_line_width(linear, 1, 0);
    lv_obj_set_style_line_dash_width(linear, 4, 0);
    lv_obj_set_style_line_dash_gap(linear, 4, 0);

    const int16_t subtrim = static_cast<int16_t>(profile.subtrimPercent) * 10;
    int16_t brakeLimit = -static_cast<int16_t>(profile.negativeEndpointPercent) * 10;
    int16_t forwardLimit = static_cast<int16_t>(profile.positiveEndpointPercent) * 10;
    if (profile.reversed) {
        brakeLimit = -brakeLimit;
        forwardLimit = -forwardLimit;
    }
    const int16_t neutralY = outputY(subtrim);
    const int16_t brakeEndpointY = outputY(brakeLimit + subtrim);
    const int16_t forwardEndpointY = outputY(forwardLimit + subtrim);

    throttleSubtrimPoints_[0] = {graphX, static_cast<lv_coord_t>(neutralY)};
    throttleSubtrimPoints_[1] = {
        static_cast<lv_coord_t>(graphX + graphWidth),
        static_cast<lv_coord_t>(neutralY)};
    lv_obj_t *subtrimLine = lv_line_create(panel);
    lv_line_set_points(subtrimLine, throttleSubtrimPoints_, 2);
    lv_obj_set_style_line_color(subtrimLine, colour(cAmber), 0);
    lv_obj_set_style_line_width(subtrimLine, 1, 0);
    lv_obj_set_style_line_dash_width(subtrimLine, 5, 0);
    lv_obj_set_style_line_dash_gap(subtrimLine, 4, 0);

    throttleBrakeEndpointPoints_[0] = {graphX, static_cast<lv_coord_t>(neutralY)};
    throttleBrakeEndpointPoints_[1] = {graphX, brakeEndpointY};
    lv_obj_t *brakeEndpoint = lv_line_create(panel);
    lv_line_set_points(brakeEndpoint, throttleBrakeEndpointPoints_, 2);
    lv_obj_set_style_line_color(brakeEndpoint, colour(cAmber), 0);
    lv_obj_set_style_line_width(brakeEndpoint, 4, 0);
    throttleForwardEndpointPoints_[0] = {
        static_cast<lv_coord_t>(graphX + graphWidth), neutralY};
    throttleForwardEndpointPoints_[1] = {
        static_cast<lv_coord_t>(graphX + graphWidth), forwardEndpointY};
    lv_obj_t *forwardEndpoint = lv_line_create(panel);
    lv_line_set_points(forwardEndpoint, throttleForwardEndpointPoints_, 2);
    lv_obj_set_style_line_color(forwardEndpoint, colour(cAmber), 0);
    lv_obj_set_style_line_width(forwardEndpoint, 4, 0);

    for (uint8_t index = 0; index < kThrottleGraphPointCount; ++index) {
        const int16_t input = static_cast<int16_t>(-1000 +
            static_cast<int32_t>(index) * 2000 /
            (kThrottleGraphPointCount - 1));
        const int16_t output = applyChannelResponse(profile, input);
        throttleCurvePoints_[index].x = static_cast<lv_coord_t>(
            graphX + static_cast<int32_t>(input + 1000) * graphWidth / 2000);
        throttleCurvePoints_[index].y = outputY(output);
    }
    throttleGraphCurve_ = lv_line_create(panel);
    lv_line_set_points(throttleGraphCurve_, throttleCurvePoints_,
                       kThrottleGraphPointCount);
    lv_obj_set_style_line_color(throttleGraphCurve_, colour(cCyan), 0);
    lv_obj_set_style_line_width(throttleGraphCurve_, 3, 0);
    lv_obj_set_style_line_rounded(throttleGraphCurve_, true, 0);

    throttleGraphMarker_ = lv_obj_create(panel);
    position(throttleGraphMarker_, graphX + graphWidth / 2 - 5,
             neutralY - 5, 11, 11);
    lv_obj_set_style_radius(throttleGraphMarker_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(throttleGraphMarker_, colour(cAmber), 0);
    lv_obj_set_style_border_color(throttleGraphMarker_, colour(cWhite), 0);
    lv_obj_set_style_border_width(throttleGraphMarker_, 2, 0);
    lv_obj_set_style_pad_all(throttleGraphMarker_, 0, 0);

    const bool throttle = selectedChannel_ == channelRoleSettings_.throttleChannel;
    const bool steering = selectedChannel_ == channelRoleSettings_.steeringChannel;
    makeLabel(panel, throttle ? "BRAKE" : steering ? "LEFT" : "NEG",
              graphX, portraitUi ? 140 : 220, 10, cDim);
    lv_obj_t *neutral = makeLabel(panel, "N", 0, portraitUi ? 140 : 220, 10, cDim);
    lv_obj_align(neutral, LV_ALIGN_TOP_MID, 0, portraitUi ? 140 : 220);
    makeLabel(panel, throttle ? "FWD" : steering ? "RIGHT" : "POS",
              graphX + graphWidth - (steering ? 34 : 25), portraitUi ? 140 : 220, 10, cDim);
    throttleGraphLiveLabel_ = makeLabel(panel, "IN +0%  OUT +0%",
                                        111, 6, 9, cWhite);
    refreshThrottleResponseGraph(mapAdcToPermille(latestRawAdc_));
}

void TransmitterUi::refreshThrottleResponseGraph(int16_t rawInputPermille)
{
    if (!throttleGraphMarker_ || !throttleGraphLiveLabel_) return;
    constexpr int16_t graphX = 23;
    constexpr int16_t graphY = 26;
    constexpr int16_t graphWidth = 236;
    constexpr int16_t graphHeight = portraitUi ? 110 : 188;
    rawInputPermille = static_cast<int16_t>(constrain(rawInputPermille,
                                                      -1000, 1000));
    rawInputPermille = proportionalInputValue(selectedChannel_, rawInputPermille);
    const int16_t output = applyChannelResponse(selectedResponseProfile(),
                                                rawInputPermille);
    const int16_t x = graphX + static_cast<int32_t>(rawInputPermille + 1000) *
                      graphWidth / 2000;
    const int16_t y = graphY + graphHeight -
        static_cast<int32_t>(output + kExtendedTravelPermille) * graphHeight /
        (2 * kExtendedTravelPermille);
    lv_obj_set_pos(throttleGraphMarker_, x - 5, y - 5);
    lv_label_set_text_fmt(throttleGraphLiveLabel_, "IN %+d%%  OUT %+d%%",
                          rawInputPermille / 10, output / 10);
}

void TransmitterUi::drawThrottleResponsePage()
{
    if (portraitUi) { drawPortraitDetails(UiPage::ThrottleResponse); return; }
    activePage_ = UiPage::ThrottleResponse;
    startScreen();
    resetLvglObjects();
    beginPageFocus();
    char title[24];
    snprintf(title, sizeof(title), "CH%u RESPONSE", selectedChannel_);
    makeHeader(title, "BACK");
    makeButton(258, 5, 78, 34, "RAMP", 11, cPanel2, cCyan);
    addPageActionTarget(258, 5, 78, 34,
                        &TransmitterUi::controlThrottleHeader, 0);
    drawThrottleResponseGraph();
    for (int row = 0; row < 7; ++row) {
        drawThrottleResponseRow(row);
        addPageEditorTarget(288, 52 + row * 35, 184, 32,
                            393, 66 + row * 35, 31, 15,
                            431, 66 + row * 35, 31, 15,
                            &TransmitterUi::controlThrottleResponse, row);
    }
    finishPageFocus();
}

void TransmitterUi::drawChannelSpeedRow(int row)
{
    const ChannelResponseProfile &profile = selectedResponseProfile();
    const bool throttle = selectedChannel_ == channelRoleSettings_.throttleChannel;
    const bool steering = selectedChannel_ == channelRoleSettings_.steeringChannel;
    const char *positive = throttle ? "FORWARD" : steering ? "RIGHT" : "POSITIVE";
    const char *negative = throttle ? "BRAKE/REV" : steering ? "LEFT" : "NEGATIVE";
    const int16_t y = 78 + row * 76;
    lv_obj_t *panel = makePanel(14, y, 452, 62);
    char label[28];
    snprintf(label, sizeof(label), "%s RAMP TIME", row == 0 ? positive : negative);
    makeLabel(panel, label, 12, 10, 16, cCyan);
    const uint16_t value = row == 0 ? profile.positiveRampMs : profile.negativeRampMs;
    char text[20];
    if (value == 0) snprintf(text, sizeof(text), "DIRECT");
    else snprintf(text, sizeof(text), "%u ms", value);
    makeLabel(panel, text, 242, 10, 16, cWhite);
    makeButton(350, y + 20, 42, 34, "-", 22);
    makeButton(402, y + 20, 42, 34, "+", 22);
}

void TransmitterUi::drawChannelSpeedPage()
{
    if (portraitUi) { drawPortraitDetails(UiPage::ChannelSpeed); return; }
    activePage_ = UiPage::ChannelSpeed;
    startScreen();
    resetLvglObjects();
    beginPageFocus();
    char title[24];
    snprintf(title, sizeof(title), "CH%u OUTPUT RAMP", selectedChannel_);
    makeHeader(title, "BACK");
    drawChannelSpeedRow(0);
    addPageEditorTarget(14, 78, 452, 62,
                        350, 98, 42, 34, 402, 98, 42, 34,
                        &TransmitterUi::controlChannelSpeed, 0);
    drawChannelSpeedRow(1);
    addPageEditorTarget(14, 154, 452, 62,
                        350, 174, 42, 34, 402, 174, 42, 34,
                        &TransmitterUi::controlChannelSpeed, 1);
    lv_obj_t *note = makeLabel(lv_scr_act(),
        "0 ms = direct. Ramp is neutral-to-endpoint time.", 0, 250, 14, cDim);
    lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 250);
    finishPageFocus();
}

void TransmitterUi::drawDriveModesPage()
{
    if (portraitUi) { drawPortraitDetails(UiPage::DriveModes); return; }
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
                            &TransmitterUi::controlDriveMode, index);
    }
    lv_obj_t *panel = makePanel(16, 150, 448, 66);
    makeLabel(panel, "FORWARD LIMIT", 14, 20, 18, cCyan);
    char value[12]; snprintf(value, sizeof(value), "%u%%", driveModeSettings_.walkLimitPercent);
    makeLabel(panel, value, 260, 20, 18);
    makeButton(358, 164, 42, 38, "-", 22);
    makeButton(410, 164, 42, 38, "+", 22);
    addPageEditorTarget(16, 150, 448, 66,
                        358, 164, 42, 38, 410, 164, 42, 38,
                        &TransmitterUi::controlDriveMode, 2);
    makeLabel(lv_scr_act(),
        "WALK limits forward only; brake/reverse stays at 100%.\nChanging mode forces CUT; release trigger before arming.",
        20, 242, 14, cAmber);
    finishPageFocus();
}

void TransmitterUi::drawLiveIndicator(int16_t, uint16_t crsfValue)
{
    if (channel2LiveBar_ && crsfValue != lastDisplayedCrsf_) {
        lv_bar_set_value(channel2LiveBar_, crsfValue, LV_ANIM_OFF);
        lv_label_set_text_fmt(channel2ValueLabel_, "%4u", crsfValue);
        lastDisplayedCrsf_ = crsfValue;
    }
}

void TransmitterUi::drawSetupRow(int row)
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

void TransmitterUi::drawSetupPage()
{
    if (portraitUi) { drawPortraitDetails(UiPage::Setup); return; }
    activePage_ = UiPage::Setup;
    startScreen();
    resetLvglObjects();
    makeHeader("SYSTEM SETUP", "BACK");
    beginPageFocus();
    makeButton(258, 5, 78, 34, "INFO", 11, cPanel2, cCyan);
    addPageActionTarget(258, 5, 78, 34,
                        &TransmitterUi::controlSetup, 8);
    for (int row = 0; row < 4; ++row) drawSetupRow(row);
    addPageActionTarget(353, 60, 106, 36,
                        &TransmitterUi::controlSetup, 0);
    addPageActionTarget(353, 110, 106, 36,
                        &TransmitterUi::controlSetup, 1);
    addPageEditorTarget(10, 156, 460, 44,
                        353, 159, 48, 36, 411, 159, 48, 36,
                        &TransmitterUi::controlSetup, 2);
    addPageEditorTarget(10, 206, 460, 44,
                        353, 209, 48, 36, 411, 209, 48, 36,
                        &TransmitterUi::controlSetup, 3);
    makeButton(10, 265, 88, 44, "TEST", 14, cPanel2, cAmber);
    addPageActionTarget(10, 265, 88, 44,
                        &TransmitterUi::controlSetup, 4);
    makeButton(106, 265, 110, 44, "DISPLAY", 13, cPanel2, cCyan);
    addPageActionTarget(106, 265, 110, 44,
                        &TransmitterUi::controlSetup, 5);
    makeButton(224, 265, 116, 44, "TX BAT", 13, cPanel2, cCyan);
    addPageActionTarget(224, 265, 116, 44,
                        &TransmitterUi::controlSetup, 6);
    makeButton(348, 265, 122, 44, "RX BAT", 13, cPanel2, cCyan);
    addPageActionTarget(348, 265, 122, 44,
                        &TransmitterUi::controlSetup, 7);
    finishPageFocus();
}

void TransmitterUi::drawFirmwareInfoPage()
{
    activePage_ = UiPage::FirmwareInfo;
    startScreen();
    resetLvglObjects();
    makeHeader("FIRMWARE INFO", "BACK");
    beginPageFocus();

    lv_obj_t *identity = makePanel(10, 58, kScreenWidth-20, portraitUi ? 106 : 72);
    makeLabel(identity, JR_R1_FIRMWARE_NAME, 14, 10, 20, cCyan);
    char revision[80];
    snprintf(revision, sizeof(revision), "%s @ %s  [%s]",
             JR_R1_FIRMWARE_BRANCH, JR_R1_FIRMWARE_COMMIT,
             JR_R1_FIRMWARE_GIT_STATE);
    makeLabel(identity, revision, 14, 40, 12,
              strcmp(JR_R1_FIRMWARE_GIT_STATE, "clean") == 0
                  ? cGreen : cAmber);

    lv_obj_t *build = makePanel(10, portraitUi ? 180 : 132, kScreenWidth-20, portraitUi ? 126 : 82);
    makeLabel(build, "BUILD", 14, 9, 12, cDim);
    char builtAt[48];
    snprintf(builtAt, sizeof(builtAt), "%s  %s",
             JR_R1_FIRMWARE_BUILD_DATE, JR_R1_FIRMWARE_BUILD_TIME);
    makeLabel(build, builtAt, 14, 29, 16, cWhite);
    makeLabel(build, JR_R1_FIRMWARE_BUILD_ENV, 14, 54, 13, cCyan);

    lv_obj_t *hardware = makePanel(10, portraitUi ? 322 : 222, kScreenWidth-20, portraitUi ? 126 : 76);
    makeLabel(hardware, "HARDWARE PROFILE", 14, 8, 12, cDim);
#ifdef JR_R1_XIAO_PCB
    makeLabel(hardware, "Seeed XIAO ESP32-S3 / R1 PCB", 14, 28, 14, cWhite);
#else
    makeLabel(hardware, "ESP32-S3-DevKitC-1 N16R8", 14, 28, 14, cWhite);
#endif
    makeLabel(hardware, DISPLAY_PROFILE_NAME, 14, 50, 13, cCyan);
    if(portraitUi){for(auto panel : {identity,build,hardware}) {for(uint32_t i=0;i<lv_obj_get_child_cnt(panel);++i){auto label=lv_obj_get_child(panel,i);lv_obj_set_width(label,270);lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);}}}
    finishPageFocus();
}

void TransmitterUi::drawElrsTxConfigPage()
{
    if (activePage_ != UiPage::ElrsTxConfig) {
        armRequested_ = false;
        safetyResetRequested_ = true;
    }
    activePage_ = UiPage::ElrsTxConfig;
    startScreen(); resetLvglObjects();
    makeHeader("ELRS TX SETTINGS", "BACK");
    beginPageFocus();
    auto label = makeLabel(lv_scr_act(), txConfig_.status(), 8, 48, 12, cAmber);
    lv_obj_set_width(label, kScreenWidth - 16);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    const auto selected = txConfig_.parameter(txConfigSelected_);
    const int width = kScreenWidth - 16;
    const int footer = kScreenHeight - (portraitUi ? 62 : 40);
    if (selected) {
        label = makeLabel(lv_scr_act(), selected->name, 8, 92, 17, cCyan);
        lv_obj_set_width(label, width);
        char value[384] = {};
        if (selected->type == 9) {
            selected->option(txConfigDraft_, value, sizeof(value));
            const size_t length = strlen(value);
            if (selected->info[0]) snprintf(value+length,sizeof(value)-length," %s",selected->info);
        }
        else snprintf(value, sizeof(value), "%s", selected->info);
        label = makeLabel(lv_scr_act(), value, 8, 124, 15, cWhite);
        lv_obj_set_width(label, width); lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        if (selected->type == 9) {
            makePageActionButton(8, 192, width/2-4, 34, "PREVIOUS VALUE", 12,
                cPanel2, cWhite, cLine, &TransmitterUi::controlElrsTxConfig, 8);
            makePageActionButton(12+width/2, 192, width/2-4, 34, "NEXT VALUE", 12,
                cPanel2, cWhite, cLine, &TransmitterUi::controlElrsTxConfig, 9);
        }
        if (selected->type == 9 || selected->type == 13) {
            const char *action = selected->type == 9 ? "APPLY" :
                (txConfig_.confirming() ? "CONFIRM" : "RUN COMMAND");
            makePageActionButton(8, footer, width/2-4, 32, action, 13,
                cPanel2, cAmber, cLine, &TransmitterUi::controlElrsTxConfig, 10);
        }
        makePageActionButton(12+width/2, footer, width/2-4, 32, "CANCEL / BACK", 12,
            cPanel2, cWhite, cLine, &TransmitterUi::controlElrsTxConfig, 11);
        finishPageFocus();
        return;
    }
    const auto folder = txConfig_.parameter(txConfigFolder_);
    label = makeLabel(lv_scr_act(), folder ? folder->name : txConfig_.device(), 8, 78, 12, cCyan);
    lv_obj_set_width(label, width);
    uint8_t row=0, skipped=0;
    memset(txConfigRows_, 0, sizeof(txConfigRows_));
    for (uint8_t id=1; id<=txConfig_.count(); ++id) {
        const auto p=txConfig_.parameter(id);
        if (!p || p->hidden || p->parent!=txConfigFolder_) continue;
        if (skipped++ < txConfigOffset_) continue;
        if (row==4) break;
        txConfigRows_[row]=id;
        char text[128], value[384] = {};
        if (p->type==9) p->option(p->value,value,sizeof(value));
        snprintf(text,sizeof(text),"%.47s%s%.64s",p->name,
            p->type==11 ? " >" : (p->type==9 ? ": " : ""),value);
        makePageActionButton(8, 100+row*(portraitUi?68:38), width, portraitUi?60:34, text, portraitUi?14:12,
            cPanel2, cWhite, cLine, &TransmitterUi::controlElrsTxConfig,row);
        if(portraitUi){auto button=lv_obj_get_child(lv_scr_act(),-1);auto textLabel=lv_obj_get_child(button,0);lv_obj_set_width(textLabel,width-20);lv_label_set_long_mode(textLabel,LV_LABEL_LONG_WRAP);lv_obj_set_style_text_align(textLabel,LV_TEXT_ALIGN_LEFT,0);lv_obj_center(textLabel);}
        ++row;
    }
    const int cell=width/4;
    const char *actions[]={"PREV","NEXT","REFRESH","BINDING"};
    for (uint8_t i=0;i<4;++i)
        makePageActionButton(8+i*cell,footer,cell-4,32,actions[i],11,
            cPanel2,cCyan,cLine,&TransmitterUi::controlElrsTxConfig,4+i);
    finishPageFocus();
}

void TransmitterUi::leaveElrsTxConfig()
{
    if (txConfig_.busyCommand()) {
        txConfig_.command(txConfigSelected_, 5, millis());
        return; // wait for cancellation, or use Refresh after a lost connection
    }
    if (txConfigSelected_) {
        txConfigSelected_=0; txConfigDraftDirty_=false;
    } else if (txConfigFolder_) {
        const auto p=txConfig_.parameter(txConfigFolder_);
        txConfigFolder_=p ? p->parent : 0; txConfigOffset_=0;
    } else {
        txConfig_.stop(); drawMainMenu(); return;
    }
    drawElrsTxConfigPage();
}

void TransmitterUi::controlElrsTxConfig(uint8_t item, PageControlEvent)
{
    if (item<4 && !txConfigSelected_) {
        if (!txConfig_.ready()) return;
        const uint8_t id=txConfigRows_[item];
        const auto p=txConfig_.parameter(id);
        if (!p) return;
        if (p->type==11) { txConfigFolder_=id; txConfigOffset_=0; }
        else { txConfigSelected_=id; txConfigDraft_=p->value; txConfigDraftDirty_=false; }
    } else if (item==4) {
        txConfigOffset_=txConfigOffset_>=4 ? txConfigOffset_-4 : 0;
    } else if (item==5) {
        unsigned children=0;
        for (uint8_t id=1;id<=txConfig_.count();++id) {
            const auto p=txConfig_.parameter(id);
            if (p && !p->hidden && p->parent==txConfigFolder_) ++children;
        }
        if (txConfigOffset_+4<children) txConfigOffset_+=4;
    } else if (item==6) {
        txConfig_.start(millis()); txConfigFolder_=txConfigOffset_=txConfigSelected_=0;
        txConfigDraftDirty_=false;
    } else if (item==7) {
        txConfig_.stop(); drawElrsBindingPage(); return;
    } else if (item==8 || item==9) {
        const auto p=txConfig_.parameter(txConfigSelected_);
        if (!txConfig_.ready() || !p || p->type!=9) return;
        const int step=item==8 ? -1 : 1;
        for (int value=int(txConfigDraft_)+step;value>=p->minimum && value<=p->maximum;value+=step) {
            char text[384];
            if (p->option(value,text,sizeof(text))) {
                txConfigDraft_=value; txConfigDraftDirty_=true; break;
            }
        }
    } else if (item==10) {
        const auto p=txConfig_.parameter(txConfigSelected_);
        if (!p) return;
        if (p->type==9) {
            if (txConfig_.apply(txConfigSelected_,txConfigDraft_,millis())) txConfigDraftDirty_=false;
        } else if (p->type==13) {
            txConfig_.command(txConfigSelected_,txConfig_.confirming() ? 4 : 1,millis());
        }
    } else if (item==11) { leaveElrsTxConfig(); return; }
    playKeyClick(); drawElrsTxConfigPage();
}

void TransmitterUi::handleElrsTxConfigTouch(int16_t x,int16_t y)
{
    if (headerActionTouched(x,y)) { leaveElrsTxConfig(); return; }
    if (y>=kScreenHeight-40) {
        if (txConfigSelected_) controlElrsTxConfig(x<kScreenWidth/2 ? 10 : 11,PageControlEvent::Activate);
        else controlElrsTxConfig(4+constrain((x-8)/((kScreenWidth-16)/4),0,3),PageControlEvent::Activate);
    } else if (txConfigSelected_ && y>=192 && y<226) {
        controlElrsTxConfig(x<kScreenWidth/2 ? 8 : 9,PageControlEvent::Activate);
    } else if (!txConfigSelected_ && y>=100 && y<252) {
        controlElrsTxConfig((y-100)/38,PageControlEvent::Activate);
    }
}

void TransmitterUi::drawElrsBindingPage()
{
    activePage_ = UiPage::ElrsBinding;
    startScreen();
    resetLvglObjects();
    makeHeader("ELRS BINDING", "BACK");
    beginPageFocus();

    lv_obj_t *phrasePanel = makePanel(8, 50, kScreenWidth-16, portraitUi ? 86 : 43);
    char phraseText[64];
    snprintf(phraseText, sizeof(phraseText), "PHRASE: %s",
             elrsBindPhrase_[0] == '\0' ? "<blank / unbound>" : elrsBindPhrase_);
    auto phraseLabel=makeLabel(phrasePanel, phraseText, 8, 3, 13, cWhite);
    lv_obj_set_width(phraseLabel,kScreenWidth-34);lv_label_set_long_mode(phraseLabel,LV_LABEL_LONG_WRAP);
    auto statusLabel=makeLabel(phrasePanel, elrsStatus_, 8, portraitUi ? 51 : 22, 11,
              strstr(elrsStatus_, "MATCH") != nullptr ? cGreen : cAmber);
    lv_obj_set_width(statusLabel,kScreenWidth-34);lv_label_set_long_mode(statusLabel,LV_LABEL_LONG_WRAP);

    const char *actions[] = {"SET TX", "SET RX", "UID TX", "UID RX", "CLEAR"};
    for (uint8_t index = 0; index < 5; ++index) {
        const bool unavailable = (index == 1 || index == 3) &&
                                 !elrsReceiverLinkAvailable_;
        makePageActionButton(portraitUi ? 8+(index%3)*103 : 8+index*93, portraitUi ? 143+(index/3)*42 : 94, portraitUi ? 98 : 87, portraitUi ? 38 : 32,
                             actions[index], 11,
                             unavailable ? cBlack : cPanel2,
                             unavailable ? cDim :
                                 (index < 2 ? cAmber : cCyan),
                             cLine, &TransmitterUi::controlElrsBinding, index);
    }

    const char *rows[] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm-_"};
    const uint8_t lengths[] = {10, 10, 9, 9};
    for (uint8_t row = 0; row < 4; ++row) {
        constexpr int16_t width = portraitUi ? 31 : 42;
        const int16_t startX = (kScreenWidth - lengths[row] * width) / 2;
        for (uint8_t column = 0; column < lengths[row]; ++column) {
            char label[2] = {rows[row][column], '\0'};
            if (elrsKeyboardUppercase_ && label[0] >= 'a' && label[0] <= 'z')
                label[0] = static_cast<char>(label[0] - 'a' + 'A');
            uint8_t keyIndex = 0;
            for (uint8_t prior = 0; prior < row; ++prior)
                keyIndex += lengths[prior];
            keyIndex += column;
            makePageActionButton(startX + column * width, portraitUi ? 228+row*45 : 130+row*38,
                                 width - 3, portraitUi ? 42 : 34, label, 14,
                                 cPanel, cWhite, cLine,
                                 &TransmitterUi::controlElrsBinding,
                                 5 + keyIndex);
        }
    }
    makePageActionButton(portraitUi ? 8 : 43, portraitUi ? 414 : 282, portraitUi ? 72 : 92, portraitUi ? 40 : 32,
                         elrsKeyboardUppercase_ ? "lower" : "UPPER", 12,
                         cPanel2, cWhite, cLine,
                         &TransmitterUi::controlElrsBinding, 43);
    makePageActionButton(portraitUi ? 86 : 143, portraitUi ? 414 : 282, portraitUi ? 148 : 194, portraitUi ? 40 : 32, "SPACE", 12,
                         cPanel2, cWhite, cLine,
                         &TransmitterUi::controlElrsBinding, 44);
    makePageActionButton(portraitUi ? 240 : 345, portraitUi ? 414 : 282, portraitUi ? 72 : 92, portraitUi ? 40 : 32, "<--", 14,
                         cPanel2, cAmber, cLine,
                         &TransmitterUi::controlElrsBinding, 45);
    finishPageFocus();
}

void TransmitterUi::drawEscProgrammingPage()
{
    activePage_ = UiPage::EscProgramming;
    startScreen();
    resetLvglObjects();
    makeHeader("ESC PROGRAMMER", "BACK");
    beginPageFocus();

    lv_obj_t *status = makePanel(8, 50, kScreenWidth-16, portraitUi ? 58 : 34, cSafetyWaiting, cAmber);
    makeLabel(status, LV_SYMBOL_WARNING "  PREVIEW ONLY - NO ESC LINK",
              12, 7, 13, cAmber);
    lv_obj_t *profile = makeLabel(status, "TS160 DRAFT", 0, 8, 11, cDim);
    lv_obj_align(profile, LV_ALIGN_TOP_RIGHT, -10, portraitUi ? 34 : 8);

    static const char *tabs[] = {"BASIC", "BRAKE", "MOTOR"};
    for (uint8_t tab = 0; tab < 3; ++tab) {
        const bool selected = escProgrammingTab_ == tab;
        makePageActionButton(8 + tab * (kScreenWidth-16)/3, portraitUi ? 118 : 87, (kScreenWidth-16)/3-4, portraitUi ? 40 : 32,
                             tabs[tab], 13,
                             selected ? cPanel2 : cBlack,
                             selected ? cCyan : cDim,
                             selected ? cCyan : cLine,
                             &TransmitterUi::controlEscProgramming, tab);
    }

    static const char *labels[3][4] = {
        {"RUN MODE", "LOW-V CUTOFF", "START / PUNCH", "BEC VOLTAGE"},
        {"MAX BRAKE", "DRAG BRAKE", "INITIAL BRAKE", "NEUTRAL RANGE"},
        {"ROTATION", "MOTOR TIMING", "PWM FREQUENCY", "THERMAL LIMIT"},
    };
    static const char *values[3][4] = {
        {"FWD / BRK / REV", "3.4 V / CELL", "LEVEL 5", "7.4 V"},
        {"75%", "0%", "= DRAG BRAKE", "9%"},
        {"NORMAL", "7.5 deg", "16 kHz", "105 C"},
    };

    const uint8_t page = min<uint8_t>(escProgrammingTab_, 2);
    for (uint8_t row = 0; row < 4; ++row) {
        const int16_t y = portraitUi ? 170+row*61 : 125+row*36;
        lv_obj_t *panel = makePanel(8, y, kScreenWidth-16, portraitUi ? 55 : 31, cPanel, cLine, 5);
        makeLabel(panel, labels[page][row], 10, 7, 12, cCyan);
        lv_obj_t *value = makeLabel(panel, values[page][row], 0, 7, 12, cWhite);
        lv_obj_align(value, LV_ALIGN_TOP_RIGHT, -12, portraitUi ? 30 : 7);
    }

    // These controls establish the intended workflow without suggesting that
    // a value has actually been read from or written to the ESC.
    makePageActionButton(8, portraitUi ? 416 : 274, portraitUi ? 100 : 168, portraitUi ? 40 : 35, "READ ESC", 13,
                         cBlack, cDim, cDim,
                         &TransmitterUi::controlEscProgramming, 3);
    makePageActionButton(portraitUi ? 114 : 184, portraitUi ? 416 : 274, portraitUi ? 100 : 168, portraitUi ? 40 : 35, "WRITE ESC", 13,
                         cBlack, cDim, cDim,
                         &TransmitterUi::controlEscProgramming, 4);
    lv_obj_t *comms = makePanel(portraitUi ? 220 : 360, portraitUi ? 416 : 274, portraitUi ? 92 : 112, portraitUi ? 40 : 35,
                                cSafetyWaiting, cAmber, 7);
    lv_obj_t *commsLabel = makeLabel(comms, "NO COMMS", 0, 0, 11, cAmber);
    lv_obj_center(commsLabel);
    finishPageFocus();
}

void TransmitterUi::drawDisplayRow(int row, int visibleRow)
{
    static const char *labels[] = {
        "SCREEN TIMEOUT", "IDLE ACTION", "UI BRIGHTNESS", "COLOUR SCHEME",
        "ACTIVATE NOW"
    };
    const int16_t y = 56 + visibleRow * 50;
    lv_obj_t *panel = makePanel(10, y, 460, 44);
    if (row == 3) {
        makeLabel(panel, labels[row], 12, 3, 10, cDim);
        makeLabel(panel, colourSchemeName(systemSettings_.nightMode, systemSettings_.darkColourScheme), 12, 20, 14, cWhite);
        makeButton(353, y + 4, 106, 36, "CHOOSE", 13);
        return;
    }
    makeLabel(panel, labels[row], 12, 12, 16, cCyan);
    char value[24];
    if (row == 0) {
        if (systemSettings_.screenTimeoutSeconds == 0) snprintf(value, sizeof(value), "NEVER");
        else snprintf(value, sizeof(value), "%u sec", systemSettings_.screenTimeoutSeconds);
    } else if (row == 1) {
        snprintf(value, sizeof(value), "%s",
                 systemSettings_.logoScreensaverEnabled ? "LOGO SAVER" : "DISPLAY OFF");
    } else if (row == 2) {
        snprintf(value, sizeof(value), "%u%%", systemSettings_.displayBrightnessPercent);
    } else {
        snprintf(value, sizeof(value), "%s",
                 systemSettings_.logoScreensaverEnabled ? "SHOW LOGO" : "DISPLAY OFF");
    }
    makeLabel(panel, value, 220, 12, 16, cWhite);
    if (row == 0 || row == 2) {
        makeButton(353, y + 4, 48, 36, "-", 24);
        makeButton(411, y + 4, 48, 36, "+", 24);
    } else {
        const bool activateRow = row == 4;
        makeButton(353, y + 4, 106, 36,
                   activateRow
                       ? (systemSettings_.logoScreensaverEnabled ? "START" : "SLEEP")
                       : "TOGGLE",
                   13, activateRow ? cAmber : cPanel2,
                   activateRow ? cBlack : cWhite);
    }
}

void TransmitterUi::controlColourScheme(uint8_t choice, PageControlEvent event)
{
    if (event != PageControlEvent::Activate || choice > darkColourSchemeCount) return;
    systemSettings_.nightMode = choice != 0;
    if (choice) systemSettings_.darkColourScheme = choice - 1;
    uiNightMode = systemSettings_.nightMode;
    uiDarkColourScheme = systemSettings_.darkColourScheme;
    saveSystemSettings();
    playKeyClick();
    drawColourSchemePage();
}

void TransmitterUi::drawColourSchemePage()
{
    activePage_ = UiPage::ColourScheme;
    startScreen();
    resetLvglObjects();
    makeHeader("Colour scheme", "Back");
    beginPageFocus();
    const int rowHeight = portraitUi ? 62 : 42;
    const uint8_t selected = systemSettings_.nightMode ? systemSettings_.darkColourScheme + 1 : 0;
    for (uint8_t choice = 0; choice <= darkColourSchemeCount; ++choice) {
        const int y = 54 + choice * rowHeight;
        auto panel = makePanel(8, y, kScreenWidth - 16, rowHeight - 4,
                               choice == selected ? cPanel2 : cPanel, cLine);
        makeLabel(panel, colourSchemeName(choice != 0, choice ? choice - 1 : 0),
                  12, (rowHeight - 20) / 2, 14, choice == selected ? cCyan : cWhite);
        if (choice == selected) makeLabel(panel, LV_SYMBOL_OK, kScreenWidth - 46,
                                         (rowHeight - 20) / 2, 14, cCyan);
        addPageActionTarget(8, y, kScreenWidth - 16, rowHeight - 4,
                            &TransmitterUi::controlColourScheme, choice);
    }
    makeLabel(lv_scr_act(), "Saved automatically. Night mode uses your last dark scheme.",
              12, portraitUi ? 376 : 275, 12, cDim);
    auto note = lv_obj_get_child(lv_scr_act(), -1);
    lv_obj_set_width(note, kScreenWidth - 24);
    finishPageFocus();
}

void TransmitterUi::drawDisplayPage()
{
    if (portraitUi) { drawPortraitDetails(UiPage::DisplaySetup); return; }
    activePage_ = UiPage::DisplaySetup;
    startScreen();
    resetLvglObjects();
    makeHeader("DISPLAY", "BACK");
    beginPageFocus();
    const int firstRow = displaySetupBank_ * 4;
    const int rowCount = min(4, 5 - firstRow);
    for (int visibleRow = 0; visibleRow < rowCount; ++visibleRow) {
        const int row = firstRow + visibleRow;
        drawDisplayRow(row, visibleRow);
        if (row == 0 || row == 2) {
            addPageEditorTarget(10, 56 + visibleRow * 50, 460, 44,
                                353, 60 + visibleRow * 50, 48, 36,
                                411, 60 + visibleRow * 50, 48, 36,
                                &TransmitterUi::controlDisplay, row);
        } else {
            addPageActionTarget(10, 56 + visibleRow * 50, 460, 44,
                                &TransmitterUi::controlDisplay, row);
        }
    }
    makeLabel(lv_scr_act(), "Logo saver dims to 25%; display off saves most power.",
              20, 258, 12, cDim);
    makeButton(10, 274, 50, 35, "<", 20, cPanel2,
               displaySetupBank_ == 0 ? cDim : cWhite);
    lv_obj_t *pageLabel = makeLabel(
        lv_scr_act(), displaySetupBank_ == 0 ? "DISPLAY 1/2" : "DISPLAY 2/2",
        0, 284, 13, cCyan);
    lv_obj_align(pageLabel, LV_ALIGN_TOP_MID, 0, 284);
    makeButton(420, 274, 50, 35, ">", 20, cPanel2,
               displaySetupBank_ == 1 ? cDim : cWhite);
    addPageActionTarget(10, 274, 50, 35,
                        &TransmitterUi::controlDisplay, 10);
    addPageActionTarget(420, 274, 50, 35,
                        &TransmitterUi::controlDisplay, 11);
    finishPageFocus();
}

void TransmitterUi::drawTxBatteryRow(int row)
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

void TransmitterUi::drawTxBatteryPage()
{
    if (portraitUi) { drawPortraitDetails(UiPage::TxBatterySetup); return; }
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
                                &TransmitterUi::controlTxBattery, row);
        } else {
            addPageActionTarget(343, 59 + row * 50, 106, 36,
                                &TransmitterUi::controlTxBattery, row);
        }
    }
    lv_obj_t *note = makeLabel(lv_scr_act(),
        "GPIO5 via 47k/10k divider", 0, 280, 12, cDim);
    lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 280);
    finishPageFocus();
}

void TransmitterUi::drawRxBatteryRow(int row)
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

void TransmitterUi::drawRxBatteryPage()
{
    if (portraitUi) { drawPortraitDetails(UiPage::RxBatterySetup); return; }
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
                                &TransmitterUi::controlRxBattery, row);
        } else {
            addPageActionTarget(353, 60 + row * 50, 106, 36,
                                &TransmitterUi::controlRxBattery, row);
        }
    }
    lv_obj_t *note = makeLabel(lv_scr_act(),
        "Voltage received from ER6G telemetry", 0, 280, 12, cDim);
    lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 280);
    finishPageFocus();
}

void TransmitterUi::drawCalibrationPage()
{
    startScreen();
    resetLvglObjects();
#ifdef JR_R1_UI_PORTRAIT
    makeHeader(calibratingSteering_ ? "STEERING CAL" : "THROTTLE CAL", "CANCEL");
#else
    makeHeader(calibratingSteering_
        ? "STEERING CALIBRATION" : "THROTTLE CALIBRATION", "CANCEL");
#endif
    beginPageFocus();
#ifdef JR_R1_UI_PORTRAIT
    {
    makeLabel(lv_scr_act(), calibratingSteering_
        ? "1. MOVE TO FULL LEFT" : "1. MOVE TO FULL BRAKE",
        16, 68, 15, cCyan);
    makeLabel(lv_scr_act(), calibratingSteering_
        ? "2. MOVE TO FULL RIGHT" : "2. MOVE TO FULL FORWARD",
        16, 102, 15, cCyan);
    makeLabel(lv_scr_act(), "3. RELEASE AND HOLD CENTRE", 16, 136, 15, cCyan);
    char held[44];
    snprintf(held, sizeof(held), "CH%u OUTPUT HELD AT NEUTRAL",
             calibratingSteering_ ? channelRoleSettings_.steeringChannel
                                  : channelRoleSettings_.throttleChannel);
    makeLabel(lv_scr_act(), held, 16, 178, 13, cAmber);
    lv_obj_t *panel = makePanel(10, 214, 300, 246);
    makeLabel(panel, "MINIMUM AVERAGE", 16, 18, 13, cDim);
    makeLabel(panel, "LIVE INPUT", 16, 86, 13, cDim);
    makeLabel(panel, "MAXIMUM AVERAGE", 16, 154, 13, cDim);
    calibrationMinimumLabel_ = makeLabel(panel, "----", 190, 10, 24);
    calibrationLiveLabel_ = makeLabel(panel, "----", 190, 78, 24);
    calibrationMaximumLabel_ = makeLabel(panel, "----", 190, 146, 24);
    makeLabel(panel, "Move to both ends,\nthen hold centre.",
              16, 202, 11, cDim);
    drawCalibrationValues(calibratingSteering_
        ? latestSteeringRawAdc_ : latestRawAdc_);
    finishPageFocus();
    return;
    }
#endif
    makeLabel(lv_scr_act(), calibratingSteering_
        ? "1. MOVE TO FULL LEFT" : "1. MOVE TO FULL BRAKE",
        20, 62, 16, cCyan);
    makeLabel(lv_scr_act(), calibratingSteering_
        ? "2. MOVE TO FULL RIGHT" : "2. MOVE TO FULL FORWARD",
        20, 92, 16, cCyan);
    makeLabel(lv_scr_act(), "3. RELEASE AND HOLD CENTRE", 20, 122, 16, cCyan);
    char held[44];
    snprintf(held, sizeof(held), "CH%u OUTPUT HELD AT NEUTRAL",
             calibratingSteering_ ? channelRoleSettings_.steeringChannel
                                  : channelRoleSettings_.throttleChannel);
    makeLabel(lv_scr_act(), held, 20, 160, 16, cAmber);
    lv_obj_t *panel = makePanel(18, 202, 444, 92);
    makeLabel(panel, "MIN AVG", 16, 10, 14, cDim);
    makeLabel(panel, "LIVE", 180, 10, 14, cDim);
    makeLabel(panel, "MAX AVG", 326, 10, 14, cDim);
    calibrationMinimumLabel_ = makeLabel(panel, "----", 10, 42, 24);
    calibrationLiveLabel_ = makeLabel(panel, "----", 168, 42, 24);
    calibrationMaximumLabel_ = makeLabel(panel, "----", 326, 42, 24);
    drawCalibrationValues(calibratingSteering_
        ? latestSteeringRawAdc_ : latestRawAdc_);
    finishPageFocus();
}

void TransmitterUi::drawCalibrationValues(uint16_t adc)
{
    (void)adc;
    ProportionalInput &input = calibratingSteering_
        ? steeringInput_ : triggerInput_;
    if (!input.calibrationActive() || !calibrationLiveLabel_) return;
    const ProportionalCalibrationProgress progress =
        input.calibrationProgress();
    lv_label_set_text_fmt(calibrationMinimumLabel_, "%4u", progress.minimum);
    lv_label_set_text_fmt(calibrationLiveLabel_, "%4u", progress.live);
    lv_label_set_text_fmt(calibrationMaximumLabel_, "%4u", progress.maximum);
    lv_obj_set_style_text_color(calibrationMinimumLabel_,
        colour(progress.sawMinimum ? cGreen : cWhite), 0);
    lv_obj_set_style_text_color(calibrationMaximumLabel_,
        colour(progress.sawMaximum ? cGreen : cWhite), 0);
}

void TransmitterUi::startCalibration(uint16_t adc, bool steering)
{
    calibratingSteering_ = steering;
    calibrationReturnPage_ = steering
        ? UiPage::ChannelPlaceholder : UiPage::Channel2;
    activePage_ = UiPage::Calibration;
    (steering ? steeringInput_ : triggerInput_).startCalibration(adc);
    drawCalibrationPage();
    Serial.printf("CAL: %s; move input to both ends, then release and hold neutral\n",
                  steering ? "steering" : "throttle");
}

void TransmitterUi::cancelCalibration()
{
    (calibratingSteering_ ? steeringInput_ : triggerInput_).cancelCalibration();
    if (calibrationReturnPage_ == UiPage::ChannelPlaceholder)
        drawChannelPlaceholder();
    else
        drawPage();
    Serial.println("CAL: cancelled; previous calibration retained");
}

void TransmitterUi::updateCalibration(uint16_t adc)
{
    ProportionalInput &input = calibratingSteering_
        ? steeringInput_ : triggerInput_;
    if (!input.updateCalibration(adc, millis())) return;
    const ProportionalCalibration &calibration = input.calibration();
    saveCalibration();
    Serial.printf("CAL: saved %s min=%u centre=%u max=%u\n",
                  calibratingSteering_ ? "steering" : "throttle",
                  calibration.minimum, calibration.centre, calibration.maximum);
    playBuzzer(180);
    if (calibrationReturnPage_ == UiPage::ChannelPlaceholder)
        drawChannelPlaceholder();
    else
        drawPage();
}

uint16_t TransmitterUi::readTouchRegister(uint8_t command)
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

bool TransmitterUi::readGt911Register(uint16_t reg, uint8_t *data, size_t length)
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

bool TransmitterUi::writeGt911Register(uint16_t reg, uint8_t value)
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

bool TransmitterUi::initialiseTouchController()
{
#ifdef JR_R1_SIMULATOR
    return true;
#else
#if DISPLAY_PROFILE == DISPLAY_PROFILE_WAVESHARE_RPI_35F
    // Goodix address-select/reset sequence used by Waveshare. Holding INT low
    // when reset is released selects address 0x5d.
#ifdef JR_R1_XIAO_PCB
    const bool resetReady = pcbResetTouch();
    bootDiagnosticsResult(BootCheck::TouchReset, resetReady);
    if (!resetReady) return false;
#else
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
#endif

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

void TransmitterUi::applyBacklight(uint8_t percent)
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

bool TransmitterUi::readTouch(int16_t &x, int16_t &y, uint16_t &rawX,
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

#ifdef JR_R1_UI_PORTRAIT
    // ST7796 rotation 2 is portrait rotated 180 degrees from the GT911's
    // native 320x480 coordinate orientation.
    x = constrain(static_cast<int32_t>(kScreenWidth - 1 - rawX),
                  0L, static_cast<int32_t>(kScreenWidth - 1));
    y = constrain(static_cast<int32_t>(kScreenHeight - 1 - rawY),
                  0L, static_cast<int32_t>(kScreenHeight - 1));
#else
    // Match the Waveshare example's rotation 3: landscape with the connector
    // orientation used by the ST7796 display profile.
    x = constrain(static_cast<int32_t>(kScreenWidth - 1 - rawY),
                  0L, static_cast<int32_t>(kScreenWidth - 1));
    y = constrain(static_cast<int32_t>(rawX),
                  0L, static_cast<int32_t>(kScreenHeight - 1));
#endif
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

void TransmitterUi::handleDashboardTouch(int16_t x, int16_t y)
{
#ifdef JR_R1_UI_PORTRAIT
    if (y < 44) {
        if (x >= 272) {
            playKeyClick(); drawMainMenu();
        } else if (lastSafetyStatus_ == OutputSafetyStatus::Armed) {
            armRequested_ = false;
            safetyResetRequested_ = true;
            playBuzzer(180);
            lastSafetyStatus_ = OutputSafetyStatus::ControlStale;
        } else if (lastTouchDurationMs_ >= kDashboardArmHoldMs) {
            armRequested_ = true;
            safetyResetRequested_ = true;
            playBuzzer(80);
            lastSafetyStatus_ = OutputSafetyStatus::ControlStale;
        }
    } else if (y >= 398) {
        if (x < 82) { playKeyClick(); drawMainMenu(); }
        else if (x < 160) { playKeyClick(); drawTelemetryPage(); }
        else {
            driveModesReturnPage_ = UiPage::Dashboard;
            playKeyClick(); drawDriveModesPage();
        }
    }
    return;
#endif
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

void TransmitterUi::handleTelemetryTouch(int16_t x, int16_t y)
{
    if (headerActionTouched(x, y)) {
        playKeyClick();
        drawDashboard();
    }
}

void TransmitterUi::handleMainMenuTouch(int16_t x, int16_t y)
{
#ifdef JR_R1_UI_PORTRAIT
    {
    if (x < 5 || x >= 315 || y < 52 || y >= 436) return;
    const int item = (y - 52) / 48;
    if (item < 0 || item >= 8 || !mainMenuItemEnabled(item)) return;
    mainMenuFocusIndex_ = static_cast<uint8_t>(item);
    activateMainMenuFocus();
    return;
    }
#endif
    if (x < 5 || x >= 475 || y < 52 || y >= 270) return;
    const int item = ((y - 52) / 54) * 2 + (x >= 240 ? 1 : 0);
    if (item < 0 || item >= 8 || !mainMenuItemEnabled(item)) return;
    mainMenuFocusIndex_ = static_cast<uint8_t>(item);
    activateMainMenuFocus();
}

void TransmitterUi::handleModelSelectTouch(int16_t x, int16_t y)
{
    if (headerActionTouched(x, y)) {
        playKeyClick();
        drawMainMenu();
        return;
    }
    if (y >= 52 && y < 257) {
        controlModels(static_cast<uint8_t>((y - 52) / 41),
                      PageControlEvent::Activate);
    } else if (y >= 270 && x < 80) {
        controlModels(10, PageControlEvent::Activate);
    } else if (y >= 270 && x >= 400) {
        controlModels(11, PageControlEvent::Activate);
    }
}

void TransmitterUi::handleChannelsTouch(int16_t x, int16_t y)
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

void TransmitterUi::handleChannelConfigTouch(int16_t x, int16_t y)
{
    if (headerActionTouched(x, y)) {
        playKeyClick(); drawChannelsPage(); return;
    }
    if (y >= 244 && y < 304) {
        if (selectedChannel_ == channelRoleSettings_.throttleChannel &&
            x >= 42 && x < 232) {
            playKeyClick(); drawPage(); return;
        }
        if (selectedChannel_ == channelRoleSettings_.steeringChannel &&
            x >= 42 && x < 232) {
            playKeyClick();
            startCalibration(latestSteeringRawAdc_, true);
            return;
        }
        if (channelHasProportionalControl(selectedChannel_) &&
            x >= 232 &&
            x < 438) {
            throttleResponseReturnPage_ = UiPage::ChannelPlaceholder;
            playKeyClick(); drawThrottleResponsePage(); return;
        }
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

void TransmitterUi::handleChannelRolesTouch(int16_t x, int16_t y)
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

void TransmitterUi::handleThrottleResponseTouch(int16_t x, int16_t y)
{
    if (headerActionTouched(x, y)) {
        playKeyClick();
        if (throttleResponseReturnPage_ == UiPage::Channel2) drawPage();
        else if (throttleResponseReturnPage_ == UiPage::Dashboard) drawDashboard();
        else if (throttleResponseReturnPage_ == UiPage::ChannelPlaceholder)
            drawChannelPlaceholder();
        else drawMainMenu();
        return;
    }
    if (y < 48 && x >= 250 && x < 340) { playKeyClick(); drawChannelSpeedPage(); return; }
    if (y < 52 || y >= 297 || x < 288) return;
    const int row = (y - 52) / 35;
    const int8_t direction = x < 424 ? -1 : 1;
    ChannelResponseProfile &profile = selectedResponseProfile();
    bool changed = false;
    if (row == 0) {
        const int value = profile.subtrimPercent +
                          direction * kResponseSubtrimStepPercent;
        profile.subtrimPercent = constrain(value, -20, 20);
        changed = true;
    } else if (row == 1) {
        const int value = profile.positiveEndpointPercent +
                          direction * kResponseEndpointStepPercent;
        profile.positiveEndpointPercent = constrain(value, 50, 100);
        changed = true;
    } else if (row == 2) {
        const int value = profile.positiveRatePercent + direction * kResponseRateStepPercent;
        profile.positiveRatePercent = constrain(value, 0, 100); changed = true;
    } else if (row == 3) {
        const int value = profile.positiveExpoPercent + direction * kResponseExpoStepPercent;
        profile.positiveExpoPercent = constrain(value, -100, 100); changed = true;
    } else if (row == 4) {
        const int value = profile.negativeEndpointPercent +
                          direction * kResponseEndpointStepPercent;
        profile.negativeEndpointPercent = constrain(value, 50, 100);
        changed = true;
    } else if (row == 5) {
        const int value = profile.negativeRatePercent + direction * kResponseRateStepPercent;
        profile.negativeRatePercent = constrain(value, 0, 100); changed = true;
    } else if (row == 6) {
        const int value = profile.negativeExpoPercent + direction * kResponseExpoStepPercent;
        profile.negativeExpoPercent = constrain(value, -100, 100); changed = true;
    }
    if (changed) { saveSettings(); playKeyClick(); drawThrottleResponsePage(); }
}

void TransmitterUi::handleChannelSpeedTouch(int16_t x, int16_t y)
{
    if (headerActionTouched(x, y)) { playKeyClick(); drawThrottleResponsePage(); return; }
    if (x < 340 || y < 78 || y >= 230) return;
    const uint8_t row = (y - 78) / 76;
    const int16_t direction = x < 400 ? -1 : 1;
    ChannelResponseProfile &profile = selectedResponseProfile();
    uint16_t &value = row == 0 ? profile.positiveRampMs : profile.negativeRampMs;
    const int32_t changed = static_cast<int32_t>(value) +
                            direction * kChannelSpeedStepMs;
    value = constrain(changed, 0L, static_cast<long>(kChannelSpeedMaximumMs));
    saveSettings();
    playKeyClick();
    drawChannelSpeedPage();
}

void TransmitterUi::handleControlAssignmentsTouch(int16_t x, int16_t y)
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

void TransmitterUi::handleBindingEditorTouch(int16_t x, int16_t y)
{
    if (headerActionTouched(x, y) || (y >= 270 && x < 150)) {
        playKeyClick(); drawControlAssignmentsPage(); return;
    }
    const PhysicalControlKind kind = physicalControlDescriptor(
        static_cast<size_t>(selectedBindingInput_)).kind;
    const bool parameter = bindingDraft_.target == AssignmentTarget::ModelParameter &&
        isChannelParameterTarget(bindingDraft_.targetId);
    if (y >= 85 && y < 118 && x >= 330) {
        bindingDraft_.enabled = !bindingDraft_.enabled;
    } else if (y >= 119 && y < 152 && x >= 330) {
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
    } else if (y >= 153 && y < 186 && x >= 330) {
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
    } else if (y >= 187 && y < 220 && x >= 330) {
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
    } else if (y >= 221 && y < 254 && x >= 330) {
        bindingDraft_.contexts = adjacentControlContext(
            bindingDraft_.contexts, x < 400 ? -1 : 1);
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

void TransmitterUi::handleInputMonitorTouch(int16_t x, int16_t y)
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

void TransmitterUi::handleDriveModesTouch(int16_t x, int16_t y)
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

void TransmitterUi::handleSetupTouch(int16_t x, int16_t y)
{
    if (y < 48 && x >= 254 && x < 340) {
        playKeyClick(); drawFirmwareInfoPage(); return;
    }
    if (headerActionTouched(x, y)) { playKeyClick(); drawMainMenu(); return; }
    if (y >= 265 && y < 315 && x < 102) { playBuzzer(500); return; }
    if (y >= 265 && y < 315 && x >= 102 && x < 220) {
        displaySetupBank_ = 0;
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

void TransmitterUi::appendElrsKeyboardCharacter(char character)
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

void TransmitterUi::queueElrsRequest(ElrsManagementAction action,
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

void TransmitterUi::handleElrsBindingTouch(int16_t x, int16_t y)
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

void TransmitterUi::handleDisplayTouch(int16_t x, int16_t y)
{
    if (headerActionTouched(x, y)) { playKeyClick(); drawSetupPage(); return; }
    if (y >= 270) {
        if (x < 80) controlDisplay(10, PageControlEvent::Activate);
        else if (x >= 400) controlDisplay(11, PageControlEvent::Activate);
        return;
    }
    if (y < 56 || y >= 256 || x < 340) return;
    const uint8_t visibleRow = (y - 56) / 50;
    const uint8_t row = displaySetupBank_ * 4 + visibleRow;
    if (row >= 5) return;
    const int8_t direction = x < 405 ? -1 : 1;
    if (row == 0) {
        static const uint16_t values[] = {0, 30, 60, 300};
        uint8_t index = 0;
        while (index < 4 && values[index] != systemSettings_.screenTimeoutSeconds) ++index;
        if (index >= 4) index = 2;
        index = static_cast<uint8_t>((index + (direction > 0 ? 1 : 3)) % 4);
        systemSettings_.screenTimeoutSeconds = values[index];
    } else if (row == 1) {
        systemSettings_.logoScreensaverEnabled =
            !systemSettings_.logoScreensaverEnabled;
    } else if (row == 2) {
        systemSettings_.displayBrightnessPercent = constrain(
            static_cast<int>(systemSettings_.displayBrightnessPercent) + direction * 10,
            20, 100);
        uiBrightnessPercent = systemSettings_.displayBrightnessPercent;
        applyBacklight(uiBrightnessPercent);
    } else if (row == 3) {
        playKeyClick();
        drawColourSchemePage();
        return;
    } else {
        playKeyClick();
        if (systemSettings_.logoScreensaverEnabled) {
            logoSaverReturnPage_ = UiPage::DisplaySetup;
            drawLogoSaver();
        } else {
            sleepDisplay();
        }
        return;
    }
    saveSystemSettings();
    playKeyClick();
    drawDisplayPage();
}

void TransmitterUi::handleTxBatteryTouch(int16_t x, int16_t y)
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

void TransmitterUi::handleRxBatteryTouch(int16_t x, int16_t y)
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

void TransmitterUi::handleTouch(int16_t x, int16_t y)
{
    if (activePage_ == UiPage::LogoSaver) {
        leaveLogoSaver();
        return;
    }
    if (activePage_ == UiPage::BootSplash ||
        activePage_ == UiPage::StartupWarning) return;
    if (designPage()) { designTouchDispatch_ = true; dispatchPageControlTouch(x, y); designTouchDispatch_ = false; return; }
    if (activePage_ != UiPage::Dashboard && headerHomeTouched(x, y)) {
        if (calibrationActive()) {
            (calibratingSteering_ ? steeringInput_ : triggerInput_)
                .cancelCalibration();
            armRequested_ = false;
            safetyResetRequested_ = true;
            Serial.println("CAL: cancelled by Home; previous calibration retained");
        }
        playKeyClick();
        drawDashboard();
        return;
    }
    if (calibrationActive()) {
        if (headerActionTouched(x, y)) cancelCalibration();
        return;
    }
    if (dispatchPageControlTouch(x, y)) return;
    if (portraitUi) return;
    if (activePage_ == UiPage::Dashboard) { handleDashboardTouch(x, y); return; }
    if (activePage_ == UiPage::RadioDiagnostics) { handleTelemetryTouch(x, y); return; }
    if (activePage_ == UiPage::MainMenu) { handleMainMenuTouch(x, y); return; }
    if (activePage_ == UiPage::ModelSelect) { handleModelSelectTouch(x, y); return; }
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
    if (activePage_ == UiPage::ElrsTxConfig) { handleElrsTxConfigTouch(x, y); return; }
    if (y < 48 && x >= 186 && x < 262) {
        selectedChannel_ = channelRoleSettings_.throttleChannel;
        throttleResponseReturnPage_ = UiPage::Channel2;
        playKeyClick(); drawThrottleResponsePage(); return;
    }
    if (y < 48 && x >= 262 && x < 340) { playKeyClick(); drawChannelsPage(); return; }
    if (headerActionTouched(x, y)) { playKeyClick(); startCalibration(latestRawAdc_); return; }
    if (y < 56 || y >= 256) return;
    const int row = (y - 56) / 50;
    bool changed = false;
    if (row < 3 && x >= 354 && x < 404) {
        if (row == 0 && throttleProfile().negativeEndpointPercent > 50) --throttleProfile().negativeEndpointPercent;
        if (row == 1 && throttleProfile().subtrimPercent > -20) --throttleProfile().subtrimPercent;
        if (row == 2 && throttleProfile().positiveEndpointPercent > 50) --throttleProfile().positiveEndpointPercent;
        changed = true;
    } else if (row < 3 && x >= 408 && x < 470) {
        if (row == 0 && throttleProfile().negativeEndpointPercent < 100) ++throttleProfile().negativeEndpointPercent;
        if (row == 1 && throttleProfile().subtrimPercent < 20) ++throttleProfile().subtrimPercent;
        if (row == 2 && throttleProfile().positiveEndpointPercent < 100) ++throttleProfile().positiveEndpointPercent;
        changed = true;
    } else if (row == 3 && x >= 340) { throttleProfile().reversed = !throttleProfile().reversed; changed = true; }
    if (changed) {
        playKeyClick();
        scheduleSettingsSave();
        refreshSettingRowValue(row);
    }
}

void TransmitterUi::updateTouch()
{
    int16_t x = 0, y = 0;
    uint16_t rawX = 0, rawY = 0, rawZ = 0;
    const uint32_t now = millis();
#ifdef JR_R1_SIMULATOR
    const bool irqActive = simulatorTouchDown();
#elif defined(JR_R1_XIAO_PCB)
    const bool irqActive = pcbTouchIrqActive();
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
            touchDownStartedMs_ = now;
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
    lastTouchDurationMs_ = now - touchDownStartedMs_;
    touchWasDown_ = false;
    touchReleaseSinceMs_ = 0;
    touchIgnoreUntilMs_ = now + kTouchRetriggerGuardMs;
    lastInteractionMs_ = now;
    Serial.printf("TOUCH up page=%s screen=(%d,%d)\n",
                  uiPageName(activePage_), releasedX, releasedY);
    handleTouch(releasedX, releasedY);
}

void TransmitterUi::update(uint16_t rawAdc, uint16_t steeringRawAdc,
                        int16_t rawInputPermille,
                        int16_t outputPermille,
                        uint16_t crsfValue, OutputSafetyStatus safetyStatus,
                        const CrsfTelemetryState &telemetry,
                        uint16_t txVoltageDecivolts, bool txVoltageValid,
                        const CrsfChannelOutputModel &channelOutputs,
                        bool channelOutputsLive)
{
    speedStatistics_.update(telemetry.groundSpeedDeciKmh, telemetry.gpsUpdatedMs, millis(),
                            telemetry.gpsValid && telemetry.satellites >= 4);
    designTelemetry_ = telemetry; designSafety_ = safetyStatus;
    designTxVoltage_ = txVoltageDecivolts; designTxValid_ = txVoltageValid;
    latestRawAdc_ = rawAdc;
    latestSteeringRawAdc_ = steeringRawAdc;
    updateSteeringEndpointSelection();
    for (uint8_t ch=1; ch<=16; ++ch) designRequested_[ch-1] = channelOutputs.requested(ch);
    const uint16_t enabledMask = channelOutputs.enabledMask();
    channelOutputs.resolve(channelOutputsLive, dashboardChannelValues_);
    if (enabledMask != dashboardEnabledChannelMask_) {
        dashboardEnabledChannelMask_ = enabledMask;
        if (activePage_ == UiPage::Dashboard) drawDashboard();
    }
    if (activePage_ == UiPage::ElrsTxConfig) {
        if (txConfigSequence_ != telemetry.txParameterSequence) {
            txConfigSequence_ = telemetry.txParameterSequence;
            txConfig_.receive(telemetry.txParameterType, telemetry.txParameterPayload,
                              telemetry.txParameterLength, millis());
        }
        // Share the existing bounded management queue. No second UART owner.
        // Wait for the control task to acknowledge CUT before any requests.
        if (!elrsRequestPending_ && safetyStatus == OutputSafetyStatus::Calibration) {
            TxParameterRequest request;
            if (txConfig_.poll(millis(), request)) {
                elrsPendingRequest_ = ElrsManagementRequest{};
                elrsPendingRequest_.action = ElrsManagementAction::TxParameter;
                elrsPendingRequest_.parameter = request;
                elrsRequestPending_ = true;
            }
        }
        if (txConfigRevision_ != txConfig_.revision()) {
            txConfigRevision_ = txConfig_.revision();
            if (!txConfigDraftDirty_ && txConfigSelected_) {
                const auto p = txConfig_.parameter(txConfigSelected_);
                if (p) txConfigDraft_ = p->value;
            }
            drawElrsTxConfigPage();
        }
    } else txConfig_.stop();
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
    if (calibrationActive()) updateCalibration(
        calibratingSteering_ ? steeringRawAdc : rawAdc);

    const uint32_t now = millis();
    if (activePage_ == UiPage::BootSplash &&
        now - bootSplashStartedMs_ >= kBootSplashDurationMs) {
        if (startupPreflightComplete_) drawDashboard();
        else drawStartupWarningPage();
    }
    if (activePage_ == UiPage::LogoSaver &&
        static_cast<int32_t>(now - nextLogoSaverMoveMs_) >= 0) {
        logoSaverPosition_ = static_cast<uint8_t>((logoSaverPosition_ + 1) % 4);
        positionLogoSaver();
        nextLogoSaverMoveMs_ = now + kLogoSaverMoveMs;
    }
    if (settingsSavePending_ &&
        static_cast<int32_t>(now - settingsSaveDueMs_) >= 0) {
        saveSettings();
    }
    if (channelParameterSavePending_ &&
        static_cast<int32_t>(now - channelParameterSaveDueMs_) >= 0) {
        saveChannelParameters();
    }
    if (!displaySleeping_ && activePage_ != UiPage::BootSplash &&
        activePage_ != UiPage::LogoSaver &&
        activePage_ != UiPage::StartupWarning &&
        systemSettings_.screenTimeoutSeconds != 0 &&
        now - lastInteractionMs_ >=
            static_cast<uint32_t>(systemSettings_.screenTimeoutSeconds) * 1000U) {
        if (systemSettings_.logoScreensaverEnabled) {
            logoSaverReturnPage_ = activePage_;
            drawLogoSaver();
        } else {
            sleepDisplay();
        }
    }
    if (static_cast<int32_t>(now - nextDisplayUpdateMs_) >= 0) {
        nextDisplayUpdateMs_ = now + kUiUpdateMs;
        if (calibrationActive()) drawCalibrationValues(
            calibratingSteering_ ? steeringRawAdc : rawAdc);
        else if (designPage()) {
            updateDesign(telemetry, safetyStatus, txVoltageDecivolts, txVoltageValid);
        }
        else if (activePage_ == UiPage::Dashboard) {
            drawDashboardLive(crsfValue, safetyStatus, telemetry,
                              txVoltageDecivolts, txVoltageValid);
        }
        else if (activePage_ == UiPage::RadioDiagnostics) {
            drawTelemetryLive(telemetry, txVoltageDecivolts, txVoltageValid);
        }
        else if (activePage_ == UiPage::Channel2) drawLiveIndicator(outputPermille, crsfValue);
        else if (activePage_ == UiPage::ThrottleResponse)
            refreshThrottleResponseGraph(rawInputPermille);
        else if (activePage_ == UiPage::InputMonitor) refreshInputMonitorRows();
    }
    lv_timer_handler();
}
