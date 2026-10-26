#pragma once
#include <stdint.h>

struct WaveshareTouchPoint { int16_t x, y; };
// GT911 native coordinates are 320x480. Landscape rotation 1 is 90 degrees
// anticlockwise from the installed portrait rotation 2; clamp edge noise.
inline WaveshareTouchPoint waveshareTouchPoint(uint16_t rawX, uint16_t rawY, bool portrait)
{
    const int16_t x = rawX > 319 ? 319 : rawX;
    const int16_t y = rawY > 479 ? 479 : rawY;
    return portrait ? WaveshareTouchPoint{int16_t(319-x), int16_t(479-y)} :
                      WaveshareTouchPoint{y, int16_t(319-x)};
}
