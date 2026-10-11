#include "offline_map.h"
#include <cstdlib>
#if defined(ARDUINO) && !defined(JR_R1_SIMULATOR)
#include <esp_heap_caps.h>
#endif
namespace OfflineMap {
static Package loaded;
static uint32_t generation = 0;
uint32_t revision() { return generation; }
static uint16_t *image = nullptr;
const uint16_t *pixels() { return image; }
uint16_t *allocatePixels(unsigned count) {
    if(!count||count>512U*512U)return nullptr;
#if defined(ARDUINO) && !defined(JR_R1_SIMULATOR)
    return static_cast<uint16_t*>(heap_caps_malloc(count*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
#else
    return static_cast<uint16_t*>(malloc(count*2));
#endif
}
const Package &current() { return loaded; }
void install(const Package &map, uint16_t *ownedPixels) { free(image);image=ownedPixels;loaded=map;++generation; }
bool covers(int32_t lat, int32_t lon) {
    return loaded.valid && lat >= loaded.south-10000 && lat <= loaded.north+10000 &&
        lon >= loaded.west-10000 && lon <= loaded.east+10000;
}
}
