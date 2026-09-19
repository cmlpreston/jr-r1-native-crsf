#include "ui_pixel_cache.h"
#include <cassert>
#include <cstdio>
int main() {
    UiPixelCache<4, 3> cache;
    uint16_t storage[12] = {}, a[] = {1, 2, 3, 4}, b[] = {1, 2, 9, 4};
    assert(!cache.unchanged(0, 0, 4, a));
    cache.remember(0, 0, 4, a); // Allocation failure fallback.
    cache.attach(storage);
    cache.remember(1, 1, 2, a);
    assert(!cache.unchanged(1, 1, 2, a));
    cache.remember(0, 1, 4, a);
    assert(cache.unchanged(0, 1, 4, a));
    assert(!cache.unchanged(0, 1, 4, b));
    cache.remember(2, 1, 1, b + 2);
    assert(cache.unchanged(0, 1, 4, b));
    assert(!cache.unchanged(0, 0, 4, b));
    cache.invalidate();
    assert(!cache.unchanged(0, 1, 4, b));
    cache.remember(0, 1, 4, a);
    cache.setEnabled(false);
    assert(!cache.enabled() && !cache.unchanged(0, 1, 4, a));
    cache.remember(0, 1, 4, b);
    cache.setEnabled(true);
    assert(cache.enabled() && !cache.unchanged(0, 1, 4, a));
    cache.remember(0, 1, 4, b);
    assert(cache.unchanged(0, 1, 4, b));
    puts("UI pixel cache PASS");
}
