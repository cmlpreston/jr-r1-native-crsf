#include "simulator_platform.h"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace {
constexpr int kScale = 2;
SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;
SDL_Texture *texture = nullptr;
int16_t screenWidth = 0;
int16_t screenHeight = 0;
std::vector<uint32_t> framebuffer;
bool quitRequested = false;
bool touchDown = false;
bool displayEnabled = true;
uint8_t brightnessPercent = 100;
int16_t touchX = 0;
int16_t touchY = 0;
SimulatorControls controls;
bool forwardPressed = false;
bool brakePressed = false;

void updateThrottle()
{
    controls.throttlePermille = forwardPressed ? 1000 : (brakePressed ? -1000 : 0);
}

void present()
{
    if (renderer == nullptr || texture == nullptr) return;
    SDL_UpdateTexture(texture, nullptr, framebuffer.data(),
                      screenWidth * static_cast<int>(sizeof(uint32_t)));
    SDL_SetRenderDrawColor(renderer, 8, 10, 14, 255);
    SDL_RenderClear(renderer);
    if (displayEnabled) {
        SDL_Rect destination {0, 0, screenWidth * kScale, screenHeight * kScale};
        SDL_RenderCopy(renderer, texture, nullptr, &destination);
    }
    SDL_RenderPresent(renderer);
}

void printControls()
{
    std::puts(
        "Simulator controls:\n"
        "  mouse      touch the transmitter screen\n"
        "  W / S      full forward / full brake while held\n"
        "  L          toggle ELRS link\n"
        "  [ / ]      lower / raise receiver voltage\n"
        "  F12        save simulator/screenshot.bmp\n"
        "  Esc        quit");
}
}

bool simulatorDisplayBegin(int16_t width, int16_t height)
{
    screenWidth = width;
    screenHeight = height;
    framebuffer.assign(static_cast<size_t>(width) * height, 0xff000000U);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    window = SDL_CreateWindow("JR R1 native CRSF simulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width * kScale, height * kScale, SDL_WINDOW_ALLOW_HIGHDPI);
    renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr)
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, width, height);
    if (window == nullptr || renderer == nullptr || texture == nullptr) {
        std::fprintf(stderr, "SDL display creation failed: %s\n", SDL_GetError());
        simulatorShutdown();
        return false;
    }
    SDL_RenderSetLogicalSize(renderer, width * kScale, height * kScale);
    printControls();
    return true;
}

void simulatorShutdown()
{
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    texture = nullptr;
    renderer = nullptr;
    window = nullptr;
    SDL_Quit();
}

void simulatorPollEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                quitRequested = true;
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    touchDown = event.type == SDL_MOUSEBUTTONDOWN;
                    touchX = constrain<int16_t>(event.button.x / kScale, 0, screenWidth - 1);
                    touchY = constrain<int16_t>(event.button.y / kScale, 0, screenHeight - 1);
                }
                break;
            case SDL_MOUSEMOTION:
                touchX = constrain<int16_t>(event.motion.x / kScale, 0, screenWidth - 1);
                touchY = constrain<int16_t>(event.motion.y / kScale, 0, screenHeight - 1);
                break;
            case SDL_KEYDOWN:
                if (event.key.repeat) break;
                if (event.key.keysym.sym == SDLK_ESCAPE) quitRequested = true;
                else if (event.key.keysym.sym == SDLK_w) forwardPressed = true;
                else if (event.key.keysym.sym == SDLK_s) brakePressed = true;
                else if (event.key.keysym.sym == SDLK_l) {
                    controls.linkValid = !controls.linkValid;
                    std::printf("SIM: ELRS link %s\n", controls.linkValid ? "connected" : "lost");
                } else if (event.key.keysym.sym == SDLK_LEFTBRACKET) {
                    controls.receiverVoltageDecivolts = std::max<uint16_t>(0, controls.receiverVoltageDecivolts - 1);
                } else if (event.key.keysym.sym == SDLK_RIGHTBRACKET) {
                    ++controls.receiverVoltageDecivolts;
                } else if (event.key.keysym.sym == SDLK_F12) {
                    simulatorSaveScreenshot("screenshot.bmp");
                }
                updateThrottle();
                break;
            case SDL_KEYUP:
                if (event.key.keysym.sym == SDLK_w) forwardPressed = false;
                else if (event.key.keysym.sym == SDLK_s) brakePressed = false;
                updateThrottle();
                break;
        }
    }
}

bool simulatorQuitRequested() { return quitRequested; }
const SimulatorControls &simulatorControls() { return controls; }
bool simulatorTouchDown() { return touchDown; }

void simulatorFlush(const lv_area_t *area, const lv_color_t *pixels)
{
    size_t source = 0;
    for (int16_t y = area->y1; y <= area->y2; ++y) {
        for (int16_t x = area->x1; x <= area->x2; ++x, ++source) {
            const uint32_t converted = lv_color_to32(pixels[source]);
            const uint8_t scale = displayEnabled ? brightnessPercent : 0;
            const uint8_t red = ((converted >> 16U) & 0xffU) * scale / 100U;
            const uint8_t green = ((converted >> 8U) & 0xffU) * scale / 100U;
            const uint8_t blue = (converted & 0xffU) * scale / 100U;
            framebuffer[static_cast<size_t>(y) * screenWidth + x] =
                0xff000000U | (static_cast<uint32_t>(red) << 16U) |
                (static_cast<uint32_t>(green) << 8U) | blue;
        }
    }
    present();
}

bool simulatorReadTouch(int16_t &x, int16_t &y, uint16_t &rawX,
                        uint16_t &rawY, uint16_t &rawZ)
{
    x = touchX;
    y = touchY;
    rawX = static_cast<uint16_t>(touchX);
    rawY = static_cast<uint16_t>(touchY);
    rawZ = touchDown ? 1000 : 0;
    return touchDown;
}

void simulatorSetDisplayEnabled(bool enabled)
{
    displayEnabled = enabled;
    present();
}

void simulatorSetBrightness(uint8_t percent)
{
    brightnessPercent = constrain<uint8_t>(percent, 0, 100);
}

void simulatorBeep(uint16_t pitchHz, uint16_t durationMs)
{
    std::printf("SIM: beep %u Hz for %u ms\n", pitchHz, durationMs);
}

void simulatorStopBeep() {}

void simulatorWriteScreenshot(Stream &output)
{
    for (uint32_t pixel : framebuffer) {
        const uint8_t bytes[] = {
            static_cast<uint8_t>(pixel >> 16U),
            static_cast<uint8_t>(pixel >> 8U),
            static_cast<uint8_t>(pixel),
        };
        output.write(bytes, sizeof(bytes));
    }
}

bool simulatorSaveScreenshot(const char *path)
{
    if (framebuffer.empty()) return false;
    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(
        framebuffer.data(), screenWidth, screenHeight, 32,
        screenWidth * static_cast<int>(sizeof(uint32_t)),
        0x00ff0000U, 0x0000ff00U, 0x000000ffU, 0xff000000U);
    const bool saved = surface != nullptr && SDL_SaveBMP(surface, path) == 0;
    if (surface) SDL_FreeSurface(surface);
    std::printf("SIM: screenshot %s %s\n", saved ? "saved" : "failed", path);
    return saved;
}
