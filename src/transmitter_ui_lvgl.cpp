#include "channel_settings_repository.h"
#include "transmitter_ui.h"
#include "ui_refresh.h"
#include "model_image.h"
#include "setup_portal.h"
#include "offline_map.h"
#include "ui_pixel_cache.h"
#include "waveshare_touch_orientation.h"

#include <Preferences.h>
#include "boot_diagnostics.h"
#include <cmath>
#ifndef JR_R1_SIMULATOR
#include <Arduino_GFX_Library.h>
#include <esp_heap_caps.h>
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
LV_IMG_DECLARE(r1_evo_wordmark);

namespace {

#ifdef JR_R1_UI_PORTRAIT
int16_t kScreenWidth = DISPLAY_LANDSCAPE_HEIGHT;
int16_t kScreenHeight = DISPLAY_LANDSCAPE_WIDTH;
constexpr uint8_t kWaveshareDisplayRotation = 2;
#else
int16_t kScreenWidth = DISPLAY_LANDSCAPE_WIDTH;
int16_t kScreenHeight = DISPLAY_LANDSCAPE_HEIGHT;
constexpr uint8_t kWaveshareDisplayRotation = 1;
#endif
bool portraitUi = kScreenWidth < kScreenHeight;
// Separate cards share one LCD-pixel gutter in either orientation.
constexpr int kPanelGap = 6;
constexpr int kPanelMargin = 10;
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
constexpr PhysicalControlId kParameterControls[] = {
    PhysicalControlId::Count, PhysicalControlId::GripTrimA,
    PhysicalControlId::GripTrimB, PhysicalControlId::ThrottleTrim,
    PhysicalControlId::SteeringTrim, PhysicalControlId::Aux3Trim,
};
constexpr uint32_t kDisplayWakeArmMs = 250;
constexpr uint32_t kDisplayWakePollMs = 20;
constexpr uint32_t kSettingsSaveDelayMs = 750;
constexpr uint16_t kBindingProfileVersion = 1;
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
#if defined(JR_R1_XIAO_PCB) && DISPLAY_PROFILE == DISPLAY_PROFILE_WAVESHARE_RPI_35F
// Use the same SPIClass transport as the verified grounded-CS bench test.
SPIClass waveshareSpi(FSPI);
Arduino_DataBus *displayBus = new Arduino_HWSPI(
    TFT_DC_PIN, TFT_CS_PIN, TFT_SCK_PIN, TFT_MOSI_PIN, TFT_MISO_PIN,
    &waveshareSpi);
#else
Arduino_DataBus *displayBus = new Arduino_ESP32SPI(
    TFT_DC_PIN, TFT_CS_PIN, TFT_SCK_PIN, TFT_MOSI_PIN, TFT_MISO_PIN);
#endif
#if DISPLAY_PROFILE == DISPLAY_PROFILE_WAVESHARE_RPI_35F
#ifdef JR_R1_XIAO_PCB
// Verified bench initialization table, including 500 ms settling waits.
#include "waveshare_startup.inc"
// CS is permanently low on the production wiring. The SPI peripheral and
// pin mux must settle before releasing the LCD's external reset.
class PcbWaveshareDisplay : public Arduino_ST7796 {
public:
    using Arduino_ST7796::Arduino_ST7796;
    bool begin(int32_t speed = GFX_NOT_DEFINED) override {
        resetReady_ = false;
        if (!pcbControls.setOutput(PCB_LCD_RESET_PORT, false)) {
            bootDiagnosticsResult(BootCheck::LcdReset, false);
            return false;
        }
        // The base begin prepares mode-3 settings, then calls our tftInit hook.
        const bool ready = Arduino_ST7796::begin(speed);
        return ready && resetReady_;
    }
protected:
    void tftInit() override {
        bootDiagnosticsStage("LCD-reset");
        // HWSPI applies its mode on the first transaction, not in begin().
        // Settle that idle-clock transition while RESET is still asserted.
        _bus->beginWrite();
        _bus->endWrite();
        delay(500);
        resetReady_ = pcbControls.writeOutput(PCB_LCD_RESET_PORT, true);
        bootDiagnosticsResult(BootCheck::LcdReset, resetReady_);
        if (!resetReady_) return;
        delay(500);
        bootDiagnosticsStage("display-driver");
        _bus->sendCommand(ST7796_SWRESET);
        delay(500);
        _bus->batchOperation(waveshare_startup_operations,
                             sizeof(waveshare_startup_operations));
        invertDisplay(false);
        Serial.println("LCD: SPIClass mode 3 settled under reset; write-only startup, 500 ms waits");
    }
private:
    bool resetReady_ = false;
};
Arduino_GFX *display = new PcbWaveshareDisplay(
    displayBus, TFT_RESET_PIN, kWaveshareDisplayRotation, true, 320, 480);
#else
Arduino_GFX *display = new Arduino_ST7796(
    displayBus, TFT_RESET_PIN, kWaveshareDisplayRotation, true, 320, 480);
#endif
#else
Arduino_GFX *display = new Arduino_ILI9486_18bit(
    displayBus, GFX_NOT_DEFINED, 1, false);
#endif
SPIClass screenshotSpi(FSPI);

lv_disp_draw_buf_t drawBuffer;
lv_disp_drv_t displayDriver;
lv_color_t lineBuffer[DISPLAY_LANDSCAPE_WIDTH * 16];
uint8_t screenshotRow[DISPLAY_LANDSCAPE_WIDTH * 3];

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
lv_color_t lineBuffer[DISPLAY_LANDSCAPE_WIDTH * 16];
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

UiPixelCache<DISPLAY_LANDSCAPE_WIDTH, DISPLAY_LANDSCAPE_HEIGHT> pixelCache;
lv_disp_t *registeredDisplay=nullptr;
bool lcdTransportReady = false;
struct UiFrameMetrics {
    uint32_t rendered = 0, sent = 0, transfers = 0, transferUs = 0;
    uint32_t pageStartedUs = 0, buildUs = 0, touchAcceptedUs = 0;
    uint32_t flushUs = 0, layoutUs = 0, handlerUs = 0;
    bool pagePending = false, touchPending = false;
} uiMetrics;

void flushDisplay(lv_disp_drv_t *driver, const lv_area_t *area,
                  lv_color_t *pixels)
{
    const uint32_t flushStarted = micros();
    const int width = area->x2 - area->x1 + 1;
    const int height = area->y2 - area->y1 + 1;
    auto *rgb = reinterpret_cast<uint16_t *>(pixels);
    SetupPortal::captureScreen(area->x1,area->y1,width,height,rgb);
    uiMetrics.rendered += width * height;
    if (!lcdTransportReady) {
        lv_disp_flush_ready(driver);
        uiMetrics.flushUs += uint32_t(micros()) - flushStarted;
        return;
    }
    // Coalesce adjacent changed rows, keeping command overhead bounded by the
    // 16-row LVGL buffer. No second DMA buffer or internal-RAM frame is needed.
    int row = 0;
    while (row < height) {
        if (pixelCache.unchanged(area->x1, area->y1 + row, width, rgb + row * width)) {
            ++row;
            continue;
        }
        const int first = row++;
        if (!pixelCache.enabled()) row = height; // Original single-transfer path.
        while (row < height && !pixelCache.unchanged(
                area->x1, area->y1 + row, width, rgb + row * width)) ++row;
        const uint32_t started = micros();
#ifdef JR_R1_SIMULATOR
        lv_area_t changed = *area;
        changed.y1 += first;
        changed.y2 = area->y1 + row - 1;
        simulatorFlush(&changed, pixels + first * width);
#else
        display->draw16bitRGBBitmap(area->x1, area->y1 + first,
                                    rgb + first * width, width, row - first);
#endif
        uiMetrics.transferUs += static_cast<uint32_t>(micros()) - started;
        uiMetrics.sent += width * (row - first);
        ++uiMetrics.transfers;
        for (int i = first; pixelCache.enabled() && i < row; ++i)
            pixelCache.remember(area->x1, area->y1 + i, width, rgb + i * width);
    }
    lv_disp_flush_ready(driver);
    uiMetrics.flushUs += uint32_t(micros()) - flushStarted;
}

// Report once after a completed refresh, never print in the transfer callback.
void reportUiFrame()
{
    if (uiMetrics.pagePending || uiMetrics.touchPending) {
        const uint32_t finished = micros();
        Serial.printf("UI PERF: build=%luus rendered=%lu sent=%lu transfers=%lu SPI=%luus page=%luus touch-release=%luus cache=%s layout=%luus flush-overhead=%luus lvgl-other=%luus\n",
            (unsigned long)uiMetrics.buildUs, (unsigned long)uiMetrics.rendered,
            (unsigned long)uiMetrics.sent, (unsigned long)uiMetrics.transfers,
            (unsigned long)uiMetrics.transferUs,
            (unsigned long)(uiMetrics.pagePending ? finished - uiMetrics.pageStartedUs : 0),
            (unsigned long)(uiMetrics.touchPending ? finished - uiMetrics.touchAcceptedUs : 0),
            pixelCache.enabled() ? "on" : "off",
            (unsigned long)uiMetrics.layoutUs,
            (unsigned long)(uiMetrics.flushUs - uiMetrics.transferUs),
            (unsigned long)(uiMetrics.handlerUs >= uiMetrics.flushUs ? uiMetrics.handlerUs - uiMetrics.flushUs : 0));
    }
    uiMetrics = UiFrameMetrics{};
}


void position(lv_obj_t *object, int16_t x, int16_t y, int16_t width,
              int16_t height)
{
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
}

constexpr uint8_t kUiCornerRadius = 6;

lv_obj_t *makePanel(int16_t x, int16_t y, int16_t width, int16_t height,
                    uint32_t fill = cPanel, uint32_t border = cLine,
                    uint8_t radius = kUiCornerRadius)
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
                    UiText::Style textStyle = UiText::Secondary, uint32_t textColour = cWhite)
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
    lv_obj_set_style_text_font(label, UiText::font(textStyle), 0);
    lv_obj_set_style_text_color(label, colour(textColour), 0);
    return label;
}

lv_obj_t *makeButton(int16_t x, int16_t y, int16_t width, int16_t height,
                     const char *text, UiText::Style textStyle = UiText::Menu,
                     uint32_t fill = cPanel2, uint32_t textColour = cWhite,
                     uint32_t border = cLine)
{
    lv_obj_t *button = makePanel(x, y, width, height, fill, border, kUiCornerRadius);
    lv_obj_t *label = makeLabel(button, text, 0, 0, textStyle, textColour);
    lv_obj_center(label);
    return button;
}

// A crossed output arrow marks a control preview whose requested positions
// are being held by output safety. It does not imply the radio is silent.
lv_obj_t *makeHeldOutputIcon(lv_obj_t *parent)
{
    auto icon=lv_obj_create(parent);position(icon,0,0,18,16);
    lv_obj_set_style_bg_opa(icon,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(icon,0,0);lv_obj_set_style_pad_all(icon,0,0);
    lv_obj_clear_flag(icon,LV_OBJ_FLAG_CLICKABLE);
    static const lv_point_t arrow[]={{1,8},{15,8},{11,4},{15,8},{11,12}};
    static const lv_point_t slash[]={{4,14},{13,1}};
    auto line=[&](const lv_point_t *points,int count,uint32_t col,int width){
        auto object=lv_line_create(icon);lv_line_set_points(object,points,count);
        lv_obj_set_style_line_color(object,colour(col),0);lv_obj_set_style_line_width(object,width,0);
        lv_obj_set_style_line_rounded(object,true,0);lv_obj_clear_flag(object,LV_OBJ_FLAG_CLICKABLE);
    };
    line(arrow,5,cAmber,2);line(slash,2,cPanel,5);line(slash,2,cAmber,2);
    lv_obj_align(icon,LV_ALIGN_TOP_RIGHT,-6,4);return icon;
}

lv_obj_t *wifiStatusIcon=nullptr;
int wifiIconState=-1;
void updateWifiStatusIcon() {
    if(!wifiStatusIcon)return;
    const auto &s=SetupPortal::state();
    const int state=s.connected?3:s.connecting?2:s.hotspot?1:0;
    if(state==wifiIconState)return;wifiIconState=state;
    const uint32_t colours[]={0x71838a,0x59c8de,0xffc45c,0x8ed7a3};
    lv_obj_set_style_text_color(wifiStatusIcon,lv_color_hex(colours[state]),0);
    lv_obj_set_style_text_opa(wifiStatusIcon,state?LV_OPA_COVER:LV_OPA_50,0);
}
void makeWifiStatusIcon(int x,int y) {
    wifiStatusIcon=makeLabel(lv_scr_act(),LV_SYMBOL_WIFI,x,y,UiText::Menu,0x71838a);
    wifiIconState=-1;updateWifiStatusIcon();
}

void startScreen()
{
    wifiStatusIcon=nullptr;
    SetupPortal::screenChanged();
    const auto touchUs = uiMetrics.touchAcceptedUs;
    const bool touchPending = uiMetrics.touchPending;
    uiMetrics = UiFrameMetrics{};
    uiMetrics.touchAcceptedUs = touchUs;
    uiMetrics.touchPending = touchPending;
    uiMetrics.pageStartedUs = micros();
    uiMetrics.pagePending = true;
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), colour(cBlack), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
}


// Draw the lock directly: the compact UI fonts do not contain a lock glyph.
lv_obj_t *makeDriveIcon(lv_obj_t *parent, int x, int y)
{
    auto icon=lv_obj_create(parent);position(icon,x,y,12,16);
    lv_obj_set_style_pad_all(icon,0,0);
    lv_obj_set_style_bg_opa(icon,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(icon,0,0);
    auto shackle=lv_obj_create(icon);position(shackle,2,0,8,10);
    lv_obj_set_style_bg_opa(shackle,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(shackle,2,0);
    lv_obj_set_style_border_color(shackle,colour(cAmber),0);lv_obj_set_style_radius(shackle,4,0);
    auto body=lv_obj_create(icon);position(body,0,6,12,9);
    lv_obj_set_style_border_width(body,0,0);lv_obj_set_style_bg_color(body,colour(cAmber),0);
    lv_obj_set_style_radius(body,2,0);
    return icon;
}

void updateDriveIcon(lv_obj_t *icon, bool live, bool fault)
{
    if(!icon)return;
    auto shackle=lv_obj_get_child(icon,0);auto body=lv_obj_get_child(icon,1);
    if(live&&!lv_obj_has_flag(shackle,LV_OBJ_FLAG_HIDDEN))lv_obj_add_flag(shackle,LV_OBJ_FLAG_HIDDEN);
    else if(!live&&lv_obj_has_flag(shackle,LV_OBJ_FLAG_HIDDEN))lv_obj_clear_flag(shackle,LV_OBJ_FLAG_HIDDEN);
    if(lv_obj_get_style_border_color(shackle,0).full!=colour(fault?cRed:cAmber).full)
        lv_obj_set_style_border_color(shackle,colour(fault?cRed:cAmber),0);
    lv_obj_set_size(body,live?8:12,live?8:9);
    lv_obj_set_pos(body,live?2:0,6);
    if(lv_obj_get_style_radius(body,0)!=(live?LV_RADIUS_CIRCLE:2))
        lv_obj_set_style_radius(body,live?LV_RADIUS_CIRCLE:2,0);
    uiSetBgColor(body,colour(live?cGreen:fault?cRed:cAmber),0);
}


// Every detail screen has one escape in the same place. Section navigation
// is appended by finishPageFocus; modal editors retain Cancel/Apply instead.
bool standardDetailHeader = false;
void makeHeader(const char *title, const char *action = nullptr)
{
    standardDetailHeader = true;
    auto header = makePanel(0, 0, kScreenWidth, 44, cHeader, cHeader, 0);
    const bool cancel = action && (!strcmp(action,"CANCEL") || !strcmp(action,"Cancel"));
    const bool done = action && !strcmp(action,"Done");
    makeButton(8, 5, 76, 34, cancel ? "Cancel" : done ? "Done" : "Back", UiText::Secondary);
    auto titleLabel = makeLabel(header, title, 96, 9, UiText::Heading, 0xe7edef);
    lv_obj_set_width(titleLabel, kScreenWidth - 140);
    lv_label_set_long_mode(titleLabel, LV_LABEL_LONG_DOT);
    makeWifiStatusIcon(kScreenWidth - 33, 14);
}

bool headerActionTouched(int16_t x, int16_t y)
{
    return y >= 5 && y < 39 && x >= 8 && x < 84;
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
        case UiPage::ModuleLink:return "module-link";
        case UiPage::WifiSetup: return "wifi-setup";
        case UiPage::OutputStatus:return "output-status";
        case UiPage::ModeChooser:return "mode-chooser";
        case UiPage::ChannelInput:return "channel-input";
        case UiPage::ChannelResponseSetup:return "channel-response";
        case UiPage::ParameterControlPicker:return "parameter-control-picker";
        case UiPage::ResponseProfile:return "response-profile";
        case UiPage::MixingOverview:return "mixing-overview";
        case UiPage::NetworkOptions:return "network-options";
        case UiPage::SystemMenu:return "system";
        case UiPage::TimerControl:return "timer";
        case UiPage::QuickNavigation:return "quick-navigation";
        case UiPage::FieldResetConfirmation:return "reset-field";
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
        case UiPage::BootPin: return "boot-pin";
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
        case UiPage::ChannelMixing: return "channel-mixing";
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

void TransmitterUi::setDisplayCacheEnabled(bool enabled)
{
    pixelCache.setEnabled(enabled);
    uiMetrics = UiFrameMetrics{};
    Serial.printf("UI CACHE: %s (temporary; reboot restores on). Repeat the same navigation sequence.\n",
                  pixelCache.enabled() ? "on" : "off");
}


#include "transmitter_ui_design.inc"
#include "transmitter_ui_portrait.inc"
#ifdef JR_R1_SIMULATOR
#include "../simulator/ui_audit.inc"
#endif

bool TransmitterUi::begin()
{
    Preferences benchPreferences;
    if (benchPreferences.begin("r1-bench", true)) {
        headlessTesting_ = benchPreferences.getBool("headless", false);
        benchPreferences.end();
    }
    if (headlessTesting_) {
        enterHardwareRecovery();
        Serial.println("HEADLESS: LCD and touch skipped; remote UI available; normal arming inhibited");
    }
    loadPinSecurity();
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
#endif
    // Audible power-on acknowledgement precedes peripheral startup, so a
    // missing LCD or touch controller cannot hide it. Honour the sound setting.
    playBuzzer(180);
#ifndef JR_R1_SIMULATOR
    Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);
    Wire.setClock(CONTROL_I2C_FREQUENCY);
    Serial.printf("I2C: SDA=%d SCL=%d clock=%lu Hz\n", TOUCH_SDA_PIN, TOUCH_SCL_PIN,
                  static_cast<unsigned long>(CONTROL_I2C_FREQUENCY));
#ifdef JR_R1_XIAO_PCB
    // Both expanders must be configured before GT911 address selection or
    // Arduino_GFX startup. Keypad pull-ups are enabled here, before sampling.
    bootDiagnosticsStage("expanders");
    const bool pcbReady = pcbIoBegin([](uint8_t attempt, bool keypad, bool controls) {
        Serial.printf("PCB STARTUP attempt=%u/5 K1=%s K2=%s\n", attempt,
                      keypad ? "PASS" : "FAIL", controls ? "PASS" : "FAIL");
    });
    bootDiagnosticsResult(BootCheck::Keypad, pcbKeypad.online());
    bootDiagnosticsResult(BootCheck::Controls, pcbControls.online());
    Serial.printf("PCB IO: K1=0x20 %s K2=0x21 %s\n",
                  pcbKeypad.online() ? "ONLINE" : "OFFLINE",
                  pcbControls.online() ? "ONLINE" : "OFFLINE");
    if (pcbReady && !headlessTesting_) bootDiagnosticsStage("touch");
    const bool touchReady = !headlessTesting_ && pcbReady && initialiseTouchController();
    if (pcbReady && !headlessTesting_) bootDiagnosticsResult(BootCheck::Touch, touchReady);
#else
    const bool touchReady = !headlessTesting_ && initialiseTouchController();
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
    if (pcbReady && !headlessTesting_) bootDiagnosticsStage("display-driver");
    displayReady_ = !headlessTesting_ && pcbReady && display->begin(TFT_SPI_FREQUENCY);
    if (pcbReady && !headlessTesting_) bootDiagnosticsResult(BootCheck::Display, displayReady_);
#else
    displayReady_ = !headlessTesting_ && display->begin(TFT_SPI_FREQUENCY);
#endif
#else
    const bool touchReady = !headlessTesting_;
    displayReady_ = simulatorDisplayBegin(kScreenWidth, kScreenHeight);
    if (headlessTesting_) displayReady_ = false;
#endif
    lcdTransportReady = displayReady_;
    const bool hardwareReady = displayReady_ && touchReady;
    (void)hardwareReady; // Only XIAO requires every peripheral for live control.
    if (!displayReady_) {
        Serial.printf("DISPLAY: %s %s\n", DISPLAY_PROFILE_NAME, headlessTesting_ ? "skipped for headless testing" : "initialisation failed");
#if defined(JR_R1_XIAO_PCB) || defined(JR_R1_SIMULATOR)
        // LVGL can still render into the web mirror without a working LCD.
        displayReady_ = true;
#else
        return false;
#endif
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
#ifdef JR_R1_SIMULATOR
    static uint16_t cachedPixels[DISPLAY_LANDSCAPE_WIDTH * DISPLAY_LANDSCAPE_HEIGHT];
    pixelCache.attach(cachedPixels);
#elif defined(JR_R1_XIAO_PCB)
    pixelCache.attach(static_cast<uint16_t *>(ps_malloc(kScreenWidth * kScreenHeight * sizeof(uint16_t))));
    Serial.printf("DISPLAY: pixel cache %s (307200 bytes PSRAM)\n", pixelCache.enabled() ? "enabled" : "unavailable; normal transfers");
#endif
    lv_disp_draw_buf_init(&drawBuffer, lineBuffer, nullptr,
                          kScreenWidth * 16);
    lv_disp_drv_init(&displayDriver);
    displayDriver.hor_res = kScreenWidth;
    displayDriver.ver_res = kScreenHeight;
    displayDriver.flush_cb = flushDisplay;
    displayDriver.draw_buf = &drawBuffer;
    pixelCache.setDimensions(kScreenWidth,kScreenHeight);
    registeredDisplay=lv_disp_drv_register(&displayDriver);

    drawBootSplash();
    lastInteractionMs_ = millis();
    lv_refr_now(nullptr);
    Serial.printf("DISPLAY: %s, %dx%d, SPI=%lu Hz, touch=%s, buffer=%u rows\n",
                  DISPLAY_PROFILE_NAME, kScreenWidth, kScreenHeight,
                  static_cast<unsigned long>(TFT_SPI_FREQUENCY),
                  touchReady ? "ready" : "offline", 16U);
    if (!touchReady)
        Serial.println("DISPLAY: touch controller did not respond; LCD remains usable");
#if defined(JR_R1_XIAO_PCB) && !defined(JR_R1_SIMULATOR)
    bootDiagnosticsResult(BootCheck::Ui, true);
    return hardwareReady;
#else
    return true;
#endif
}

ChannelResponseProfile &TransmitterUi::profileForChannel(uint8_t channel)
{
    channel = constrain(channel, 1, static_cast<int>(CRSF_CHANNEL_COUNT));
    return channelModel_.responses[channel - 1];
}

const ChannelResponseProfile &TransmitterUi::profileForChannel(uint8_t channel) const
{
    channel = constrain(channel, 1, static_cast<int>(CRSF_CHANNEL_COUNT));
    return channelModel_.responses[channel - 1];
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
        if(displaySleeping_)wakeDisplay();
        requestConfigurationCut();
        if(!SetupPortal::beginUsbScreenshot()){
            output.print("TFTSHOT-ERROR insufficient screenshot memory\n");return false;
        }
        // Capture the actual LVGL flush pixels; no panel readback or MISO needed.
        lv_obj_invalidate(lv_scr_act());lv_refr_now(nullptr);
        return SetupPortal::writeUsbScreenshot(output);
    }
#endif
    if (displaySleeping_) wakeDisplay();

    // A capture blocks the UI/control loop while pixels cross USB. Keep the
    // independent CRSF task alive, but require an explicit re-arm afterwards.
    requestConfigurationCut();
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
    standardDetailHeader=false;
    sectionNavigationVisible_=false;detailContentCompacted_=false;
    steeringEndpointButtons_[0]=steeringEndpointButtons_[1]=editorStepButton_=nullptr;
    driveProgress_=driveHint_=driveIcon_=nullptr;
    wifiKeyPreview_ = nullptr;
    for (auto &view : widgetViews_) view = DashboardWidgetView{};
    for (auto &value : designValues_) value = nullptr;
    for (auto &value : designDetails_) value = nullptr;
    for (auto &value : designBars_) value = nullptr;
    designStatus_ = designLink_ = designCurve_ = designMarker_ = nullptr;
    armProgress_=nullptr;
    driveHoldOverlay_=driveHoldLabel_=driveHoldBar_=nullptr;
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
    calibrationStatusLabel_ = nullptr;
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
        if(index>=static_cast<size_t>(PhysicalControlId::KeyMode)&&index<static_cast<size_t>(PhysicalControlId::KeyMode)+8&&state.faulted)
            keypadPresses_[index-static_cast<size_t>(PhysicalControlId::KeyMode)]=KeypadPress{};
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
    uint32_t faultMask = 0;
    for (size_t index = 0; index < kPhysicalControlCount; ++index)
        if (physicalInputStates_[index].faulted && (unsafeMask & (uint32_t{1} << index)))
            faultMask |= uint32_t{1} << index;
    const bool changed = startupPreflightComplete_ != complete ||
                         startupWarningMask_ != unsafeMask || startupFaultMask_ != faultMask;
    startupFaultMask_ = faultMask;
    startupPreflightComplete_ = complete;
    startupWarningMask_ = unsafeMask;
    if (!bootPinUnlocked_ || pinPurpose_ != PinPurpose::None) return;
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
    requestConfigurationCut();
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
    for (uint8_t slot=0;slot<kModelMemoryCount;++slot) {
        modelImages_[slot]=slot==0?0:kModelImageNone;
        modelColours_[slot]=kModelOriginalColour;modelAccents_[slot]=modelBases_[slot]=kModelOriginalColour;
    }
    Preferences preferences;
    preferences.begin("jr-r1-models", true);
    if(preferences.getBytesLength("images-v1")==sizeof(modelImages_))
        preferences.getBytes("images-v1",modelImages_,sizeof(modelImages_));
    if(preferences.getBytesLength("colours-v1")==sizeof(modelColours_))
        preferences.getBytes("colours-v1",modelColours_,sizeof(modelColours_));
    if(preferences.getBytesLength("accents-v1")==sizeof(modelAccents_))preferences.getBytes("accents-v1",modelAccents_,sizeof(modelAccents_));
    if(preferences.getBytesLength("bases-v1")==sizeof(modelBases_))preferences.getBytes("bases-v1",modelBases_,sizeof(modelBases_));
    for(uint8_t slot=0;slot<kModelMemoryCount;++slot){
        if(modelImages_[slot]>=kModelImageCount)modelImages_[slot]=kModelImageNone;
        if(!validModelColour(modelAccents_[slot]))modelAccents_[slot]=kModelOriginalColour;
        if(!validModelColour(modelBases_[slot]))modelBases_[slot]=kModelOriginalColour;
        if(!validModelColour(modelColours_[slot]))modelColours_[slot]=kModelOriginalColour;
    }
    activeModelSlot_ = preferences.getUChar("active", 0);
    if (preferences.getBytesLength("names") == sizeof(modelNames_))
        preferences.getBytes("names", modelNames_, sizeof(modelNames_));
    memset(dashboardHidden_,0,sizeof(dashboardHidden_));
    if(preferences.getBytesLength("widget-hide-v1")==sizeof(dashboardHidden_))
        preferences.getBytes("widget-hide-v1",dashboardHidden_,sizeof(dashboardHidden_));
    for (auto &layout : dashboardLayouts_) layout = defaultDashboardLayout();
    if (preferences.getBytesLength("widgets-v4") == sizeof(dashboardLayouts_)) {
        preferences.getBytes("widgets-v4",dashboardLayouts_,sizeof(dashboardLayouts_));
        for(auto &layout:dashboardLayouts_)sanitizeDashboardLayout(layout);
    } else if (preferences.getBytesLength("widgets-v3") == sizeof(dashboardLayouts_)) {
        preferences.getBytes("widgets-v3", dashboardLayouts_, sizeof(dashboardLayouts_));
        for(auto &layout:dashboardLayouts_)for(auto &w:layout.cells)if(w.kind==DashboardWidgetKind::AllChannels)w.source=0;
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
    preferences.putBytes("images-v1",modelImages_,sizeof(modelImages_));
    preferences.putBytes("colours-v1",modelColours_,sizeof(modelColours_));
    preferences.putBytes("accents-v1",modelAccents_,sizeof(modelAccents_));
    preferences.putBytes("bases-v1",modelBases_,sizeof(modelBases_));
    preferences.putBytes("widgets-v4", dashboardLayouts_, sizeof(dashboardLayouts_));
    preferences.putBytes("widget-hide-v1",dashboardHidden_,sizeof(dashboardHidden_));
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

    requestConfigurationCut();
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
    modelStorageFault_ = false;
    loadMixing();
    for (ChannelResponseProfile &profile : channelModel_.responses)
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
    const auto record = ChannelSettingsRepository::loadResponses(activeModelSlot_, channelModel_);
    if (record == ChannelSettingsLoad::Invalid) {
        modelStorageFault_ = true;
        for (auto &profile : channelModel_.responses) profile = ChannelResponseProfile{};
        Serial.println("MODEL STORAGE: invalid channel record; live outputs inhibited");
    }

    preferences.begin("jr-r1-trigger", true);
    ProportionalCalibration calibration {
        preferences.getUShort("adc_min", legacyCalibration.minimum),
        preferences.getUShort("adc_ctr", legacyCalibration.centre),
        preferences.getUShort("adc_max", legacyCalibration.maximum),
    };
    if (preferences.getBytesLength("cal_v1") == sizeof(calibration)) {
        ProportionalCalibration saved;
        if (preferences.getBytes("cal_v1", &saved, sizeof(saved)) == sizeof(saved) && saved.valid())
            calibration = saved;
    }
    preferences.end();

    for (ChannelResponseProfile &profile : channelModel_.responses)
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
    if (preferences.getBytesLength("cal_v1") == sizeof(steeringCalibration)) {
        ProportionalCalibration saved;
        if (preferences.getBytes("cal_v1", &saved, sizeof(saved)) == sizeof(saved) && saved.valid())
            steeringCalibration = saved;
    }
    preferences.end();
    if (!steeringCalibration.valid()) {
        steeringCalibration = {
            STEERING_ADC_MIN, STEERING_ADC_CENTER, STEERING_ADC_MAX};
    }
    steeringInput_.setCalibration(steeringCalibration);
}

bool TransmitterUi::persistChannelModel()
{
    const bool ok = ChannelSettingsRepository::save(activeModelSlot_, channelModel_);
    modelStorageFault_ = !ok;
    if (!ok) {
        requestConfigurationCut();
        Serial.println("MODEL STORAGE: save failed; outputs inhibited");
    }
    return ok;
}

void TransmitterUi::saveSettings()
{
    settingsSavePending_ = !persistChannelModel();
    if (settingsSavePending_) settingsSaveDueMs_ = millis() + 1000;
}

void TransmitterUi::scheduleSettingsSave()
{
    requestConfigurationCut();
    settingsSavePending_ = true;
    settingsSaveDueMs_ = millis() + kSettingsSaveDelayMs;
}

bool TransmitterUi::setHeadlessTesting(bool enabled)
{
    // CUT before NVS work, even when persisting the setting fails.
    requestConfigurationCut();
    Preferences preferences;
    if (!preferences.begin("r1-bench", false)) return false;
    const bool saved = preferences.putBool("headless", enabled) > 0 &&
        preferences.getBool("headless", !enabled) == enabled;
    preferences.end();
    if (!saved) return false;
    if (enabled) {
        headlessTesting_ = true;
        enterHardwareRecovery();
        lcdTransportReady = false;
        pixelCache.invalidate();
    }
    // Never resume LCD/touch or clear the safety latch mid-session. OFF only
    // changes the next boot, where the full hardware preflight runs again.
    return true;
}

void TransmitterUi::requestConfigurationCut()
{
    armRequested_ = false;
    safetyResetRequested_ = true;
    remoteArmHold_.cancel();
    // Hardware veto is immediate, before any page construction or NVS work.
    if (safetyCutHandler_) safetyCutHandler_();
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

bool TransmitterUi::saveCalibration()
{
    const ProportionalCalibration &calibration = calibratingSteering_
        ? steeringInput_.calibration() : triggerInput_.calibration();
    Preferences preferences;
    if (!preferences.begin(calibratingSteering_
        ? "jr-r1-steering" : "jr-r1-trigger", false)) return false;
    // One atomic NVS blob avoids mixing old/new endpoints after a power loss.
    const bool written = preferences.putBytes("cal_v1", &calibration,
        sizeof(calibration)) == sizeof(calibration);
    ProportionalCalibration readback;
    const bool verified = written && preferences.getBytes("cal_v1", &readback,
        sizeof(readback)) == sizeof(readback) &&
        readback.minimum == calibration.minimum &&
        readback.centre == calibration.centre &&
        readback.maximum == calibration.maximum;
    preferences.end();
    return verified;
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
    // Walk is handset-owned: rebooting or changing models must not bypass it.
    driveModeSettings_.activeMode = walkRetained_ ? DriveMode::Walk : DriveMode::Normal;
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
    if (ChannelSettingsRepository::loadParameters(activeModelSlot_, channelModel_) == ChannelSettingsLoad::Invalid) {
        modelStorageFault_ = true;
        for (auto &values : channelModel_.parameters) values = ChannelParameterValues{};
    }
}

void TransmitterUi::saveChannelParameters()
{
    channelParameterSavePending_ = !persistChannelModel();
    if (channelParameterSavePending_) channelParameterSaveDueMs_ = millis() + 1000;
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
    constexpr auto &choices = kParameterControls;
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
    requestConfigurationCut();
    bindingCount_ = count;
    for (size_t index = 0; index < count; ++index) bindings_[index] = candidate[index];
    saveBindings();
    bindingsChanged_ = true;
    requestConfigurationCut();
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
        if(bindingDraft_.initial<bindingDraft_.minimum||bindingDraft_.initial>bindingDraft_.maximum){
            snprintf(bindingError_,sizeof(bindingError_),"Adjust saved trim to fit range first");return false;
        }
    }
    if(bindingDraft_.target==AssignmentTarget::CrsfChannel||bindingDraft_.target==AssignmentTarget::ModelParameter){
        const int limit=bindingDraft_.target==AssignmentTarget::ModelParameter?200:1000;
        if(bindingDraft_.minimum < -limit || bindingDraft_.maximum > limit || bindingDraft_.step<1 || bindingDraft_.step>2*limit){
            snprintf(bindingError_,sizeof(bindingError_),"Range or step outside allowed limits");return false;
        }
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
    requestConfigurationCut();
    bindingCount_ = count;
    for (size_t index = 0; index < count; ++index) bindings_[index] = candidate[index];
    saveBindings();
    bindingsChanged_ = true;
    requestConfigurationCut();
    return true;
}

bool TransmitterUi::removeSelectedBinding()
{
    const int existing = bindingIndexForInput(selectedBindingInput_);
    if (existing < 0) return false;
    for (size_t index = static_cast<size_t>(existing); index + 1 < bindingCount_;
         ++index)
        bindings_[index] = bindings_[index + 1];
    requestConfigurationCut();
    --bindingCount_;
    saveBindings();
    bindingsChanged_ = true;
    requestConfigurationCut();
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
    pixelCache.invalidate();
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
        case UiPage::WifiSetup: drawWifiPage(); break;
        case UiPage::BootSplash: drawBootSplash(); break;
        case UiPage::BootPin: drawBootPin(); break;
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
        case UiPage::ChannelMixing: drawChannelMixing(); break;
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
if(portraitUi) {
    lv_img_set_zoom(logo, 176);
    lv_obj_align(logo, LV_ALIGN_TOP_MID, 0, 150);
} else {
    lv_obj_align(logo, LV_ALIGN_TOP_MID, 0, 92);
}

    lv_obj_t *designation = lv_img_create(lv_scr_act());
    lv_img_set_src(designation, &r1_evo_wordmark);
if(portraitUi) {
    lv_obj_align(designation, LV_ALIGN_TOP_MID, 0, 252);
} else {
    lv_obj_align(designation, LV_ALIGN_TOP_MID, 0, 204);
}
}

// The same keypad handles boot unlock, fresh Walk-exit authorization and PIN change.
static bool validSecurityPin(const char *pin)
{
    if(!pin || strlen(pin)!=4)return false;
    for(int i=0;i<4;++i)if(pin[i]<'0'||pin[i]>'9')return false;
    return true;
}
void TransmitterUi::loadPinSecurity()
{
    Preferences p;
    if(!p.begin("jr-r1-security",true))return;
    String saved=p.getString("pin","1234");
    if(validSecurityPin(saved.c_str()))saved.toCharArray(securityPin_,sizeof(securityPin_));
    walkRetained_=p.getBool("walk",false);p.end();
}
bool TransmitterUi::verifyPin(const char *pin) const
{
    return validSecurityPin(pin)&&strcmp(pin,securityPin_)==0;
}
bool TransmitterUi::storePin(const char *pin)
{
    if(!validSecurityPin(pin))return false;
    Preferences p;if(!p.begin("jr-r1-security",false))return false;
    bool ok=p.putString("pin",pin)>0 && strcmp(p.getString("pin","").c_str(),pin)==0;p.end();
    if(ok)strcpy(securityPin_,pin);
    return ok;
}
bool TransmitterUi::changePin(const char *current,const char *next,const char *confirm)
{
    if(armRequested_||designSafety_==OutputSafetyStatus::Armed||!verifyPin(current)||
       !validSecurityPin(next)||!confirm||strcmp(next,confirm)!=0)return false;
    return storePin(next);
}
bool TransmitterUi::recoverPinOverUsb(const char *next)
{
    if(armRequested_||designSafety_==OutputSafetyStatus::Armed||!storePin(next))return false;
    requestConfigurationCut();
    bootPinDigits_[0]=newPin_[0]=0;bootPinError_=false;pinRecoveryHelp_=false;
    pinPurpose_=bootPinUnlocked_?PinPurpose::None:PinPurpose::Boot;
    if(bootPinUnlocked_)drawDashboard();else drawBootPin();
    return true;
}
void TransmitterUi::beginPin(PinPurpose purpose,UiPage returnPage)
{
    requestConfigurationCut();
    pinPurpose_=purpose;pinReturnPage_=returnPage;pinLastInputMs_=millis();pinRecoveryHelp_=false;
    bootPinDigits_[0]=newPin_[0]=0;bootPinError_=false;drawBootPin();
}
void TransmitterUi::updatePinTimeout(uint32_t now)
{
    if(pinPurpose_!=PinPurpose::WalkExit||pinRecoveryHelp_||uint32_t(now-pinLastInputMs_)<10000)return;
    pinPurpose_=PinPurpose::None;bootPinDigits_[0]=newPin_[0]=0;bootPinError_=false;
    requestConfigurationCut();lastInteractionMs_=now;drawDashboard();
}
bool TransmitterUi::requestDriveMode(DriveMode mode)
{
    if(!bootPinUnlocked_||pinPurpose_!=PinPurpose::None)return false;
    if(mode==driveModeSettings_.activeMode)return true;
    if(mode==DriveMode::Normal){beginPin(PinPurpose::WalkExit,activePage_);return false;}
    Preferences p;if(!p.begin("jr-r1-security",false))return false;
    bool saved=p.putBool("walk",true)>0;p.end();if(!saved)return false;
    walkRetained_=true;driveModeSettings_.activeMode=DriveMode::Walk;
    requestConfigurationCut();saveDriveModeSettings();return true;
}
void TransmitterUi::drawBootPin()
{
    if(!bootPinUnlocked_)pinPurpose_=PinPurpose::Boot;
    activePage_=UiPage::BootPin;armRequested_=false;
    startScreen();resetLvglObjects();beginPageFocus(false);
    if(pinRecoveryHelp_){
        makePageActionButton(8,5,76,34,"Back",UiText::Menu,cPanel,cWhite,cLine,&TransmitterUi::controlBootPin,14);
        makeLabel(lv_scr_act(),"PIN recovery",96,9,UiText::Heading,cWhite);
        const int top=portraitUi?88:62;
        auto steps=makeLabel(lv_scr_act(),
            "1. Connect the XIAO USB-C to a computer.\n"
            "2. Open its serial monitor at 115200 baud.\n"
            "3. Send this command, then press Enter:",12,top,UiText::Secondary,cWhite);
        lv_obj_set_width(steps,kScreenWidth-24);lv_label_set_long_mode(steps,LV_LABEL_LONG_WRAP);
        auto command=makeLabel(lv_scr_act(),"PINRESET 1234",12,portraitUi?218:150,UiText::Heading,cCyan);
        lv_obj_set_width(command,kScreenWidth-24);
        auto note=makeLabel(lv_scr_act(),
            "This sets the PIN to 1234.\nOutputs must be CUT. Walk mode is unchanged.\nReturn to PIN entry to continue.",
            12,portraitUi?268:194,UiText::Secondary,cDim);
        lv_obj_set_width(note,kScreenWidth-24);lv_label_set_long_mode(note,LV_LABEL_LONG_WRAP);
        finishPageFocus();return;
    }
    char masked[8]="_ _ _ _";for(size_t i=0;i<strlen(bootPinDigits_);++i)masked[i*2]='*';
    auto digits=makeLabel(lv_scr_act(),bootPinError_?"PIN incorrect, mismatch or save failed":masked,
        12,portraitUi?8:6,bootPinError_?UiText::Secondary:UiText::Heading,bootPinError_?cRed:cWhite);
    lv_obj_set_width(digits,kScreenWidth-24);lv_obj_set_style_text_align(digits,LV_TEXT_ALIGN_CENTER,0);
    const int width=kScreenWidth-24,left=12,top=portraitUi?44:38;
    const int height=portraitUi?86:54,gap=8,cell=(width-2*gap)/3;
    for(int i=0;i<12;++i){
        const char *label=i==9?"Clear":i==10?"0":i==11?(pinPurpose_==PinPurpose::Boot?"Unlock":"Continue"):nullptr;
        char digit[2]={static_cast<char>('1'+i),0};if(!label)label=digit;
        const uint8_t id=i==9?10:i==10?0:i==11?11:i+1;
        makePageActionButton(left+(i%3)*(cell+gap),top+(i/3)*(height+gap),cell,height,label,i<9||i==10?UiText::Reading:UiText::Menu,cPanel,cWhite,cLine,&TransmitterUi::controlBootPin,id);
    }
    if(pinPurpose_!=PinPurpose::Boot)
        makePageActionButton(12,portraitUi?428:282,80,30,"Cancel",UiText::Secondary,cPanel,cWhite,cLine,&TransmitterUi::controlBootPin,12);
    makePageActionButton((kScreenWidth-132)/2,portraitUi?428:282,132,30,
        "Forgot PIN?",UiText::Secondary,cBlack,cCyan,cBlack,&TransmitterUi::controlBootPin,13);
    finishPageFocus();
}
void TransmitterUi::controlBootPin(uint8_t item, PageControlEvent event)
{
    if(event!=PageControlEvent::Activate||pinPurpose_==PinPurpose::None)return;
    if(item>14)return;
    pinLastInputMs_=millis();
    if(item==13){pinRecoveryHelp_=true;drawBootPin();return;}
    if(item==14){pinRecoveryHelp_=false;drawBootPin();return;}
    if(pinRecoveryHelp_)return;
    if(item==12&&pinPurpose_!=PinPurpose::Boot){
        pinPurpose_=PinPurpose::None;bootPinDigits_[0]=newPin_[0]=0;bootPinError_=false;
        if(pinReturnPage_==UiPage::SystemMenu)drawDesignPage(pinReturnPage_);else drawDashboard();return;
    }
    if(item==10){bootPinDigits_[0]=0;bootPinError_=false;}
    else if(item<10){
        size_t n=strlen(bootPinDigits_);
        if(n<4){bootPinDigits_[n]='0'+item;bootPinDigits_[n+1]=0;}
        bootPinError_=false;
        const bool authenticating=pinPurpose_==PinPurpose::Boot||pinPurpose_==PinPurpose::WalkExit||pinPurpose_==PinPurpose::ChangeCurrent;
        if(authenticating&&strlen(bootPinDigits_)==4){controlBootPin(11,PageControlEvent::Activate);return;}
    }
    else if(item==11){
        bool ok=validSecurityPin(bootPinDigits_);
        if(pinPurpose_==PinPurpose::ChangeNew&&ok){strcpy(newPin_,bootPinDigits_);pinPurpose_=PinPurpose::ChangeConfirm;}
        else if(pinPurpose_==PinPurpose::ChangeConfirm){
            ok=ok&&strcmp(bootPinDigits_,newPin_)==0&&storePin(newPin_);
            if(ok){pinPurpose_=PinPurpose::None;newPin_[0]=bootPinDigits_[0]=0;drawDesignPage(UiPage::SystemMenu);return;}
        }else if(pinPurpose_!=PinPurpose::ChangeNew){
            ok=verifyPin(bootPinDigits_);
            if(ok&&pinPurpose_==PinPurpose::ChangeCurrent)pinPurpose_=PinPurpose::ChangeNew;
            else if(ok){
                if(pinPurpose_==PinPurpose::WalkExit){
                    Preferences p;ok=p.begin("jr-r1-security",false);
                    if(ok){ok=p.putBool("walk",false)>0;p.end();}
                    if(ok){walkRetained_=false;driveModeSettings_.activeMode=DriveMode::Normal;saveDriveModeSettings();}
                }
                if(ok){bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;bootPinDigits_[0]=0;bootPinError_=false;
                    requestConfigurationCut();lastInteractionMs_=millis();
                    if(startupPreflightComplete_||hardwareRecovery_)drawDashboard();else drawStartupWarningPage();return;}
            }
        }
        bootPinDigits_[0]=0;bootPinError_=!ok;if(!ok)playBuzzer(180);
    }
    drawBootPin();
}

void TransmitterUi::positionLogoSaver()
{
    if (!logoSaverLogo_ || !logoSaverDesignation_) return;
    static constexpr int16_t xOffsets[] = {0, -4, 4, 0};
    static constexpr int16_t portraitY[] = {-100, -32, 100, 32};
    static constexpr int16_t landscapeY[] = {-40, 20, 40, -20};
    const uint8_t position=logoSaverPosition_%4;
    const int y=portraitUi?portraitY[position]:landscapeY[position];
    lv_obj_align(logoSaverLogo_,LV_ALIGN_CENTER,xOffsets[position],y);
    lv_obj_align(logoSaverDesignation_,LV_ALIGN_CENTER,xOffsets[position],y);
    const bool designation=position%2;
    if(designation){
        lv_obj_add_flag(logoSaverLogo_,LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(logoSaverDesignation_,LV_OBJ_FLAG_HIDDEN);
    }else{
        lv_obj_clear_flag(logoSaverLogo_,LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(logoSaverDesignation_,LV_OBJ_FLAG_HIDDEN);
    }
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
if(portraitUi) {
    lv_img_set_zoom(logoSaverLogo_, 176);
}
    logoSaverDesignation_ = lv_img_create(lv_scr_act());
    lv_img_set_src(logoSaverDesignation_, &r1_evo_wordmark);
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
    channelModel_.copyTo(snapshot);
    snapshot.modelStorageValid = !modelStorageFault_;
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
    snapshot.armRequested = (!moduleLinkManaged_ || moduleLink_.enabled()) && !hardwareRecovery_ && !rfBenchMode_ && bootPinUnlocked_ && pinPurpose_ == PinPurpose::None && armRequested_ && !SetupPortal::active() && !SetupPortal::firmwareUpdating();
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
    const ChannelParameterValues &values = channelModel_.parameters[channel - 1];
    return kind == ChannelParameterKind::Subtrim
        ? values.subtrimPermille : values.trimPermille;
}

bool TransmitterUi::applyChannelParameterAction(uint16_t targetId, int16_t value)
{
    if(!bootPinUnlocked_){armRequested_=false;return false;}
    if (!isChannelParameterTarget(targetId)) return false;
    const uint8_t channel = channelParameterTargetChannel(targetId);
    const ChannelParameterKind kind = channelParameterTargetKind(targetId);
    if (channel < 1 || channel > 16 ||
        (kind != ChannelParameterKind::Trim &&
         kind != ChannelParameterKind::Subtrim)) return false;
    value = constrain(value, -kChannelParameterLimitPermille,
                      kChannelParameterLimitPermille);
    int16_t &stored = kind == ChannelParameterKind::Subtrim
        ? channelModel_.parameters[channel - 1].subtrimPermille
        : channelModel_.parameters[channel - 1].trimPermille;
    if (stored == value) return true;
    if (kind == ChannelParameterKind::Subtrim) requestConfigurationCut();
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
    if(!bootPinUnlocked_||pinPurpose_!=PinPurpose::None){
        armRequested_=false;
        if(activePage_!=UiPage::BootPin||value<=0)return false;
        const auto key=static_cast<TransmitterFunction>(targetId);
        if(key==TransmitterFunction::MenuPrevious)movePageFocus(-1);
        else if(key==TransmitterFunction::MenuNext)movePageFocus(1);
        else if(key==TransmitterFunction::MenuSelect)activatePageFocus();
        else return false;
        return true;
    }
    if(SetupPortal::active())return false;
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
    if(!bootPinUnlocked_){armRequested_=false;return false;}
    if (targetId >= static_cast<uint16_t>(DriveModeSelection::Count)) return false;
    const DriveModeSelection selection = static_cast<DriveModeSelection>(targetId);
    if (selection == DriveModeSelection::CutArm) {
        armRequested_ = value > 0 && pinPurpose_ == PinPurpose::None;
        safetyResetRequested_ = true;
    } else if (selection == DriveModeSelection::WalkNormal) {
        const DriveMode requested = value > 0 ? DriveMode::Normal : DriveMode::Walk;
        if(value<=0)walkSwitchExitRequested_=false;
        if(requested==DriveMode::Normal&&driveModeSettings_.activeMode==DriveMode::Walk){
            if(walkSwitchExitRequested_)return false;
            walkSwitchExitRequested_=true;
        }
        if(!requestDriveMode(requested))return false;
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
    makeLabel(header, "STARTUP SAFETY CHECK", 14, 12, portraitUi ? UiText::Menu : UiText::Heading, 0xe7edef);
    beginPageFocus(false);

    lv_obj_t *banner = makePanel(12, 54, kScreenWidth-24, portraitUi ? 90 : 58, cSafetyFault, cRed);
    makeLabel(banner, startupWarningMask_ == 0
                          ? "CHECKING CONTROLS..."
                          : startupFaultMask_ != 0
                              ? (portraitUi ? "CHECK INPUT\nCONNECTIONS" : "CHECK INPUT CONNECTIONS")
                              : (portraitUi ? "RETURN CONTROLS\nTO SAFE POSITION" : "RETURN CONTROLS TO SAFE POSITION"),
              18, 8, UiText::Menu, startupWarningMask_ == 0 ? cAmber : cRed);
    makeLabel(banner, "Output remains CUT", 18, portraitUi ? 66 : 33, UiText::Caption, cWhite);

    if (startupWarningMask_ == 0) {
        makeLabel(lv_scr_act(), "Waiting for stable input readings", portraitUi ? 14 : 72, portraitUi ? 176 : 154,
                  UiText::Menu, cDim);
    } else {
        uint8_t shown = 0;
        for (size_t index = 0; index < kPhysicalControlCount; ++index) {
            if ((startupWarningMask_ & (uint32_t{1} << index)) == 0) continue;
            const int16_t column = portraitUi ? 0 : shown >= 4 ? 1 : 0;
            const int16_t row = portraitUi ? shown : shown >= 4 ? shown - 4 : shown;
            const int16_t x = 18 + column * 230;
            const int16_t y = (portraitUi ? 153 : 126) + row * (portraitUi ? 31 : 38);
            lv_obj_t *panel = makePanel(x, y, portraitUi ? 284 : 214, portraitUi ? 28 : 31, cPanel2, cAmber);
            makeLabel(panel, physicalControlDescriptor(index).name,
                      8, 7, UiText::Caption, cWhite);
            makeLabel(panel, (startupFaultMask_ & (uint32_t{1} << index)) ? "FAULT" : "ACTIVE",
                      portraitUi ? 231 : 165, 8, UiText::Caption, cRed);
            ++shown;
            if (shown >= 8) break;
        }
    }
    const char *instructions = startupFaultMask_ != 0
        ? (portraitUi ? "Check power / input connections.\nContinues when readings are safe." : "Check power / input connections to continue")
        : (portraitUi ? "Release switches / centre trims\nto continue" : "Release switches / centre trims to continue");
    makeLabel(lv_scr_act(), instructions, portraitUi ? 14 : 65,
              portraitUi ? 421 : 286, UiText::Caption, cAmber);
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

    const int panelWidth=portraitUi?kScreenWidth-2*kPanelMargin:(kScreenWidth-2*kPanelMargin-kPanelGap)/2;
    const int top=44+kPanelGap, cardHeight=126;
    lv_obj_t *link = makePanel(kPanelMargin, top, panelWidth, cardHeight);
    makeLabel(link, "RADIO LINK", 10, 7, UiText::Caption, cCyan);
    telemetryLinkStateLabel_ = makeLabel(link, "NO DATA", 10, 27, UiText::Heading, cDim);
    telemetryLinkQualityLabel_ = makeLabel(link, "LQ   ---%", 10, 56, UiText::Menu, cDim);
    telemetryRssiLabel_ = makeLabel(link, "RSSI --- dBm", 10, 80, UiText::Menu, cDim);
    telemetrySnrLabel_ = makeLabel(link, "SNR  -- dB", 10, 101, UiText::Caption, cDim);

    lv_obj_t *power = makePanel(portraitUi?kPanelMargin:kPanelMargin+panelWidth+kPanelGap, portraitUi?top+cardHeight+kPanelGap:top, panelWidth, cardHeight);
    makeLabel(power, "POWER / RF", 10, 7, UiText::Caption, cCyan);
    telemetryTxVoltageLabel_ = makeLabel(power, "TX  --.- V", 10, 31, UiText::Menu, cDim);
    telemetryRxVoltageLabel_ = makeLabel(power, "RX  --.- V", 10, 59, UiText::Menu, cDim);
    telemetryRadioLabel_ = makeLabel(power, "ANT -   MODE -   PWR -", 10, 94, UiText::Caption, cDim);

    lv_obj_t *stream = makePanel(kPanelMargin, top+(portraitUi?2:1)*(cardHeight+kPanelGap), kScreenWidth-2*kPanelMargin, portraitUi ? 120 : 98);
    makeLabel(stream, "CRSF STREAM", 10, 7, UiText::Caption, cCyan);
    telemetryFramesLabel_ = makeLabel(stream, "FRAMES          0", 10, 31, UiText::Menu, cWhite);
    telemetryCrcLabel_ = makeLabel(stream, "CRC ERR         0", portraitUi ? 10 : 245, portraitUi ? 78 : 31, portraitUi ? UiText::Caption : UiText::Menu, cWhite);
    telemetryBytesLabel_ = makeLabel(stream, "BYTES           0", 10, 58, UiText::Menu, cWhite);
    telemetryAgeLabel_ = makeLabel(stream, "AGE          ----", portraitUi ? 150 : 245, portraitUi ? 78 : 58, portraitUi ? UiText::Caption : UiText::Menu, cDim);

    makeLabel(lv_scr_act(), "Live receiver telemetry via Ranger Nano", 12, portraitUi ? 446 : 293, UiText::Caption, cDim);
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
    assignmentChannelContext_ = 0;
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
    const char *text, UiText::Style textStyle, uint32_t fill,
    uint32_t textColour, uint32_t border,
    PageControlHandler handler, uint8_t argument)
{
    lv_obj_t *button = makeButton(x, y, width, height, text, textStyle,
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

bool TransmitterUi::navigationIsModal() const
{
    switch(activePage_) {
        case UiPage::BootSplash: case UiPage::BootPin: case UiPage::LogoSaver:
        case UiPage::StartupWarning: case UiPage::Calibration:
        case UiPage::QuickNavigation: case UiPage::FieldResetConfirmation:
        case UiPage::ValueEditor: case UiPage::WidgetEditor:
        case UiPage::ParameterControlPicker: case UiPage::MixingOverview:
        case UiPage::ChannelMixing: case UiPage::BindingEditor:
        case UiPage::ElrsBinding: case UiPage::ModuleLink: return true;
        case UiPage::WifiSetup: return wifiEdit_ != 0;
        case UiPage::ElrsTxConfig: return txConfigSelected_ != 0;
        default: return false;
    }
}

// Subsection navigation uses one presentation everywhere. These are tabs,
// separate from framed action buttons and the primary bottom navigation.
void TransmitterUi::drawPageTabs(const char *const *labels,const uint8_t *actions,
                                uint8_t count,uint8_t selected,
                                PageControlHandler handler,int16_t y)
{
    if(!count)return;
    constexpr int left=10,height=34;
    const int width=kScreenWidth-2*left;
    for(uint8_t i=0;i<count;++i){
        const int x=left+i*width/count,w=(i+1)*width/count-i*width/count;
        auto tab=makePageActionButton(x,y,w,height,labels[i],UiText::Menu,cBlack,
            i==selected?cCyan:cWhite,cBlack,handler,actions[i]);
        lv_obj_set_style_border_width(tab,0,0);lv_obj_set_style_radius(tab,0,0);
        if(i==selected){
            auto line=lv_obj_create(tab);position(line,0,height-3,w,3);
            lv_obj_set_style_bg_color(line,colour(cCyan),0);
            lv_obj_set_style_border_width(line,0,0);lv_obj_set_style_radius(line,0,0);
            lv_obj_clear_flag(line,LV_OBJ_FLAG_CLICKABLE);
        }
    }
}

void TransmitterUi::drawPageControls(uint8_t page,uint8_t count)
{
    const int y=kScreenHeight-(portraitUi?60:40)-34;
    const int left=(kScreenWidth-count*28)/2;
    auto background=makePanel(left-4,y+2,count*28+8,24,cWhite,cWhite,12);
    lv_obj_set_style_bg_opa(background,LV_OPA_10,0);
    lv_obj_set_style_border_width(background,0,0);
    lv_obj_set_style_shadow_width(background,0,0);
    lv_obj_clear_flag(background,LV_OBJ_FLAG_CLICKABLE);
    for(uint8_t i=0;i<count;++i){
        auto target=makePageActionButton(left+i*28,y,28,28,"",UiText::Caption,
            cBlack,cWhite,cBlack,&TransmitterUi::controlPageDot,i);
        lv_obj_set_style_border_width(target,0,0);
        lv_obj_set_style_bg_opa(target,LV_OPA_TRANSP,0);
        auto dot=makePanel(10,10,8,8,i==page?cCyan:cDim,i==page?cCyan:cDim,4);
        lv_obj_set_style_border_width(dot,0,0);
        lv_obj_set_style_bg_opa(dot,i==page?LV_OPA_COVER:LV_OPA_50,0);
        lv_obj_set_parent(dot,target);lv_obj_clear_flag(dot,LV_OBJ_FLAG_CLICKABLE);
    }
}

void TransmitterUi::controlPageDot(uint8_t page,PageControlEvent event)
{
    if(event!=PageControlEvent::Activate)return;
    if(activePage_==UiPage::Tune&&page<2){tuneBank_=page;drawDesignPage(activePage_);}
    else if(activePage_==UiPage::Telemetry&&page<(DashboardSensorCount+5)/6){sensorBank_=page;drawDesignPage(activePage_);}
    else if(activePage_==UiPage::ChannelMonitor&&page<4){designBank_=page;drawDesignPage(activePage_);}
    else if(activePage_==UiPage::MainMenu&&page<2){setupPage_=page;drawMainMenu();}
    else if(activePage_==UiPage::DisplaySetup&&page<2){displaySetupBank_=page;drawDisplayPage();}
    else if(activePage_==UiPage::InputMonitor&&page<(kPhysicalControlCount+4)/5){inputMonitorBank_=page;drawInputMonitorPage();}
}

bool TransmitterUi::handlePageSwipe(int16_t x,int16_t y,int16_t endX,int16_t endY,uint32_t duration)
{
    if(!bootPinUnlocked_||pinPurpose_!=PinPurpose::None)return false;
    uint8_t page=0,count=0;
    switch(activePage_){
        case UiPage::DisplaySetup:page=displaySetupBank_;count=2;break;
        case UiPage::Tune:page=tuneBank_;count=2;break;
        case UiPage::Telemetry:page=sensorBank_;count=(DashboardSensorCount+5)/6;break;
        case UiPage::ChannelMonitor:page=designBank_;count=4;break;
        case UiPage::MainMenu:page=setupPage_;count=2;break;
        case UiPage::InputMonitor:page=inputMonitorBank_;count=(kPhysicalControlCount+4)/5;break;
        default:return false;
    }
    const int dx=endX-x,dy=endY-y;
    if(abs(dx)<24&&abs(dy)<24)return false; // Ordinary tap.
    // Consume drags on paged screens even if they are too vertical/slow or
    // reach an end, so lifting a finger cannot activate the row underneath.
    const int bottom=kScreenHeight-(portraitUi?60:40);
    const int top=activePage_==UiPage::InputMonitor?44:80;
    if(duration>1200||abs(dx)<60||abs(dy)>35||abs(dx)<2*abs(dy)||
       y<top||endY<top||y>=bottom||endY>=bottom)return true;
    const int next=constrain(int(page)+(dx<0?1:-1),0,int(count)-1);
    if(next!=page)controlPageDot(next,PageControlEvent::Activate);
    return true;
}

void TransmitterUi::drawSectionNavigation(uint8_t section)
{
    sectionNavigationVisible_=true;
    const int y=kScreenHeight-(portraitUi?60:40);
    const char *names[]={"Drive","Tune","Data","Setup"};
    for(uint8_t i=0;i<4;++i){
        auto tab=makePageActionButton(i*kScreenWidth/4,y,kScreenWidth/4,40,
            names[i],UiText::Menu,i==section?cPanel2:cPanel,i==section?cCyan:cWhite,cLine,
            &TransmitterUi::controlSectionNavigation,i);
        lv_obj_set_style_radius(tab,0,0);
        if(i==0){
            designStatus_=lv_obj_get_child(tab,0);lv_obj_align(designStatus_,LV_ALIGN_CENTER,0,0);
            driveIcon_=makeDriveIcon(tab,0,0);lv_obj_align(driveIcon_,LV_ALIGN_LEFT_MID,4,0);
            driveProgress_=makeBar(tab,0,37,kScreenWidth/4,3,0,800);
        }
        if(i==section){auto mark=makePanel(0,0,kScreenWidth/4,i==0?2:5,cCyan,cCyan,0);
            lv_obj_set_parent(mark,tab);lv_obj_set_pos(mark,0,35);}
    }
    updateDriveIcon(driveIcon_,designSafety_==OutputSafetyStatus::Armed,
        hardwareRecovery_ || designSafety_==OutputSafetyStatus::InputFault ||
        designSafety_==OutputSafetyStatus::ConfigurationFault || designSafety_==OutputSafetyStatus::ControlStale);
}

void TransmitterUi::controlSectionNavigation(uint8_t section, PageControlEvent event)
{
    if(navigationIsModal())return;
    if(activePage_==UiPage::WifiSetup){
        requestConfigurationCut();
        wifiSetupPassword_[0]=0;wifiPassword_[0]=0;wifiEdit_=0;wifiSetupPasswordError_=false;
    }
    controlDesign(section,event);
}

// The older detail layouts occupied the full height. Fit their content and
// matching touch/keypad rectangles into the shared content area, keeping the
// header and footer fixed and using the same mapping for every detail screen.
static void fitDetailChildren(lv_obj_t *parent, int numerator, int denominator, bool root)
{
    for(uint32_t i=0;i<lv_obj_get_child_cnt(parent);++i){
        auto child=lv_obj_get_child(parent,i);
        const int x=lv_obj_get_x(child),y=lv_obj_get_y(child),h=lv_obj_get_height(child);
        if(root && y<44)continue;
        fitDetailChildren(child,numerator,denominator,false);
        lv_obj_set_align(child,LV_ALIGN_TOP_LEFT);
        lv_obj_set_pos(child,x,root?44+(y-44)*numerator/denominator:y*numerator/denominator);
        if(!lv_obj_check_type(child,&lv_label_class))lv_obj_set_height(child,max(1,h*numerator/denominator));
    }
}

void TransmitterUi::finishPageFocus()
{
    if(standardDetailHeader){
        if(!navigationIsModal()){
            lv_obj_update_layout(lv_scr_act());
            const int numerator=kScreenHeight-(portraitUi?60:40)-4-44;
            const int denominator=kScreenHeight-(portraitUi?20:0)-44;
            fitDetailChildren(lv_scr_act(),numerator,denominator,true);
            auto fit=[&](PageFocusTarget::Rectangle &r){if(r.height>0 && r.y>=44){
                const int bottom=44+(r.y+r.height-44)*numerator/denominator;
                r.y=44+(r.y-44)*numerator/denominator;r.height=bottom-r.y;}};
            for(uint8_t i=0;i<pageFocusCount_;++i){auto &t=pageFocusTargets_[i];fit(t.focus);fit(t.activate);fit(t.decrease);fit(t.increase);}
            detailContentCompacted_=true;
            if(activePage_==UiPage::InputMonitor)
                drawPageControls(inputMonitorBank_,(kPhysicalControlCount+4)/5);
            drawSectionNavigation(activePage_==UiPage::RadioDiagnostics || (activePage_==UiPage::InputMonitor && inputMonitorReturnPage_==UiPage::Telemetry)?2:3);
        }
        // Wi-Fi edits have explicit Done/Cancel behaviour; all other details
        // use the normal parent navigation (including keypad activation).
        addPageActionTarget(8,5,76,34,&TransmitterUi::controlNavigation,0);
        pageStandardHeaderPending_=false;
    }
    if(wifiStatusIcon&&!navigationIsModal())
        addPageActionTarget(kScreenWidth-40,0,40,44,&TransmitterUi::controlDesign,205);
    if (portraitUi && !(activePage_==UiPage::Gps && gpsFullScreen_)) {
        auto strip=makePanel(0,460,320,20,cHeader,cHeader,0);
        makeLabel(strip,"JR",8,3,UiText::Caption,cRed);
        makeLabel(strip,"R-1",29,3,UiText::Caption,0xe7edef);
        makeLabel(strip,"TOUCH CONTROL",111,4,UiText::Caption,0xa9bbc5);
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
    if (row == 2) {
        channelRoleSettings_.steeringEnabled = !channelRoleSettings_.steeringEnabled;
        channelRolesChanged_ = true;
        requestConfigurationCut();
        saveChannelRoleSettings();
        playKeyClick();
        drawChannelRolesPage();
        return;
    }
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
    if(row==5){requestDisplayRotation();return;}
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
    if(field>=8&&field<=11){bindingEditorBank_=field-8;playKeyClick();drawBindingEditorPage();return;}
    if(field>=12&&field<=17){
        if(bindingDraft_.target!=AssignmentTarget::CrsfChannel&&bindingDraft_.target!=AssignmentTarget::ModelParameter)return;
        const bool parameter=bindingDraft_.target==AssignmentTarget::ModelParameter;
        const int limit=parameter?200:1000,delta=event==PageControlEvent::Decrease?-10:10;
        auto bounded=[](int value,int low,int high){return static_cast<int16_t>(value<low?low:value>high?high:value);};
        if(field==12)bindingDraft_.minimum=bounded(bindingDraft_.minimum+delta,-limit,bindingDraft_.maximum);
        if(field==13)bindingDraft_.maximum=bounded(bindingDraft_.maximum+delta,bindingDraft_.minimum,limit);
        if(field==14)bindingDraft_.step=bounded(bindingDraft_.step+delta,1,2*limit);
        if(field==15&&!parameter)bindingDraft_.initial=bounded(bindingDraft_.initial+delta,bindingDraft_.minimum,bindingDraft_.maximum);
        if(field==16&&!parameter)bindingDraft_.failsafe=bounded(bindingDraft_.failsafe+delta,bindingDraft_.minimum,bindingDraft_.maximum);
        if(field==17)bindingDraft_.inverted=!bindingDraft_.inverted;
        bindingError_[0]='\0';playKeyClick();drawBindingEditorPage();return;
    }
    // Function and drive-mode behaviour is derived from the selected action.
    if(field==3&&bindingDraft_.target!=AssignmentTarget::CrsfChannel&&
       bindingDraft_.target!=AssignmentTarget::ModelParameter)return;
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
        inputMonitorReturnPage_=UiPage::ControlAssignments;inputMonitorBank_ = 0; playKeyClick(); drawInputMonitorPage();
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
    if (item >= 5) return;
    const int x[5]={portraitUi?295:452,portraitUi?43:62,portraitUi?121:178,portraitUi?199:294,portraitUi?277:414};
    const int y[5]={portraitUi?22:20,portraitUi?437:280,portraitUi?437:280,portraitUi?437:280,portraitUi?437:280};
    handleDashboardTouch(x[item], y[item]);
}

void TransmitterUi::controlChannelConfig(uint8_t item, PageControlEvent event)
{
    if(item==5) {
        mixingDraft_=channelModel_.mixing[selectedChannel_-1];mixingBank_=0;mixingSaveError_=false;
        drawChannelMixing();return;
    }
    if (item == 4) {
        assignmentChannelContext_ = selectedChannel_;
        assignmentBank_ = 0;
        if (channelHasProportionalControl(selectedChannel_)) {
            channelInputPage_=0;drawDesignPage(UiPage::ChannelInput); return;
        }
        for(size_t i=0;i<bindingCount_;++i) {
            if(bindings_[i].target==AssignmentTarget::CrsfChannel &&
               bindings_[i].crsfChannel==selectedChannel_) {
                selectedBindingInput_=bindings_[i].input;
                bindingDraft_=bindings_[i];bindingError_[0]='\0';bindingEditorBank_=0;
                assignmentBank_=static_cast<size_t>(selectedBindingInput_)/5;
                drawBindingEditorPage();return;
            }
        }
        drawControlAssignmentsPage();return;
    }
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
    if(activePage_==UiPage::ModuleLink){controlDesign(241,PageControlEvent::Activate);return;}
    if(activePage_==UiPage::QuickNavigation){drawDesignPage(keypadReturnPage_);return;}
    if(activePage_==UiPage::FieldResetConfirmation){drawDesignPage(UiPage::ValueEditor);return;}
    if(activePage_==UiPage::WifiSetup){
        requestConfigurationCut();
        if(wifiEdit_==4 || wifiEdit_==6){wifiSetupPassword_[0]=0;wifiSetupPasswordError_=false;wifiEdit_=0;drawWifiPage();}
        else if(wifiEdit_){controlWifi(13,PageControlEvent::Activate);}
        else{wifiPassword_[0]=0;wifiSetupPassword_[0]=0;drawDesignPage(UiPage::NetworkOptions);}
        return;
    }
    if (designPage()) {
        if(activePage_==UiPage::ModelSelect && modelAppearanceEditing_){modelAppearanceEditing_=false;drawModelSelectPage();return;}
        if(activePage_==UiPage::WidgetEditor && widgetInputsEditing_){widgetInputsEditing_=false;drawDesignPage(UiPage::WidgetEditor);return;}
        if(activePage_==UiPage::OutputStatus||activePage_==UiPage::ModeChooser||activePage_==UiPage::DriveModes||activePage_==UiPage::ChannelPlaceholder||activePage_==UiPage::ChannelInput||activePage_==UiPage::ChannelResponseSetup||activePage_==UiPage::ParameterControlPicker||activePage_==UiPage::ResponseProfile||activePage_==UiPage::MixingOverview||activePage_==UiPage::NetworkOptions||activePage_==UiPage::SystemMenu){controlDesign(209,PageControlEvent::Activate);return;}
        if (activePage_ == UiPage::ValueEditor) drawDesignPage(editorReturn_);
        else if (activePage_ == UiPage::WidgetEditor) drawDesignPage(UiPage::WidgetSelect);
        else if (activePage_ == UiPage::SensorDetail || activePage_ == UiPage::Gps || activePage_ == UiPage::ChannelMonitor) drawTelemetryPage();
        else if (activePage_ == UiPage::TxBatterySetup || activePage_ == UiPage::DisplaySetup || activePage_ == UiPage::WidgetSelect || activePage_ == UiPage::Channels || activePage_ == UiPage::ModelSelect || activePage_ == UiPage::BatteryMenu) drawMainMenu();
        else drawDashboard();
        return;
    }
    playKeyClick();
    switch (activePage_) {
        case UiPage::Tune: case UiPage::ValueEditor: case UiPage::Gps:
        case UiPage::ChannelMonitor: case UiPage::SensorDetail: case UiPage::WidgetSelect: case UiPage::WidgetEditor: case UiPage::BatteryMenu:
        case UiPage::RadioDiagnostics: drawTelemetryPage(); break;
        case UiPage::WifiSetup: return;
        case UiPage::BootSplash: break;
        case UiPage::BootPin: break;
        case UiPage::LogoSaver: leaveLogoSaver(); break;
        case UiPage::Dashboard: break;
        case UiPage::StartupWarning: break;
        case UiPage::Telemetry: drawDashboard(); break;
        case UiPage::MainMenu: drawDashboard(); break;
        case UiPage::ModelSelect: drawMainMenu(); break;
        case UiPage::Channels: drawMainMenu(); break;
        case UiPage::ChannelRoles: if(assignmentChannelContext_)drawChannelPlaceholder();else drawChannelsPage(); break;
        case UiPage::Channel2:
        case UiPage::ChannelPlaceholder: drawChannelsPage(); break;
        case UiPage::ThrottleResponse:
            if (throttleResponseReturnPage_ == UiPage::Channel2) drawPage();
            else if (throttleResponseReturnPage_ == UiPage::Dashboard) drawDashboard();
            else if (throttleResponseReturnPage_ == UiPage::ChannelPlaceholder)
                drawChannelPlaceholder();
            else if(throttleResponseReturnPage_==UiPage::ChannelResponseSetup)drawDesignPage(UiPage::ChannelResponseSetup);
            else drawMainMenu();
            break;
        case UiPage::ChannelMixing: drawDesignPage(UiPage::MixingOverview); break;
        case UiPage::ChannelSpeed: drawThrottleResponsePage(); break;
        case UiPage::ControlAssignments: if(assignmentChannelContext_)drawChannelPlaceholder();else drawMainMenu(); break;
        case UiPage::BindingEditor: if(bindingEditorBank_){bindingEditorBank_=0;drawBindingEditorPage();}else drawControlAssignmentsPage(); break;
        case UiPage::InputMonitor: activePage_=inputMonitorReturnPage_;redrawActivePage();break;
        case UiPage::DriveModes:
            if (driveModesReturnPage_ == UiPage::Dashboard) drawDashboard();
            else drawMainMenu();
            break;
        case UiPage::Setup: drawMainMenu(); break;
        case UiPage::TxBatterySetup:
        case UiPage::RxBatterySetup:
        case UiPage::DisplaySetup: drawMainMenu(); break;
        case UiPage::ColourScheme: drawDisplayPage(); break;
        case UiPage::FirmwareInfo: drawDesignPage(UiPage::SystemMenu); break;
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
    lv_obj_t *panel = makePanel(18, y, 444, 68-kPanelGap);
    makeLabel(panel, row == 0 ? "STEERING" : "THROTTLE", 14, 16, UiText::Menu, cCyan);
    const uint8_t channel = row == 0 ? channelRoleSettings_.steeringChannel
                                     : channelRoleSettings_.throttleChannel;
    char value[12];
    snprintf(value, sizeof(value), "CH%u", channel);
    makeLabel(panel, value, 242, 16, UiText::Menu, cWhite);
    makeButton(350, y + 8, 44, 40, "-", UiText::Heading);
    makeButton(404, y + 8, 44, 40, "+", UiText::Heading);
}

void TransmitterUi::drawControlAssignmentsPage()
{
    activePage_ = UiPage::ControlAssignments;
    startScreen();
    resetLvglObjects();
    char heading[32];
    if(assignmentChannelContext_)snprintf(heading,sizeof(heading),"CH%u - SELECT CONTROL",assignmentChannelContext_);
    else snprintf(heading,sizeof(heading),"ASSIGNMENTS");
    makeHeader(heading, "BACK");
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
        lv_obj_t *panel = makePanel(10, y, kScreenWidth-20, (portraitUi ? 65 : 40)-kPanelGap);
        auto nameLabel=makeLabel(panel,control.name,8,portraitUi?5:(40-kPanelGap-UiText::font(UiText::Menu)->line_height)/2,UiText::Menu,cWhite);
        boundedLabel(nameLabel,portraitUi?kScreenWidth-48:190);
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
        makeLabel(panel, summary, portraitUi ? 8 : 215, portraitUi ? 29 : 5, UiText::Caption, summaryColour);
        makeLabel(panel, ">", kScreenWidth-42, 4, UiText::Menu, cDim);
        addPageActionTarget(10, y, kScreenWidth-20, portraitUi ? 58 : 35,
                            &TransmitterUi::controlAssignments, row);
    }

    makeButton(10, portraitUi ? 410 : 263, 50, 42, "<", UiText::Heading,
               assignmentBank_ == 0 ? cBlack : cPanel,
               assignmentBank_ == 0 ? cDim : cWhite);
    addPageActionTarget(10, portraitUi ? 410 : 263, 50, 42,
                        &TransmitterUi::controlAssignments, 5);
    makeButton(portraitUi ? 66 : 70, portraitUi ? 410 : 263, portraitUi ? 80 : 94, 42, "CHANNELS", UiText::Secondary, cPanel2, cCyan);
    addPageActionTarget(portraitUi ? 66 : 70, portraitUi ? 410 : 263, portraitUi ? 80 : 94, 42,
                        &TransmitterUi::controlAssignments, 6);
    makeButton(portraitUi ? 152 : 174, portraitUi ? 410 : 263, portraitUi ? 102 : 132, 42, "INPUT MON", UiText::Secondary, cPanel2, cCyan);
    addPageActionTarget(portraitUi ? 152 : 174, portraitUi ? 410 : 263, portraitUi ? 102 : 132, 42,
                        &TransmitterUi::controlAssignments, 7);
    char page[12];
    snprintf(page, sizeof(page), "%u/%u", assignmentBank_ + 1, kBanks);
    makeLabel(lv_scr_act(), page, portraitUi ? 140 : 329, portraitUi ? 396 : 276, UiText::Caption, cDim);
    makeButton(portraitUi ? 260 : 420, portraitUi ? 410 : 263, 50, 42, ">", UiText::Heading,
               assignmentBank_ + 1 >= kBanks ? cBlack : cPanel,
               assignmentBank_ + 1 >= kBanks ? cDim : cWhite);
    addPageActionTarget(portraitUi ? 260 : 420, portraitUi ? 410 : 263, 50, 42,
                        &TransmitterUi::controlAssignments, 8);
    finishPageFocus();
}

void TransmitterUi::drawBindingEditorPage()
{
    activePage_=UiPage::BindingEditor;startScreen();resetLvglObjects();
    makeHeader("Assignment", "Back");beginPageFocus();
    const auto &control=physicalControlDescriptor(static_cast<size_t>(selectedBindingInput_));
    const bool parameter=bindingDraft_.target==AssignmentTarget::ModelParameter&&isChannelParameterTarget(bindingDraft_.targetId);
    const bool editableMode=parameter||bindingDraft_.target==AssignmentTarget::CrsfChannel;
    auto name=makePanel(12,48,kScreenWidth-24,portraitUi?52:36);
    boundedLabel(makeLabel(name,control.name,10,6,UiText::Secondary,cCyan),portraitUi?kScreenWidth-44:280);
    makeLabel(name,r1InputEndpointLabel(selectedBindingInput_),portraitUi?10:310,portraitUi?30:9,UiText::Caption,cDim);
    const int tabsY=portraitUi?106:90;
    const uint8_t banks=editableMode?4:2;
    if(bindingEditorBank_>=banks)bindingEditorBank_=0;
    const char *tabs[]={"Assign","Mode","Range","State"};
    const uint8_t actions[]={8,9,10,11};
    drawPageTabs(tabs,actions,banks,bindingEditorBank_,&TransmitterUi::controlBindingEditor,tabsY);
    auto field=[&](int row,uint8_t id,const char *title,const char *value){
        const int y=(portraitUi?150:126)+row*(portraitUi?64:44),h=(portraitUi?64:44)-kPanelGap;
        auto panel=makePanel(12,y,kScreenWidth-24,h);
        boundedLabel(makeLabel(panel,title,10,4,UiText::Caption,cDim),kScreenWidth-156);
        boundedLabel(makeLabel(panel,value,10,portraitUi?27:20,UiText::Secondary,cWhite),kScreenWidth-156);
        if(id==0){
            makePageActionButton(kScreenWidth-118,y+4,94,h-8,value,UiText::Secondary,bindingDraft_.enabled?cGreen:cPanel2,
                bindingDraft_.enabled?cBlack:cWhite,cLine,&TransmitterUi::controlBindingEditor,0);
        }else if(id!=255){
            const int x=kScreenWidth-118,bw=44;
            makeButton(x,y+4,bw,h-8,"<",UiText::Menu);makeButton(x+50,y+4,bw,h-8,">",UiText::Menu);
            addPageEditorTarget(12,y,kScreenWidth-24,h,x,y+4,bw,h-8,x+50,y+4,bw,h-8,
                &TransmitterUi::controlBindingEditor,id);
        }
    };
    if(!bindingEditorBank_){
        field(0,0,"Enabled",bindingDraft_.enabled?"On":"Off");
        field(1,1,"Target",assignmentTargetName(bindingDraft_.target));
        char detail[48];const char *title="Output channel";
        if(bindingDraft_.target==AssignmentTarget::TransmitterFunction){title="Function";snprintf(detail,sizeof(detail),"%s",transmitterFunctionName(static_cast<TransmitterFunction>(bindingDraft_.targetId)));}
        else if(bindingDraft_.target==AssignmentTarget::DriveMode){title="Drive mode";snprintf(detail,sizeof(detail),"%s",driveModeSelectionName(static_cast<DriveModeSelection>(bindingDraft_.targetId)));}
        else snprintf(detail,sizeof(detail),"CH%u",parameter?channelParameterTargetChannel(bindingDraft_.targetId):bindingDraft_.crsfChannel);
        field(2,2,title,detail);
    }else if(bindingEditorBank_==1){
        int row=0;
        if(editableMode)field(row++,3,parameter?"Parameter":"Switch / button mode",parameter?
            channelParameterKindName(channelParameterTargetKind(bindingDraft_.targetId)):controlBehaviourName(bindingDraft_.behaviour));
        field(row++,4,"Active when",controlContextName(bindingDraft_.contexts));
        const char *help="";
        switch(bindingDraft_.contexts){
            case ContextAlways:help="Available in menus and while driving.";break;
            case ContextDashboard:help="Available on the Drive screen.";break;
            case ContextMenu:help="Available while browsing menus.";break;
            case ContextArmed:help="Available only while outputs are LIVE.";break;
            case ContextCut:help="Available only while outputs are CUT.";break;
            default:help="Uses the selected context combination.";break;
        }
        auto caption=makeLabel(lv_scr_act(),help,14,(portraitUi?150:126)+row*(portraitUi?64:44)+4,UiText::Caption,cDim);
        lv_obj_set_width(caption,kScreenWidth-28);lv_label_set_long_mode(caption,LV_LABEL_LONG_WRAP);
        if(!editableMode)makeLabel(lv_scr_act(),"Mode follows the chosen function / switch.",14,portraitUi?290:216,UiText::Caption,cDim);
    }
    if(bindingEditorBank_>=2){
        auto percent=[&](int row,uint8_t id,const char *title,int value){char text[32];snprintf(text,sizeof(text),"%+.1f%%",value/10.0);field(row,id,title,text);};
        if(bindingEditorBank_==2){
            percent(0,12,"Minimum / switch low",bindingDraft_.minimum);
            percent(1,13,"Maximum / switch high",bindingDraft_.maximum);
            percent(2,14,"Step (stepped controls)",bindingDraft_.step);
        }else{
            if(parameter){
                field(0,255,"Start / fault","Uses saved trim");
                field(1,255,"Trim outside range?","Adjust trim before Apply");
            }else{
                percent(0,15,"Starting value",bindingDraft_.initial);
                percent(1,16,"Input fault value",bindingDraft_.failsafe);
            }
            field(2,17,"Reverse control",bindingDraft_.inverted?"On":"Off");
        }
    }
    boundedLabel(makeLabel(lv_scr_act(),bindingError_[0]?bindingError_:"Draft only. Apply forces CUT; re-enable at neutral.",
        14,portraitUi?392:259,UiText::Caption,bindingError_[0]?cRed:cAmber),kScreenWidth-28);
    const int bottom=portraitUi?416:274,h=portraitUi?40:34;
    makePageActionButton(12,bottom,portraitUi?92:138,h,"Cancel",UiText::Secondary,cPanel,cWhite,cLine,&TransmitterUi::controlBindingEditor,5);
    if(bindingIndexForInput(selectedBindingInput_)>=0)
        makePageActionButton(portraitUi?110:160,bottom,portraitUi?92:138,h,"Remove",UiText::Secondary,cPanel,cAmber,cLine,&TransmitterUi::controlBindingEditor,6);
    makePageActionButton(portraitUi?208:308,bottom,portraitUi?100:160,h,"Apply",UiText::Secondary,cGreen,cBlack,cLine,&TransmitterUi::controlBindingEditor,7);
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
    const size_t first = inputMonitorBank_ * kRowsPerBank;
    for (uint8_t row = 0; row < kRowsPerBank; ++row) {
        const size_t index = first + row;
        if (index >= kPhysicalControlCount) break;
        const PhysicalControlDescriptor &control = physicalControlDescriptor(index);
        const int16_t y = 54 + row * (portraitUi ? 67 : 43);
        lv_obj_t *panel = makePanel(10, y, kScreenWidth-20, (portraitUi ? 67 : 43)-kPanelGap);
        auto nameLabel=makeLabel(panel,control.name,8,portraitUi?5:(40-kPanelGap-UiText::font(UiText::Menu)->line_height)/2,UiText::Menu,cWhite);
        boundedLabel(nameLabel,portraitUi?kScreenWidth-48:190);
        makeLabel(panel, r1InputEndpointLabel(control.id), portraitUi ? 8 : 200, portraitUi ? 33 : 6, UiText::Caption, cDim);
        inputMonitorStateLabels_[row] = makeLabel(panel, "--", portraitUi ? 171 : 285, 5, UiText::Caption, cWhite);
        inputMonitorEventLabels_[row] = makeLabel(panel, "--", portraitUi ? 171 : 370, portraitUi ? 33 : 6, UiText::Caption, cDim);
    }

    refreshInputMonitorRows();
    makeLabel(lv_scr_act(),"Read-only: operate controls on the handset",12,portraitUi?384:264,UiText::Caption,cDim);
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
    makePageActionButton(18, 204, 444, 42,
        channelRoleSettings_.steeringEnabled ? "Steering input: ON" : "Steering input: OFF (neutral)",
        UiText::Secondary, cPanel2, cCyan, cLine, &TransmitterUi::controlChannelRoles, 2);
    lv_obj_t *note = makeLabel(lv_scr_act(),
        "Changes force CUT. Disabled steering holds safe neutral.",
        18, 260, UiText::Caption, cAmber);
    lv_obj_set_width(note, 444);
    finishPageFocus();
}

void TransmitterUi::drawChannelPlaceholder()
{
    drawDesignPage(UiPage::ChannelPlaceholder);
}

void TransmitterUi::drawSettingRow(int row)
{
    static const char *labels[] = {"BRAKE/REV END", "SUB-TRIM", "FORWARD END", "DIRECTION"};
    const int16_t y = 56 + row * 50;
    lv_obj_t *panel = makePanel(10, y, 460, 44);
    makeLabel(panel, labels[row], 12, 12, UiText::Menu, cCyan);
    char value[20];
    if (row == 0) snprintf(value, sizeof(value), "%u%%", throttleProfile().negativeEndpointPercent);
    else if (row == 1) snprintf(value, sizeof(value), "%+d%%", throttleProfile().subtrimPercent);
    else if (row == 2) snprintf(value, sizeof(value), "%u%%", throttleProfile().positiveEndpointPercent);
    else snprintf(value, sizeof(value), "%s", throttleProfile().reversed ? "REVERSE" : "NORMAL");
    channel2SettingValueLabels_[row] = makeLabel(
        panel, value, 212, 12, UiText::Menu,
        row == 3 && throttleProfile().reversed ? cAmber : cWhite);
    if (row < 3) {
        lv_obj_t *minus = lv_obj_create(panel);
        position(minus, 343, 3, 48, 36);
        lv_obj_set_style_bg_color(minus, colour(cPanel2), 0);
        lv_obj_set_style_border_color(minus, colour(cLine), 0);
        lv_obj_set_style_border_width(minus, 1, 0);
        lv_obj_set_style_radius(minus, kUiCornerRadius, 0);
        lv_obj_set_style_pad_all(minus, 0, 0);
        lv_obj_t *ml = makeLabel(minus, "-", 0, 0, UiText::Heading); lv_obj_center(ml);
        lv_obj_t *plus = lv_obj_create(panel);
        position(plus, 401, 3, 48, 36);
        lv_obj_set_style_bg_color(plus, colour(cPanel2), 0);
        lv_obj_set_style_border_color(plus, colour(cLine), 0);
        lv_obj_set_style_border_width(plus, 1, 0);
        lv_obj_set_style_radius(plus, kUiCornerRadius, 0);
        lv_obj_set_style_pad_all(plus, 0, 0);
        lv_obj_t *pl = makeLabel(plus, "+", 0, 0, UiText::Heading); lv_obj_center(pl);
    } else {
        channel2DirectionToggle_ = lv_obj_create(panel);
        position(channel2DirectionToggle_, 343, 3, 106, 36);
        lv_obj_set_style_bg_color(channel2DirectionToggle_,
                                  colour(throttleProfile().reversed ? cAmber : cPanel2), 0);
        lv_obj_set_style_border_color(channel2DirectionToggle_, colour(cLine), 0);
        lv_obj_set_style_border_width(channel2DirectionToggle_, 1, 0);
        lv_obj_set_style_radius(channel2DirectionToggle_, kUiCornerRadius, 0);
        lv_obj_set_style_pad_all(channel2DirectionToggle_, 0, 0);
        channel2DirectionToggleLabel_ = makeLabel(
            channel2DirectionToggle_, "TOGGLE", 0, 0, UiText::Secondary,
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
    makeHeader(title,"Back");
    lv_obj_set_width(lv_obj_get_child(lv_obj_get_child(lv_scr_act(),0),0),150);
    makeButton(260,5,72,34,"Response",UiText::Secondary,cPanel2,cCyan);
    makeButton(340,5,90,34,"Calibrate",UiText::Secondary);
    addPageActionTarget(260,5,72,34,&TransmitterUi::controlThrottleHeader,0);
    addPageActionTarget(340,5,90,34,&TransmitterUi::controlThrottleHeader,1);
    for (int row = 0; row < 4; ++row) {
        drawSettingRow(row);
        addPageEditorTarget(10, 56 + row * 50, 460, 44,
                            353, 59 + row * 50, 48, 36,
                            411, 59 + row * 50, 48, 36,
                            &TransmitterUi::controlThrottleSetting, row);
    }
    makeLabel(lv_scr_act(), "INPUT", 12, 283, UiText::Secondary, cCyan);
    channel2ValueLabel_ = makeLabel(lv_scr_act(), "0%", 398, 283, UiText::Secondary, cGreen);
    channel2LiveBar_ = makeBar(lv_scr_act(), 70, 287, 316, 14,
                               -1000, 1000, cGreen);
    lv_bar_set_value(channel2LiveBar_, 0, LV_ANIM_OFF);
    makeLabel(lv_scr_act(), "BRAKE/REV", 70, 304, UiText::Caption, cDim);
    makeLabel(lv_scr_act(), "FORWARD", 312, 304, UiText::Caption, cDim);
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
    makeLabel(panel, labels[row], 7, 4, UiText::Caption, cCyan);
    char value[20];
    if (row == 0) snprintf(value, sizeof(value), "%+d%%", profile.subtrimPercent);
    else if (row == 1) snprintf(value, sizeof(value), "%u%%", profile.positiveEndpointPercent);
    else if (row == 2) snprintf(value, sizeof(value), "%u%%", profile.positiveRatePercent);
    else if (row == 3) snprintf(value, sizeof(value), "%+d%%", profile.positiveExpoPercent);
    else if (row == 4) snprintf(value, sizeof(value), "%u%%", profile.negativeEndpointPercent);
    else if (row == 5) snprintf(value, sizeof(value), "%u%%", profile.negativeRatePercent);
    else snprintf(value, sizeof(value), "%+d%%", profile.negativeExpoPercent);
    makeLabel(panel, value, 101, 4, UiText::Caption, cWhite);

    lv_obj_t *minus = lv_obj_create(panel); position(minus, 105, 14, 31, 15);
    lv_obj_set_style_bg_color(minus, colour(cPanel2), 0);
    lv_obj_set_style_border_color(minus, colour(cLine), 0);
    lv_obj_set_style_border_width(minus, 1, 0);
    lv_obj_set_style_radius(minus, kUiCornerRadius, 0);
    lv_obj_set_style_pad_all(minus, 0, 0);
    lv_obj_t *ml = makeLabel(minus, "-", 0, 0, UiText::Caption); lv_obj_center(ml);
    lv_obj_t *plus = lv_obj_create(panel); position(plus, 143, 14, 31, 15);
    lv_obj_set_style_bg_color(plus, colour(cPanel2), 0);
    lv_obj_set_style_border_color(plus, colour(cLine), 0);
    lv_obj_set_style_border_width(plus, 1, 0);
    lv_obj_set_style_radius(plus, kUiCornerRadius, 0);
    lv_obj_set_style_pad_all(plus, 0, 0);
    lv_obj_t *pl = makeLabel(plus, "+", 0, 0, UiText::Caption); lv_obj_center(pl);
}

void TransmitterUi::drawThrottleResponseGraph()
{
    const ChannelResponseProfile &profile = selectedResponseProfile();
    constexpr int16_t graphX = 23;
    constexpr int16_t graphY = 26;
    constexpr int16_t graphWidth = 236;
    const int16_t graphHeight = portraitUi ? 110 : 188;
    lv_obj_t *panel = makePanel(portraitUi ? 24 : 8, 52, 272, portraitUi ? 159 : 250);
    makeLabel(panel, "LIVE RESPONSE", 10, 7, UiText::Caption, cCyan);

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
              graphX, portraitUi ? 140 : 220, UiText::Caption, cDim);
    lv_obj_t *neutral = makeLabel(panel, "N", 0, portraitUi ? 140 : 220, UiText::Caption, cDim);
    lv_obj_align(neutral, LV_ALIGN_TOP_MID, 0, portraitUi ? 140 : 220);
    makeLabel(panel, throttle ? "FWD" : steering ? "RIGHT" : "POS",
              graphX + graphWidth - (steering ? 34 : 25), portraitUi ? 140 : 220, UiText::Caption, cDim);
    throttleGraphLiveLabel_ = makeLabel(panel, "IN +0%  OUT +0%",
                                        111, 6, UiText::Caption, cWhite);
    refreshThrottleResponseGraph(mapAdcToPermille(latestRawAdc_));
}

void TransmitterUi::refreshThrottleResponseGraph(int16_t rawInputPermille)
{
    if (!throttleGraphMarker_ || !throttleGraphLiveLabel_) return;
    constexpr int16_t graphX = 23;
    constexpr int16_t graphY = 26;
    constexpr int16_t graphWidth = 236;
    const int16_t graphHeight = portraitUi ? 110 : 188;
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
    makeButton(258, 5, 78, 34, "RAMP", UiText::Secondary, cPanel2, cCyan);
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
    lv_obj_t *panel = makePanel(14, y, 452, 76-kPanelGap);
    char label[28];
    snprintf(label, sizeof(label), "%s RAMP TIME", row == 0 ? positive : negative);
    makeLabel(panel, label, 12, 10, UiText::Menu, cCyan);
    const uint16_t value = row == 0 ? profile.positiveRampMs : profile.negativeRampMs;
    char text[20];
    if (value == 0) snprintf(text, sizeof(text), "DIRECT");
    else snprintf(text, sizeof(text), "%u ms", value);
    makeLabel(panel, text, 242, 10, UiText::Menu, cWhite);
    makeButton(350, y + 20, 42, 34, "-", UiText::Heading);
    makeButton(402, y + 20, 42, 34, "+", UiText::Heading);
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
        "0 ms = direct. Ramp is neutral-to-endpoint time.", 0, 250, UiText::Secondary, cDim);
    lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 250);
    finishPageFocus();
}

void TransmitterUi::drawDriveModesPage()
{
    drawDesignPage(UiPage::DriveModes);
}

void TransmitterUi::drawLiveIndicator(int16_t inputPermille)
{
    // Physical, calibrated input stays visible while transmitted output is CUT.
    // This is deliberately before output trim, reversal, rate and safety gating.
    inputPermille = constrain(inputPermille, -1000, 1000);
    if (channel2LiveBar_ && lv_bar_get_value(channel2LiveBar_) != inputPermille) {
        lv_bar_set_value(channel2LiveBar_, inputPermille, LV_ANIM_OFF);
        lv_label_set_text_fmt(channel2ValueLabel_, "%+d%%", inputPermille / 10);
    }
}

void TransmitterUi::drawSetupRow(int row)
{
    static const char *labels[] = {"BUZZER", "KEY CLICKS", "PITCH", "CLICK LENGTH"};
    const int16_t y = 56 + row * 50;
    lv_obj_t *panel = makePanel(10, y, 460, 44);
    makeLabel(panel, labels[row], 12, 12, UiText::Menu, cCyan);
    char value[20];
    if (row == 0) snprintf(value, sizeof(value), "%s", systemSettings_.buzzerEnabled ? "ON" : "OFF");
    else if (row == 1) snprintf(value, sizeof(value), "%s", systemSettings_.keyClicksEnabled ? "ON" : "OFF");
    else if (row == 2) snprintf(value, sizeof(value), "%u Hz", systemSettings_.buzzerPitchHz);
    else snprintf(value, sizeof(value), "%u ms", systemSettings_.keyClickDurationMs);
    makeLabel(panel, value, 212, 12, UiText::Menu, cWhite);
    if (row < 2) {
        const bool enabled = row == 0 ? systemSettings_.buzzerEnabled : systemSettings_.keyClicksEnabled;
        lv_obj_t *toggle = lv_obj_create(panel);
        position(toggle, 343, 3, 106, 36);
        lv_obj_set_style_bg_color(toggle, colour(enabled ? cGreen : cPanel2), 0);
        lv_obj_set_style_border_color(toggle, colour(cLine), 0);
        lv_obj_set_style_border_width(toggle, 1, 0);
        lv_obj_set_style_radius(toggle, kUiCornerRadius, 0);
        lv_obj_set_style_pad_all(toggle, 0, 0);
        lv_obj_t *label = makeLabel(toggle, "TOGGLE", 0, 0, UiText::Secondary, enabled ? cBlack : cWhite);
        lv_obj_center(label);
    } else {
        lv_obj_t *minus = lv_obj_create(panel); position(minus, 343, 3, 48, 36);
        lv_obj_set_style_bg_color(minus, colour(cPanel2), 0); lv_obj_set_style_pad_all(minus, 0, 0);
        lv_obj_t *ml = makeLabel(minus, "-", 0, 0, UiText::Heading); lv_obj_center(ml);
        lv_obj_t *plus = lv_obj_create(panel); position(plus, 401, 3, 48, 36);
        lv_obj_set_style_bg_color(plus, colour(cPanel2), 0); lv_obj_set_style_pad_all(plus, 0, 0);
        lv_obj_t *pl = makeLabel(plus, "+", 0, 0, UiText::Heading); lv_obj_center(pl);
    }
}

void TransmitterUi::drawSetupPage()
{
    drawDesignPage(UiPage::Setup);
}

void TransmitterUi::drawFirmwareInfoPage()
{
    activePage_ = UiPage::FirmwareInfo;
    startScreen();
    resetLvglObjects();
    makeHeader("FIRMWARE INFO", "BACK");
    beginPageFocus();

    lv_obj_t *identity = makePanel(10, 58, kScreenWidth-20, portraitUi ? 106 : 72);
    makeLabel(identity, JR_R1_FIRMWARE_NAME, 14, 10, UiText::Heading, cCyan);
    char revision[80];
    snprintf(revision, sizeof(revision), "%s @ %s  [%s]",
             JR_R1_FIRMWARE_BRANCH, JR_R1_FIRMWARE_COMMIT,
             JR_R1_FIRMWARE_GIT_STATE);
    makeLabel(identity, revision, 14, 40, UiText::Caption,
              strcmp(JR_R1_FIRMWARE_GIT_STATE, "clean") == 0
                  ? cGreen : cAmber);

    const int buildTop=58+(portraitUi?106:72)+kPanelGap;
    lv_obj_t *build = makePanel(10, buildTop, kScreenWidth-20, portraitUi ? 126 : 82);
    makeLabel(build, "BUILD", 14, 9, UiText::Caption, cDim);
    char builtAt[48];
    snprintf(builtAt, sizeof(builtAt), "%s  %s",
             JR_R1_FIRMWARE_BUILD_DATE, JR_R1_FIRMWARE_BUILD_TIME);
    makeLabel(build, builtAt, 14, 29, UiText::Menu, cWhite);
    makeLabel(build, JR_R1_FIRMWARE_BUILD_ENV, 14, 54, UiText::Caption, cCyan);

    lv_obj_t *hardware = makePanel(10, buildTop+(portraitUi?126:82)+kPanelGap, kScreenWidth-20, portraitUi ? 126 : 76);
    makeLabel(hardware, "HARDWARE PROFILE", 14, 8, UiText::Caption, cDim);
#ifdef JR_R1_XIAO_PCB
    makeLabel(hardware, "Seeed XIAO ESP32-S3 / R-1 PCB", 14, 28, UiText::Secondary, cWhite);
#else
    makeLabel(hardware, "ESP32-S3-DevKitC-1 N16R8", 14, 28, UiText::Secondary, cWhite);
#endif
    makeLabel(hardware, DISPLAY_PROFILE_NAME, 14, 50, UiText::Caption, cCyan);
    if(portraitUi){for(auto panel : {identity,build,hardware}) {for(uint32_t i=0;i<lv_obj_get_child_cnt(panel);++i){auto label=lv_obj_get_child(panel,i);lv_obj_set_width(label,270);lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);}}}
    finishPageFocus();
}

void TransmitterUi::drawElrsTxConfigPage()
{
    if (activePage_ != UiPage::ElrsTxConfig) {
        requestConfigurationCut();
    }
    activePage_ = UiPage::ElrsTxConfig;
    startScreen(); resetLvglObjects();
    makeHeader("ELRS TX SETTINGS", "BACK");
    beginPageFocus();
    auto label = makeLabel(lv_scr_act(), txConfig_.status(), 8, 48, UiText::Caption, cAmber);
    lv_obj_set_width(label, kScreenWidth - 16);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    const auto selected = txConfig_.parameter(txConfigSelected_);
    const int width = kScreenWidth - 16;
    const int footer = kScreenHeight - (portraitUi ? 62 : 40);
    if (selected) {
        label = makeLabel(lv_scr_act(), selected->name, 8, 92, UiText::Menu, cCyan);
        lv_obj_set_width(label, width);
        char value[384] = {};
        if (selected->type == 9) {
            selected->option(txConfigDraft_, value, sizeof(value));
            const size_t length = strlen(value);
            if (selected->info[0]) snprintf(value+length,sizeof(value)-length," %s",selected->info);
        }
        else snprintf(value, sizeof(value), "%s", selected->info);
        label = makeLabel(lv_scr_act(), value, 8, 124, UiText::Menu, cWhite);
        lv_obj_set_width(label, width); lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        if (selected->type == 9) {
            makePageActionButton(8, 192, width/2-4, 34, "PREVIOUS VALUE", UiText::Secondary,
                cPanel2, cWhite, cLine, &TransmitterUi::controlElrsTxConfig, 8);
            makePageActionButton(12+width/2, 192, width/2-4, 34, "NEXT VALUE", UiText::Secondary,
                cPanel2, cWhite, cLine, &TransmitterUi::controlElrsTxConfig, 9);
        }
        if (selected->type == 9 || selected->type == 13) {
            const char *action = selected->type == 9 ? "APPLY" :
                (txConfig_.confirming() ? "CONFIRM" : "RUN COMMAND");
            makePageActionButton(8, footer, width/2-4, 32, action, UiText::Secondary,
                cPanel2, cAmber, cLine, &TransmitterUi::controlElrsTxConfig, 10);
        }
        makePageActionButton(12+width/2, footer, width/2-4, 32, "CANCEL / BACK", UiText::Secondary,
            cPanel2, cWhite, cLine, &TransmitterUi::controlElrsTxConfig, 11);
        finishPageFocus();
        return;
    }
    const auto folder = txConfig_.parameter(txConfigFolder_);
    label = makeLabel(lv_scr_act(), folder ? folder->name : txConfig_.device(), 8, 78, UiText::Caption, cCyan);
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
        makePageActionButton(8, 100+row*(portraitUi?68:38), width, portraitUi?60:34, text, portraitUi?UiText::Secondary:UiText::Secondary,
            cPanel2, cWhite, cLine, &TransmitterUi::controlElrsTxConfig,row);
        if(portraitUi){auto button=lv_obj_get_child(lv_scr_act(),-1);auto textLabel=lv_obj_get_child(button,0);lv_obj_set_width(textLabel,width-20);lv_label_set_long_mode(textLabel,LV_LABEL_LONG_WRAP);lv_obj_set_style_text_align(textLabel,LV_TEXT_ALIGN_LEFT,0);lv_obj_center(textLabel);}
        ++row;
    }
    const int cell=width/4;
    const char *actions[]={"PREV","NEXT","REFRESH","BINDING"};
    for (uint8_t i=0;i<4;++i)
        makePageActionButton(8+i*cell,footer,cell-4,32,actions[i],UiText::Secondary,
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
    auto phraseLabel=makeLabel(phrasePanel, phraseText, 8, 3, UiText::Caption, cWhite);
    lv_obj_set_width(phraseLabel,kScreenWidth-34);lv_label_set_long_mode(phraseLabel,LV_LABEL_LONG_WRAP);
    auto statusLabel=makeLabel(phrasePanel, elrsStatus_, 8, portraitUi ? 51 : 22, UiText::Caption,
              strstr(elrsStatus_, "MATCH") != nullptr ? cGreen : cAmber);
    lv_obj_set_width(statusLabel,kScreenWidth-34);lv_label_set_long_mode(statusLabel,LV_LABEL_LONG_WRAP);

    const char *actions[] = {"SET TX", "SET RX", "UID TX", "UID RX", "CLEAR"};
    for (uint8_t index = 0; index < 5; ++index) {
        const bool unavailable = (index == 1 || index == 3) &&
                                 !elrsReceiverLinkAvailable_;
        makePageActionButton(portraitUi ? 8+(index%3)*103 : 8+index*93, portraitUi ? 143+(index/3)*42 : 94, portraitUi ? 98 : 87, portraitUi ? 38 : 32,
                             actions[index], UiText::Secondary,
                             unavailable ? cBlack : cPanel2,
                             unavailable ? cDim :
                                 (index < 2 ? cAmber : cCyan),
                             cLine, &TransmitterUi::controlElrsBinding, index);
    }

    const char *rows[] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm-_"};
    const uint8_t lengths[] = {10, 10, 9, 9};
    for (uint8_t row = 0; row < 4; ++row) {
        const int16_t width = portraitUi ? 31 : 42;
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
                                 width - 3, portraitUi ? 42 : 34, label, UiText::Secondary,
                                 cPanel, cWhite, cLine,
                                 &TransmitterUi::controlElrsBinding,
                                 5 + keyIndex);
        }
    }
    makePageActionButton(portraitUi ? 8 : 43, portraitUi ? 414 : 282, portraitUi ? 72 : 92, portraitUi ? 40 : 32,
                         elrsKeyboardUppercase_ ? "lower" : "UPPER", UiText::Secondary,
                         cPanel2, cWhite, cLine,
                         &TransmitterUi::controlElrsBinding, 43);
    makePageActionButton(portraitUi ? 86 : 143, portraitUi ? 414 : 282, portraitUi ? 148 : 194, portraitUi ? 40 : 32, "SPACE", UiText::Secondary,
                         cPanel2, cWhite, cLine,
                         &TransmitterUi::controlElrsBinding, 44);
    makePageActionButton(portraitUi ? 240 : 345, portraitUi ? 414 : 282, portraitUi ? 72 : 92, portraitUi ? 40 : 32, "<--", UiText::Secondary,
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
              12, 7, UiText::Caption, cAmber);
    lv_obj_t *profile = makeLabel(status, "TS160 DRAFT", 0, 8, UiText::Caption, cDim);
    lv_obj_align(profile, LV_ALIGN_TOP_RIGHT, -10, portraitUi ? 34 : 8);

    static const char *tabs[] = {"BASIC", "BRAKE", "MOTOR"};
    const uint8_t actions[]={0,1,2};
    drawPageTabs(tabs,actions,3,escProgrammingTab_,&TransmitterUi::controlEscProgramming,portraitUi?118:87);

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
        lv_obj_t *panel = makePanel(8, y, kScreenWidth-16, (portraitUi ? 61 : 36)-kPanelGap, cPanel, cLine, 5);
        makeLabel(panel, labels[page][row], 10, 7, UiText::Caption, cCyan);
        lv_obj_t *value = makeLabel(panel, values[page][row], 0, 7, UiText::Caption, cWhite);
        lv_obj_align(value, LV_ALIGN_TOP_RIGHT, -12, portraitUi ? 30 : 7);
    }

    // These controls establish the intended workflow without suggesting that
    // a value has actually been read from or written to the ESC.
    makePageActionButton(8, portraitUi ? 416 : 274, portraitUi ? 100 : 168, portraitUi ? 40 : 35, "READ ESC", UiText::Secondary,
                         cBlack, cDim, cDim,
                         &TransmitterUi::controlEscProgramming, 3);
    makePageActionButton(portraitUi ? 114 : 184, portraitUi ? 416 : 274, portraitUi ? 100 : 168, portraitUi ? 40 : 35, "WRITE ESC", UiText::Secondary,
                         cBlack, cDim, cDim,
                         &TransmitterUi::controlEscProgramming, 4);
    lv_obj_t *comms = makePanel(portraitUi ? 220 : 360, portraitUi ? 416 : 274, portraitUi ? 92 : 112, portraitUi ? 40 : 35,
                                cSafetyWaiting, cAmber, 7);
    lv_obj_t *commsLabel = makeLabel(comms, "NO COMMS", 0, 0, UiText::Caption, cAmber);
    lv_obj_center(commsLabel);
    finishPageFocus();
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
        auto panel = makePanel(8, y, kScreenWidth - 16, rowHeight - kPanelGap,
                               choice == selected ? cPanel2 : cPanel, cLine);
        makeLabel(panel, colourSchemeName(choice != 0, choice ? choice - 1 : 0),
                  12, (rowHeight - UiText::font(UiText::Menu)->line_height) / 2, UiText::Menu, choice == selected ? cCyan : cWhite);
        if (choice == selected) makeLabel(panel, LV_SYMBOL_OK, kScreenWidth - 46,
                                         (rowHeight - 20) / 2, UiText::Secondary, cCyan);
        addPageActionTarget(8, y, kScreenWidth - 16, rowHeight - kPanelGap,
                            &TransmitterUi::controlColourScheme, choice);
    }
    makeLabel(lv_scr_act(), "Saved automatically. Night mode uses your last dark scheme.",
              12, portraitUi ? 376 : 275, UiText::Caption, cDim);
    auto note = lv_obj_get_child(lv_scr_act(), -1);
    lv_obj_set_width(note, kScreenWidth - 24);
    finishPageFocus();
}

void TransmitterUi::drawDisplayPage()
{
    drawDesignPage(UiPage::DisplaySetup);
}

void TransmitterUi::drawTxBatteryPage()
{
    drawDesignPage(UiPage::TxBatterySetup);
}

void TransmitterUi::drawRxBatteryRow(int row)
{
    static const char *labels[] = {"CHEMISTRY", "CELL COUNT", "LOW ALARM", "ALARM"};
    const int16_t y = 56 + row * 50;
    lv_obj_t *panel = makePanel(10, y, 460, 44);
    makeLabel(panel, labels[row], 12, 12, UiText::Menu, cCyan);
    char value[24];
    if (row == 0) snprintf(value, sizeof(value), "%s", batteryChemistryName(rxBatterySettings_.chemistry));
    else if (row == 1) snprintf(value, sizeof(value), "%uS", rxBatterySettings_.cellCount);
    else if (row == 2) snprintf(value, sizeof(value), "%u.%u V",
        rxBatterySettings_.lowAlarmDecivolts / 10, rxBatterySettings_.lowAlarmDecivolts % 10);
    else snprintf(value, sizeof(value), "%s", rxBatterySettings_.alarmEnabled ? "ON" : "OFF");
    makeLabel(panel, value, 212, 12, UiText::Menu,
              row == 3 && rxBatterySettings_.alarmEnabled ? cGreen : cWhite);

    if (row == 0 || row == 3) {
        const bool enabled = row == 3 && rxBatterySettings_.alarmEnabled;
        lv_obj_t *toggle = lv_obj_create(panel);
        position(toggle, 343, 3, 106, 36);
        lv_obj_set_style_bg_color(toggle, colour(enabled ? cGreen : cPanel2), 0);
        lv_obj_set_style_border_color(toggle, colour(cLine), 0);
        lv_obj_set_style_border_width(toggle, 1, 0);
        lv_obj_set_style_radius(toggle, kUiCornerRadius, 0);
        lv_obj_set_style_pad_all(toggle, 0, 0);
        lv_obj_t *label = makeLabel(toggle, row == 0 ? "NEXT" : "TOGGLE", 0, 0, UiText::Secondary,
                                    enabled ? cBlack : cWhite);
        lv_obj_center(label);
    } else {
        makeButton(353, y + 4, 48, 36, "-", UiText::Heading);
        makeButton(411, y + 4, 48, 36, "+", UiText::Heading);
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
        "Voltage received from ER6G telemetry", 0, 280, UiText::Caption, cDim);
    lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 280);
    finishPageFocus();
}

void TransmitterUi::drawCalibrationPage()
{
    startScreen();
    resetLvglObjects();
if(portraitUi) {
    makeHeader(calibratingSteering_ ? "STEERING CAL" : "THROTTLE CAL", "CANCEL");
} else {
    makeHeader(calibratingSteering_
        ? "STEERING CALIBRATION" : "THROTTLE CALIBRATION", "CANCEL");
}
    beginPageFocus();
if(portraitUi) {
    {
    makeLabel(lv_scr_act(), calibratingSteering_
        ? "1. HOLD FULL LEFT" : "1. HOLD FULL BRAKE",
        16, 68, UiText::Menu, cCyan);
    makeLabel(lv_scr_act(), calibratingSteering_
        ? "2. HOLD FULL RIGHT" : "2. HOLD FULL FORWARD",
        16, 102, UiText::Menu, cCyan);
    makeLabel(lv_scr_act(), "3. RELEASE AND HOLD CENTRE", 16, 136, UiText::Menu, cCyan);
    char held[44];
    snprintf(held, sizeof(held), "CH%u OUTPUT HELD AT NEUTRAL",
             calibratingSteering_ ? channelRoleSettings_.steeringChannel
                                  : channelRoleSettings_.throttleChannel);
    makeLabel(lv_scr_act(), held, 16, 178, UiText::Caption, cAmber);
    lv_obj_t *panel = makePanel(10, 214, 300, 246);
    makeLabel(panel, "MINIMUM AVERAGE", 16, 18, UiText::Caption, cDim);
    makeLabel(panel, "LIVE INPUT", 16, 86, UiText::Caption, cDim);
    makeLabel(panel, "MAXIMUM AVERAGE", 16, 154, UiText::Caption, cDim);
    calibrationMinimumLabel_ = makeLabel(panel, "----", 190, 10, UiText::Heading);
    calibrationLiveLabel_ = makeLabel(panel, "----", 190, 78, UiText::Heading);
    calibrationMaximumLabel_ = makeLabel(panel, "----", 190, 146, UiText::Heading);
    calibrationStatusLabel_ = makeLabel(panel, "", 12, 198, UiText::Caption, cCyan);
    lv_obj_set_width(calibrationStatusLabel_, 276);
    drawCalibrationValues(calibratingSteering_
        ? latestSteeringRawAdc_ : latestRawAdc_);
    finishPageFocus();
    return;
    }
}
    makeLabel(lv_scr_act(), calibratingSteering_
        ? "1. HOLD FULL LEFT" : "1. HOLD FULL BRAKE",
        20, 62, UiText::Menu, cCyan);
    makeLabel(lv_scr_act(), calibratingSteering_
        ? "2. HOLD FULL RIGHT" : "2. HOLD FULL FORWARD",
        20, 92, UiText::Menu, cCyan);
    makeLabel(lv_scr_act(), "3. RELEASE AND HOLD CENTRE", 20, 122, UiText::Menu, cCyan);
    char held[44];
    snprintf(held, sizeof(held), "CH%u OUTPUT HELD AT NEUTRAL",
             calibratingSteering_ ? channelRoleSettings_.steeringChannel
                                  : channelRoleSettings_.throttleChannel);
    makeLabel(lv_scr_act(), held, 20, 160, UiText::Menu, cAmber);
    lv_obj_t *panel = makePanel(18, 202, 444, 92);
    makeLabel(panel, "MIN AVG", 16, 10, UiText::Secondary, cDim);
    makeLabel(panel, "LIVE", 180, 10, UiText::Secondary, cDim);
    makeLabel(panel, "MAX AVG", 326, 10, UiText::Secondary, cDim);
    calibrationMinimumLabel_ = makeLabel(panel, "----", 10, 42, UiText::Heading);
    calibrationLiveLabel_ = makeLabel(panel, "----", 168, 42, UiText::Heading);
    calibrationMaximumLabel_ = makeLabel(panel, "----", 326, 42, UiText::Heading);
    calibrationStatusLabel_ = makeLabel(lv_scr_act(), "", 20, 180, UiText::Caption, cCyan);
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
    if (calibrationStatusLabel_) {
        if (!progress.sawMinimum || !progress.sawMaximum)
            lv_label_set_text_fmt(calibrationStatusLabel_, "Low: %s  High: %s%s",
                progress.sawMinimum ? "OK" : "WAIT",
                progress.sawMaximum ? "OK" : "WAIT",
                portraitUi ? "\nHold each end until OK." : " - Hold until OK");
        else
            lv_label_set_text_fmt(calibrationStatusLabel_, "Both ends OK. Hold neutral%s%u / 1500 ms",
                portraitUi ? "\n" : " ", progress.centreHoldMs);
    }
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
    const ProportionalCalibration previous = input.calibration();
    if (!input.updateCalibration(adc, millis())) return;
    const ProportionalCalibration &calibration = input.calibration();
    if (!saveCalibration()) {
        input.setCalibration(previous);
        requestConfigurationCut();
        if (calibrationStatusLabel_)
            lv_label_set_text(calibrationStatusLabel_, "Save failed. Old values kept.\nCancel and retry.");
        Serial.println("CAL: save FAILED; previous calibration retained");
        return;
    }
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
    pixelCache.invalidate(); // SDL applies brightness when receiving pixels.
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

    const auto pointOnScreen = waveshareTouchPoint(rawX, rawY, portraitUi);
    x = pointOnScreen.x;
    y = pointOnScreen.y;
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
if(portraitUi) {
    if (y < 44) {
        if (x >= 272) {
            playKeyClick(); drawMainMenu();
        } else if (lastSafetyStatus_ == OutputSafetyStatus::Armed) {
            requestConfigurationCut();
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
}
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
if(portraitUi) {
    {
    if (x < 5 || x >= 315 || y < 52 || y >= 436) return;
    const int item = (y - 52) / 48;
    if (item < 0 || item >= 8 || !mainMenuItemEnabled(item)) return;
    mainMenuFocusIndex_ = static_cast<uint8_t>(item);
    activateMainMenuFocus();
    return;
    }
}
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
        if (x >= 232 &&
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
    requestConfigurationCut();
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
        else if(throttleResponseReturnPage_==UiPage::ChannelResponseSetup)drawDesignPage(UiPage::ChannelResponseSetup);
        else drawMainMenu();
        return;
    }
    if (y < 48 && x >= 250 && x < 340) { playKeyClick(); drawChannelSpeedPage(); return; }
    if (y < 52 || y >= 297 || x < 288) return;
    const int row = (y - 52) / 35;
    const int8_t direction = x < 424 ? -1 : 1;
    auto profile = selectedResponseProfile();
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
    if (changed) { channelModel_.setResponse(selectedChannel_,profile); scheduleSettingsSave(); playKeyClick(); drawThrottleResponsePage(); }
}

void TransmitterUi::handleChannelSpeedTouch(int16_t x, int16_t y)
{
    if (headerActionTouched(x, y)) { playKeyClick(); drawThrottleResponsePage(); return; }
    if (x < 340 || y < 78 || y >= 230) return;
    const uint8_t row = (y - 78) / 76;
    const int16_t direction = x < 400 ? -1 : 1;
    auto profile = selectedResponseProfile();
    uint16_t &value = row == 0 ? profile.positiveRampMs : profile.negativeRampMs;
    const int32_t changed = static_cast<int32_t>(value) +
                            direction * kChannelSpeedStepMs;
    value = constrain(changed, 0L, static_cast<long>(kChannelSpeedMaximumMs));
    channelModel_.setResponse(selectedChannel_,profile);
    scheduleSettingsSave();
    playKeyClick();
    drawChannelSpeedPage();
}

void TransmitterUi::handleControlAssignmentsTouch(int16_t x, int16_t y)
{
    if (headerActionTouched(x, y)) { navigateBack(); return; }
    constexpr uint8_t kRows = 5;
    constexpr uint8_t kBanks = (kPhysicalControlCount + kRows - 1) / kRows;
    if (y >= 258) {
        if (x < 60 && assignmentBank_ > 0) {
            --assignmentBank_; playKeyClick(); drawControlAssignmentsPage();
        } else if (x >= 70 && x < 164) {
            playKeyClick(); drawChannelsPage();
        } else if (x >= 174 && x < 306) {
            inputMonitorReturnPage_=UiPage::ControlAssignments;inputMonitorBank_ = 0; playKeyClick(); drawInputMonitorPage();
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
        selectedChannel_=control.id==PhysicalControlId::SteeringWheel?steeringChannel():throttleChannel();channelInputPage_=0;
        drawDesignPage(UiPage::ChannelInput);
        return;
    }
    selectedBindingInput_ = control.id;
    const int existing = bindingIndexForInput(control.id);
    bindingDraft_ = existing >= 0 ? bindings_[existing] :
                                    defaultBindingForInput(control.id);
    if (existing < 0)
        bindingDraft_.crsfChannel = assignmentChannelContext_ ? assignmentChannelContext_ : adjacentBindingChannel(2, 1);
    bindingError_[0] = '\0';bindingEditorBank_=0;
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
            bindingDraft_.crsfChannel = assignmentChannelContext_ ? assignmentChannelContext_ : adjacentBindingChannel(2, 1);
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
        if(!requestDriveMode(static_cast<DriveMode>(index)))return;
        requestConfigurationCut();
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
    requestConfigurationCut();
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
    if(!bootPinUnlocked_||pinPurpose_!=PinPurpose::None){
        if(activePage_==UiPage::BootPin)dispatchPageControlTouch(x,y);
        return;
    }
    if(wifiStatusIcon&&!navigationIsModal()&&x>=kScreenWidth-40&&x<kScreenWidth&&y>=0&&y<44){
        controlDesign(205,PageControlEvent::Activate);return;
    }
    if(activePage_==UiPage::Gps&&gpsFullScreen_){
        gpsFullScreen_=false;lastInteractionMs_=millis();armGestureValid_=false;drawDesignPage(UiPage::Gps);return;
    }
    if(!navigationIsModal() && designLink_ && moduleLinkManaged_ && lastTouchDurationMs_>=800 &&
       touchStartX_>=kScreenWidth-110 && touchStartX_<kScreenWidth-44 && touchStartY_<44 &&
       x>=kScreenWidth-110 && x<kScreenWidth-44 && y<44){
        moduleLinkReturnPage_=activePage_;drawDesignPage(UiPage::ModuleLink);return;
    }
    if(sectionNavigationVisible_ && y>=kScreenHeight-(portraitUi?60:40) && y<kScreenHeight-(portraitUi?20:0)){
        designTouchDispatch_=true;controlSectionNavigation(min<int>(3,x/(kScreenWidth/4)),PageControlEvent::Activate);designTouchDispatch_=false;return;
    }
    if(activePage_==UiPage::WifiSetup){
        if(headerActionTouched(x,y))navigateBack();else dispatchPageControlTouch(x,y);
        return;
    }
    if (activePage_ == UiPage::LogoSaver) {
        leaveLogoSaver();
        return;
    }
    if (activePage_ == UiPage::BootSplash ||
        activePage_ == UiPage::StartupWarning) return;
    if (designPage()) { designTouchDispatch_ = true; dispatchPageControlTouch(x, y); designTouchDispatch_ = false; return; }
    if (calibrationActive()) {
        if (headerActionTouched(x, y)) cancelCalibration();
        return;
    }
    if (dispatchPageControlTouch(x, y)) return;
    if (portraitUi) return;
    if(detailContentCompacted_ && y>=44){
        const int numerator=kScreenHeight-40-4-44,denominator=kScreenHeight-44;
        y=44+(y-44)*denominator/numerator;
    }
    if (activePage_ == UiPage::Dashboard) { handleDashboardTouch(x, y); return; }
    if (activePage_ == UiPage::RadioDiagnostics) { handleTelemetryTouch(x, y); return; }
    if (activePage_ == UiPage::MainMenu) { handleMainMenuTouch(x, y); return; }
    if (activePage_ == UiPage::ModelSelect) { handleModelSelectTouch(x, y); return; }
    if (activePage_ == UiPage::Channels) { handleChannelsTouch(x, y); return; }
    if (activePage_ == UiPage::ChannelRoles) { handleChannelRolesTouch(x, y); return; }
    if (activePage_ == UiPage::ThrottleResponse) { handleThrottleResponseTouch(x, y); return; }
    if (activePage_ == UiPage::ChannelSpeed) { handleChannelSpeedTouch(x, y); return; }
    if (activePage_ == UiPage::ControlAssignments) { handleControlAssignmentsTouch(x, y); return; }
    if (activePage_ == UiPage::BindingEditor) return; // Registered targets only; no obsolete coordinate hotspots.
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

void TransmitterUi::updateDriveHoldFeedback(uint32_t now)
{
    const bool physical=touchWasDown_&&armGestureValid_&&touchStartPage_==activePage_;
    const bool remote=!touchWasDown_&&remoteArmHold_.active(now)&&remoteArmPage_==static_cast<uint32_t>(activePage_);
    if((!physical&&!remote)||!bootPinUnlocked_||pinPurpose_!=PinPurpose::None||hardwareRecovery_||displaySleeping_){
        if(driveHoldOverlay_)lv_obj_add_flag(driveHoldOverlay_,LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if(!driveHoldOverlay_){
        const int y=kScreenHeight-(portraitUi?60:40)-60;
        driveHoldOverlay_=makePanel(10,y,kScreenWidth-20,50,cPanel2,cLine);
        lv_obj_clear_flag(driveHoldOverlay_,LV_OBJ_FLAG_CLICKABLE);
        driveHoldLabel_=makeLabel(driveHoldOverlay_,"",10,6,UiText::Secondary,cWhite);
        lv_obj_set_width(driveHoldLabel_,kScreenWidth-40);
        lv_obj_set_style_text_align(driveHoldLabel_,LV_TEXT_ALIGN_CENTER,0);
        driveHoldBar_=makeBar(driveHoldOverlay_,10,33,kScreenWidth-40,8,0,kDashboardArmHoldMs);
        lv_obj_clear_flag(driveHoldLabel_,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(driveHoldBar_,LV_OBJ_FLAG_CLICKABLE);
    }
    const uint32_t elapsed=physical?uint32_t(now-touchDownStartedMs_):remoteArmHold_.elapsed(now);
    const bool complete=elapsed>=kDashboardArmHoldMs;
    const char *text=armRequested_?(complete?"Release to CUT":"Keep holding to CUT"):
        SetupPortal::active()?"Finish Wi-Fi setup first":
        moduleLinkManaged_&&!moduleLink_.enabled()?"Enable the module link first":
        calibrationActive()?"Calibration active":
        complete?"Release to request output":"Keep holding to enable output";
    const bool blocked=!armRequested_&&(SetupPortal::active()||(moduleLinkManaged_&&!moduleLink_.enabled())||calibrationActive());
    uiSetText(driveHoldLabel_,text);
    const auto accent=colour(armRequested_||blocked?cAmber:cCyan);
    uiSetTextColor(driveHoldLabel_,accent,0);
    uiSetBgColor(driveHoldBar_,accent,LV_PART_INDICATOR);
    const int progress=elapsed>=kDashboardArmHoldMs?kDashboardArmHoldMs:int((elapsed/25)*25);
    if(lv_bar_get_value(driveHoldBar_)!=progress)lv_bar_set_value(driveHoldBar_,progress,LV_ANIM_OFF);
    if(lv_obj_has_flag(driveHoldOverlay_,LV_OBJ_FLAG_HIDDEN)){
        lv_obj_clear_flag(driveHoldOverlay_,LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(driveHoldOverlay_);
    }
}

void TransmitterUi::updateTouch()
{
    if (hardwareRecovery_) return;
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
        touchGestureX_=x;touchGestureY_=y;
        if (!touchWasDown_) {
            touchWasDown_ = true;
            touchDownStartedMs_ = now;
            touchStartX_=x;touchStartY_=y;touchStartPage_=activePage_;
            // Arming must begin inside its target and remain there throughout
            // the hold; dragging into it after a long press cannot arm.
            const bool headerHold=driveProgress_&&x>=0&&x<kScreenWidth/4&&y>=kScreenHeight-(portraitUi?60:40)&&y<kScreenHeight-(portraitUi?20:0);
            const bool panelHold=activePage_==UiPage::OutputStatus&&x>=12&&x<kScreenWidth-12&&y>=(portraitUi?238:184)&&y<(portraitUi?286:232);
            armGestureValid_=headerHold||panelHold;
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
        if(armGestureValid_){
            const bool headerHold=driveProgress_&&x>=0&&x<kScreenWidth/4&&y>=kScreenHeight-(portraitUi?60:40)&&y<kScreenHeight-(portraitUi?20:0);
            const bool panelHold=activePage_==UiPage::OutputStatus&&x>=12&&x<kScreenWidth-12&&y>=(portraitUi?238:184)&&y<(portraitUi?286:232);
            if(!headerHold&&!panelHold)armGestureValid_=false;
        }
        if(armProgress_&&activePage_==UiPage::OutputStatus){
            const bool inside=lastTouchX_>=12&&lastTouchX_<kScreenWidth-12&&lastTouchY_>=(portraitUi?238:184)&&lastTouchY_<(portraitUi?286:232);
            lv_bar_set_value(armProgress_,inside&&armGestureValid_?min(800,int(now-touchDownStartedMs_)):0,LV_ANIM_OFF);
        }
        if(driveProgress_)lv_bar_set_value(driveProgress_,armGestureValid_?min(800,int(now-touchDownStartedMs_)):0,LV_ANIM_OFF);
        updateWifiKeyPreview(lastTouchX_, lastTouchY_);
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
    lastTouchDurationMs_ = touchReleaseSinceMs_ - touchDownStartedMs_;
    touchWasDown_ = false;
    touchReleaseSinceMs_ = 0;
    touchIgnoreUntilMs_ = now + kTouchRetriggerGuardMs;
    lastInteractionMs_ = now;
    Serial.printf("TOUCH up page=%s screen=(%d,%d)\n",
                  uiPageName(activePage_), releasedX, releasedY);
    uiMetrics.touchAcceptedUs = micros();
    uiMetrics.touchPending = true;
    updateWifiKeyPreview(-1, -1);
    if(driveProgress_)lv_bar_set_value(driveProgress_,0,LV_ANIM_OFF);
    if(armProgress_)lv_bar_set_value(armProgress_,0,LV_ANIM_OFF);
    if(touchStartPage_==activePage_&&!handlePageSwipe(touchStartX_,touchStartY_,touchGestureX_,touchGestureY_,lastTouchDurationMs_))
        handleTouch(releasedX, releasedY);
    armGestureValid_=false;
}

void TransmitterUi::updateModulePower(bool detected, uint32_t now)
{
    moduleLinkManaged_=true;
    const bool wasEnabled=moduleLink_.enabled(),wasPresent=moduleLink_.present();
    moduleLink_.update(detected,now);
    if(wasEnabled&&!moduleLink_.enabled())requestConfigurationCut();
    if(activePage_==UiPage::ModuleLink&&wasPresent!=moduleLink_.present()){
        touchWasDown_=false;discardTouchUntilRelease_=true;armGestureValid_=false;
        drawDesignPage(UiPage::ModuleLink);
    }
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
    if (hardwareRecovery_ || rfBenchMode_ || (moduleLinkManaged_&&!moduleLink_.enabled())) armRequested_ = false;
    updateKeypad(millis());
    updatePinTimeout(millis());
    if(moduleLinkManaged_ && moduleLink_.prompt() && bootPinUnlocked_ &&
       pinPurpose_==PinPurpose::None && activePage_==UiPage::Dashboard && !touchWasDown_){
        moduleLinkReturnPage_=UiPage::Dashboard;
        // A finger already resting on the dashboard must not confirm a popup
        // that appeared underneath it. Require release and a new press.
        discardTouchUntilRelease_=true;armGestureValid_=false;
        drawDesignPage(UiPage::ModuleLink);
    }
    if(!bootPinUnlocked_||pinPurpose_!=PinPurpose::None){armRequested_=false;if(activePage_!=UiPage::BootSplash&&activePage_!=UiPage::BootPin)drawBootPin();}
    speedStatistics_.update(telemetry.groundSpeedDeciKmh, telemetry.gpsUpdatedMs, millis(),
                            gpsPositionAvailable(telemetry,millis()));
    designTelemetry_ = telemetry; designSafety_ = safetyStatus;
    designTxVoltage_ = txVoltageDecivolts; designTxValid_ = txVoltageValid;
    latestRawAdc_ = rawAdc;
    latestSteeringRawAdc_ = steeringRawAdc;
    updateSteeringEndpointSelection();
    // Dashboard controls preview requested positions even in CUT. Keep the
    // separately resolved values for Data > Channels (requested / sent).
    for (uint8_t ch=1; ch<=16; ++ch) designRequested_[ch-1] = channelOutputs.requested(ch);
    const uint16_t enabledMask = channelOutputs.enabledMask();
    dashboardOutputsLive_=channelOutputsLive;
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
    if(SetupPortal::active())armRequested_=false;
    if(activePage_==UiPage::WifiSetup){
        if(!SetupPortal::active()){requestConfigurationCut();drawMainMenu();}
        else if(wifiRevision_!=SetupPortal::state().revision && wifiEdit_!=1 && wifiEdit_!=2 && wifiEdit_!=4 && wifiEdit_!=6)drawWifiPage();
    }
    updateWifiStatusIcon();
    updateTouch();
    updateDriveHoldFeedback(millis());
    if(rotationPending_&&!touchWasDown_)applyDisplayRotation();
    if (calibrationActive()) updateCalibration(
        calibratingSteering_ ? steeringRawAdc : rawAdc);

    const uint32_t now = millis();
    if (activePage_ == UiPage::BootSplash &&
        now - bootSplashStartedMs_ >= kBootSplashDurationMs) {
        if (!bootPinUnlocked_) drawBootPin();
        else if (startupPreflightComplete_ || hardwareRecovery_) drawDashboard();
        else drawStartupWarningPage();
    }
    if (activePage_ == UiPage::LogoSaver &&
        static_cast<int32_t>(now - nextLogoSaverMoveMs_) >= 0) {
        logoSaverPosition_ = static_cast<uint8_t>((logoSaverPosition_ + 1) % 4);
        positionLogoSaver();
        nextLogoSaverMoveMs_ = now + kLogoSaverMoveMs;
    }
    if (!hardwareRecovery_ && !SetupPortal::active() && !displaySleeping_ && activePage_ != UiPage::BootSplash &&
        activePage_ != UiPage::LogoSaver &&
        activePage_ != UiPage::BootPin &&
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
    updateGpsPresentation(now);
    if (static_cast<int32_t>(now - nextDisplayUpdateMs_) >= 0) {
        nextDisplayUpdateMs_ = now + kUiUpdateMs;
        if(driveIcon_)updateDriveIcon(driveIcon_,safetyStatus==OutputSafetyStatus::Armed,
            hardwareRecovery_||safetyStatus==OutputSafetyStatus::InputFault||safetyStatus==OutputSafetyStatus::ConfigurationFault||safetyStatus==OutputSafetyStatus::ControlStale);
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
        else if (activePage_ == UiPage::Channel2) drawLiveIndicator(rawInputPermille);
        else if (activePage_ == UiPage::ThrottleResponse)
            refreshThrottleResponseGraph(rawInputPermille);
        else if (activePage_ == UiPage::InputMonitor) refreshInputMonitorRows();
    }
    const uint32_t renderedBefore = uiMetrics.rendered;
    // Make layout cost explicit for page transitions. LVGL would otherwise
    // perform pending layout inside its refresh handler. Do not force refresh.
    if (uiMetrics.pagePending) {
        const uint32_t layoutStarted = micros();
        lv_obj_update_layout(lv_scr_act());
        uiMetrics.layoutUs += uint32_t(micros()) - layoutStarted;
    }
    const uint32_t handlerStarted = micros();
    lv_timer_handler();
    uiMetrics.handlerUs += uint32_t(micros()) - handlerStarted;
    if (uiMetrics.rendered != renderedBefore) reportUiFrame();
    // Paint accepted touches first. Coalesce the complete model into one write,
    // only after the RF path acknowledges non-live output and the gesture ends.
    const bool outputSafe = safetyStatus != OutputSafetyStatus::Armed &&
                            safetyStatus != OutputSafetyStatus::LinkRequired &&
                            safetyStatus != OutputSafetyStatus::NeutralRequired;
    const bool saveDue = (settingsSavePending_ && int32_t(now-settingsSaveDueMs_) >= 0) ||
                         (channelParameterSavePending_ && int32_t(now-channelParameterSaveDueMs_) >= 0);
    if (!armRequested_ && outputSafe && !touchWasDown_ && !uiMetrics.touchPending &&
        !uiMetrics.pagePending && saveDue) {
        saveSettings();
        channelParameterSavePending_ = settingsSavePending_;
        channelParameterSaveDueMs_ = settingsSaveDueMs_;
    }

}

int TransmitterUi::screenWidth() const {return kScreenWidth;}
int TransmitterUi::screenHeight() const {return kScreenHeight;}
bool TransmitterUi::requestDisplayRotation() {
    if(!displayReady_||armRequested_||calibrationActive()||activePage_==UiPage::BootPin||
       pinPurpose_!=PinPurpose::None||remoteArmHold_.id())return false;
    rotationPending_=true;return true;
}
void TransmitterUi::applyDisplayRotation() {
    rotationPending_=false;
    if(armRequested_||calibrationActive()||remoteArmHold_.id())return;
    // Consume the gesture before changing its coordinate system.
    armGestureValid_=false;lastTouchDurationMs_=0;remoteArmHold_.cancel();
    lv_obj_clean(lv_scr_act());resetLvglObjects();
    portraitUi=!portraitUi;
    kScreenWidth=portraitUi?DISPLAY_LANDSCAPE_HEIGHT:DISPLAY_LANDSCAPE_WIDTH;
    kScreenHeight=portraitUi?DISPLAY_LANDSCAPE_WIDTH:DISPLAY_LANDSCAPE_HEIGHT;
    designFooterY=kScreenHeight-(portraitUi?60:40);
#ifdef JR_R1_SIMULATOR
    simulatorDisplayResize(kScreenWidth,kScreenHeight);
#else
    if(lcdTransportReady)display->setRotation(portraitUi?2:1);
#endif
    pixelCache.setDimensions(kScreenWidth,kScreenHeight);
    lv_disp_draw_buf_init(&drawBuffer,lineBuffer,nullptr,kScreenWidth*16);
    displayDriver.hor_res=kScreenWidth;displayDriver.ver_res=kScreenHeight;
    lv_disp_drv_update(registeredDisplay,&displayDriver);
    SetupPortal::resizeScreen(kScreenWidth,kScreenHeight);
    lastInteractionMs_=millis();redrawActivePage();
    Serial.printf("DISPLAY: rotated to %dx%d; reused framebuffer and draw buffer\n",kScreenWidth,kScreenHeight);
#ifndef JR_R1_SIMULATOR
    Serial.printf("ROTATION MEMORY: heap=%u largest_DMA=%u PSRAM=%u\n",unsigned(ESP.getFreeHeap()),unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_DMA)),unsigned(ESP.getFreePsram()));
#endif
}

#include "transmitter_ui_wifi.inc"

#include "transmitter_ui_mixing.inc"

#include "transmitter_ui_remote.inc"

#include "transmitter_ui_keypad.inc"
