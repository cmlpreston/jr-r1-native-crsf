#include "simulator_platform.h"
#include <emscripten.h>
#include "control_assignment.h"
#include <deque>
#include <vector>

namespace {
int16_t width = 0, height = 0, touchX = 0, touchY = 0;
std::vector<uint8_t> pixels;
SimulatorControls controls;
std::deque<std::pair<uint16_t, int16_t>> actions;
bool pressed = false, releasePending = false, enabled = true, dirty = true;
uint32_t pressedMs = 0;
uint8_t brightness = 100;

EM_JS(void, present, (const uint8_t *data, int w, int h, int light), {
    const canvas = Module['canvas'];
    if (canvas.width !== w || canvas.height !== h) { canvas.width = w; canvas.height = h; }
    const ctx = canvas.getContext('2d', {alpha: false});
    const frame = new Uint8ClampedArray(HEAPU8.subarray(data, data + w * h * 4));
    if (light !== 100) for (let i = 0; i < frame.length; i += 4) {
        frame[i] = frame[i] * light / 100;
        frame[i+1] = frame[i+1] * light / 100;
        frame[i+2] = frame[i+2] * light / 100;
    }
    ctx.putImageData(new ImageData(frame, w, h), 0, 0);
});
}

extern "C" {
EMSCRIPTEN_KEEPALIVE void demoTouch(int x, int y, int down) {
    touchX = constrain(x, 0, width - 1); touchY = constrain(y, 0, height - 1);
    if (down) {
        if (!pressed) pressedMs = millis();
        pressed = true; releasePending = false;
    } else releasePending = true;
}
EMSCRIPTEN_KEEPALIVE void demoInput(int kind, int value) {
    switch (kind) {
        case 0: controls.steeringPermille = constrain(value, -1000, 1000); break;
        case 1: controls.throttlePermille = constrain(value, -1000, 1000); break;
        case 2: controls.linkValid = value != 0; break;
        case 3: controls.receiverVoltageDecivolts = constrain(value, 0, 200); break;
        case 4: controls.transmitterVoltageDecivolts = constrain(value, 0, 200); break;
        case 5: controls.vehicleTelemetry = value != 0; break;
    }
}
EMSCRIPTEN_KEEPALIVE void demoAction(int function, int value) {
    if (function >= 0 && function < static_cast<int>(TransmitterFunction::Count) && actions.size() < 64)
        actions.emplace_back(function, value);
}
}

bool simulatorDisplayBegin(int16_t w, int16_t h) {
    width = w; height = h;
    pixels.assign(static_cast<size_t>(w) * h * 4, 0);
    for (size_t i = 3; i < pixels.size(); i += 4) pixels[i] = 255;
    return true;
}
void simulatorPollEvents() {
    // Preserve quick taps until LVGL has had a chance to poll the pointer.
    if (releasePending && millis() - pressedMs >= 85) { pressed = false; releasePending = false; }
}
bool simulatorQuitRequested() { return false; }
void simulatorShutdown() {}
const SimulatorControls &simulatorControls() { return controls; }
bool simulatorTouchDown() { return pressed; }
bool simulatorReadTouch(int16_t &x, int16_t &y, uint16_t &rx, uint16_t &ry, uint16_t &rz) {
    x = touchX; y = touchY; rx = x; ry = y; rz = pressed ? 1000 : 0;
    return pressed;
}
void simulatorFlush(const lv_area_t *area, const lv_color_t *colors) {
    size_t source = 0;
    for (int y = area->y1; y <= area->y2; ++y) for (int x = area->x1; x <= area->x2; ++x, ++source) {
        if (x < 0 || y < 0 || x >= width || y >= height) continue;
        const uint32_t color = lv_color_to32(colors[source]);
        const size_t index = (static_cast<size_t>(y) * width + x) * 4;
        pixels[index] = color >> 16; pixels[index+1] = color >> 8; pixels[index+2] = color;
    }
    dirty = true;
}
void simulatorSetDisplayEnabled(bool value) { enabled = value; dirty = true; }
void simulatorSetBrightness(uint8_t value) { brightness = constrain<int>(value, 0, 100); dirty = true; }
void simulatorBeep(uint16_t, uint16_t) {}
void simulatorStopBeep() {}
void simulatorWriteScreenshot(Stream &output) {
    for (size_t i = 0; i < pixels.size(); i += 4) output.write(&pixels[i], 3);
}
bool simulatorSaveScreenshot(const char *) { return false; }
bool simulatorConsumeAction(uint16_t &function, int16_t &value) {
    if (actions.empty()) return false;
    function = actions.front().first; value = actions.front().second;
    actions.pop_front(); return true;
}
void simulatorServiceBridge() {
    if (dirty) { present(pixels.data(), width, height, enabled ? brightness : 0); dirty = false; }
}
