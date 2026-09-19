#pragma once
#include <cstdint>
#include <cstring>

// Caller-owned RGB565 storage (PSRAM on hardware). Invalid rows are always sent.
// A row becomes valid only after a full-width write; partial writes cannot
// accidentally validate pixels that have never reached the panel.
template <int Width, int Height> class UiPixelCache {
public:
    void attach(uint16_t *storage) { pixels_ = storage; invalidate(); }
    void invalidate() { memset(valid_, 0, sizeof(valid_)); }
    bool enabled() const { return pixels_ != nullptr; }
    bool unchanged(int x, int y, int width, const uint16_t *pixels) const {
        return pixels_ && valid_[y] &&
            memcmp(pixels_ + y * Width + x, pixels, width * sizeof(uint16_t)) == 0;
    }
    void remember(int x, int y, int width, const uint16_t *pixels) {
        if (!pixels_) return;
        memcpy(pixels_ + y * Width + x, pixels, width * sizeof(uint16_t));
        if (x == 0 && width == Width) valid_[y] = true;
    }
private:
    uint16_t *pixels_ = nullptr;
    bool valid_[Height] = {};
};
