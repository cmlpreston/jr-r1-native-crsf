#pragma once
#include <cstdint>
#include <cstring>

// Caller-owned RGB565 storage (PSRAM on hardware). Invalid rows are always sent.
// A row becomes valid only after a full-width write; partial writes cannot
// accidentally validate pixels that have never reached the panel.
template <int Width, int Height> class UiPixelCache {
public:
    bool setDimensions(int width,int height) {
        if(width<1||height<1||width*height>Width*Height||height>MaxDimension||width>MaxDimension)return false;
        width_=width;height_=height;invalidate();return true;
    }
    void attach(uint16_t *storage) { pixels_ = storage; invalidate(); }
    void invalidate() { memset(valid_, 0, sizeof(valid_)); }
    void setEnabled(bool enabled) { requested_ = enabled; invalidate(); }
    bool enabled() const { return pixels_ != nullptr && requested_; }
    bool unchanged(int x, int y, int width, const uint16_t *pixels) const {
        return enabled() && valid_[y] &&
            memcmp(pixels_ + y * width_ + x, pixels, width * sizeof(uint16_t)) == 0;
    }
    void remember(int x, int y, int width, const uint16_t *pixels) {
        if (!enabled()) return;
        memcpy(pixels_ + y * width_ + x, pixels, width * sizeof(uint16_t));
        if (x == 0 && width == width_) valid_[y] = true;
    }
private:
    bool requested_ = true;
    uint16_t *pixels_ = nullptr;
    static constexpr int MaxDimension=Width>Height?Width:Height;
    int width_=Width,height_=Height;
    bool valid_[MaxDimension] = {};
};
