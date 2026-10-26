// PCB-specific colour-cycle test, never linked into normal transmitter builds.
#if defined(JR_R1_PCB_LCD_TEST)
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Arduino_GFX_Library.h>
#include "display_profile.h"
#include "pcb_io.h"

static_assert(TFT_DC_PIN == 4 && TFT_SCK_PIN == 5 && TFT_MOSI_PIN == 6 &&
              TFT_CS_PIN == -1 && TFT_MISO_PIN == -1 && TFT_BACKLIGHT_PIN == 3,
              "LCD test requires XIAO schematic (5) grounded-CS wiring");
static_assert(DISPLAY_PROFILE == DISPLAY_PROFILE_WAVESHARE_RPI_35F,
              "Only the installed Waveshare LCD (F) is supported");
#include "waveshare_startup.inc"

namespace {
SPIClass lcdSpi(FSPI);
Arduino_HWSPI bus(TFT_DC_PIN, TFT_CS_PIN, TFT_SCK_PIN, TFT_MOSI_PIN,
                  TFT_MISO_PIN, &lcdSpi);
class TestDisplay : public Arduino_ST7796 {
public:
    TestDisplay() : Arduino_ST7796(&bus, -1, 2, true, 320, 480) {}
    bool begin(int32_t speed) override {
        resetReady = false;
        if (!pcbControls.setOutput(PCB_LCD_RESET_PORT, false)) return false;
        return Arduino_ST7796::begin(speed) && resetReady;
    }
protected:
    void tftInit() override {
        _bus->beginWrite();
        _bus->endWrite(); // Establish mode-3 idle clock while RESET is low.
        delay(500);
        resetReady = pcbControls.writeOutput(PCB_LCD_RESET_PORT, true);
        Serial.printf("LCD TEST: reset release %s\n", resetReady ? "ACK" : "FAILED");
        if (!resetReady) return;
        delay(500);
        _bus->sendCommand(ST7796_SWRESET);
        delay(500);
        _bus->batchOperation(waveshare_startup_operations, sizeof(waveshare_startup_operations));
        invertDisplay(false);
    }
private:
    bool resetReady = false;
} lcd;
bool ready = false;
uint32_t cycle = 0;
void colourFrame(const char *name, uint16_t colour, uint16_t ink) {
    Serial.printf("LCD TEST: cycle=%lu %s START\n", static_cast<unsigned long>(cycle), name);
    const uint32_t started = millis();
    lcd.fillScreen(colour);
    lcd.setTextColor(ink);
    lcd.setTextSize(3);
    lcd.setCursor(16, 30);
    lcd.println("R-1 LCD TEST");
    lcd.setCursor(16, 80);
    lcd.println(name);
    lcd.setTextSize(2);
    lcd.setCursor(16, 125);
    lcd.println("1 MHz / MODE 3");
    lcd.drawRect(2, 2, 316, 476, ink);
    lcd.drawLine(0, 0, 319, 479, ink);
    lcd.drawLine(319, 0, 0, 479, ink);
    Serial.printf("LCD TEST: %s DONE in %lums\n", name, static_cast<unsigned long>(millis()-started));
    delay(2500);
}
}

void setup() {
    // This build sends no RF/RC data and never starts the transmitter control task.
    pinMode(44, INPUT);
    pinMode(43, INPUT);
    Serial.begin(115200);
    delay(1500);
    Serial.println("\nR-1 XIAO PCB LCD-ONLY TEST; no RF/control/Wi-Fi/LVGL/NVS");
    Serial.printf("LCD TEST: SCK=%d MOSI=%d DC=%d CS=GND BL=%d RESET=K2 GPB3 SPI=%lu\n",
                  TFT_SCK_PIN, TFT_MOSI_PIN, TFT_DC_PIN, TFT_BACKLIGHT_PIN,
                  static_cast<unsigned long>(TFT_SPI_FREQUENCY));
    ledcSetup(TFT_BACKLIGHT_LEDC_CHANNEL, TFT_BACKLIGHT_PWM_FREQUENCY, TFT_BACKLIGHT_PWM_BITS);
    ledcAttachPin(TFT_BACKLIGHT_PIN, TFT_BACKLIGHT_LEDC_CHANNEL);
    ledcWrite(TFT_BACKLIGHT_LEDC_CHANNEL, 255);
    Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);
    Wire.setClock(100000);
    // K2 alone is required for LCD reset. Keep touch in reset to isolate it;
    // missing touch or K1 must not stop this standalone display test.
    for (unsigned attempt=1; attempt<=5; ++attempt) {
        ready = pcbControls.configure(0x767f, 0x761f, 0x8000);
        Serial.printf("LCD TEST: K2 attempt=%u %s\n", attempt, ready ? "ACK" : "FAILED");
        if (ready) break;
        delay(200);
    }
    if (ready) ready = lcd.begin(TFT_SPI_FREQUENCY);
    Serial.printf("LCD TEST: init %s; backlight forced 100%%; touch held reset\n", ready ? "sent" : "FAILED");
}

void loop() {
    if (!ready) {
        Serial.println("LCD TEST: stopped; K2/reset failed. Repair and reboot.");
        delay(2000);
        return;
    }
    ++cycle;
    colourFrame("RED", RGB565_RED, RGB565_WHITE);
    colourFrame("GREEN", RGB565_GREEN, RGB565_BLACK);
    colourFrame("BLUE", RGB565_BLUE, RGB565_WHITE);
    colourFrame("WHITE", RGB565_WHITE, RGB565_BLACK);
    colourFrame("BLACK", RGB565_BLACK, RGB565_WHITE);
}
#endif
