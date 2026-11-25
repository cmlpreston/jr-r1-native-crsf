#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace LcdPixelTransfer {
struct Snapshot {
    bool supported=false, native=false;
    uint32_t pixelCalls=0, backendCalls=0, scratchPixels=0, maxWriteUs=0;
    uint64_t pixels=0, bytes=0, elapsedUs=0;
};

// UI-owner diagnostics. Counts cover writePixels only, excluding address-window
// commands/repeats. backendCalls counts SPIClass calls, not 64-byte FIFO loads.
// Legacy counts follow the pinned GFX 32-pixel loop; native counts are direct.
Snapshot snapshot();

// SPIClass::writePixels takes BYTE length. Its pinned ESP32 HAL loads uint32_t
// words (including a partial final word), so never pass an unaligned pointer or
// an odd number of pixels directly. No allocation, DMA or full-frame copy.
template<class Spi>
uint32_t writeNative(Spi &spi,const void *pixels,uint32_t count,uint32_t &scratchPixels) {
    const auto *data=static_cast<const uint8_t *>(pixels);
    uint32_t calls=0;
    if(!count)return calls;
    if(reinterpret_cast<uintptr_t>(data)&1u){
        // An odd byte address can never become word-aligned by adding pixels.
        // This is unusual for LVGL, but keep the API safe for other bitmaps.
        alignas(4) uint32_t scratch[16];
        while(count>=2){
            const uint32_t block=count>32?32:(count&~uint32_t(1));
            memcpy(scratch,data,size_t(block)*2);
            spi.writePixels(scratch,block*2);++calls;scratchPixels+=block;
            data+=block*2;count-=block;
        }
    }else{
        if((reinterpret_cast<uintptr_t>(data)&3u)&&count){
            uint16_t pixel;memcpy(&pixel,data,2);spi.write16(pixel);++calls;
            data+=2;--count;
        }
        const uint32_t even=count&~uint32_t(1);
        if(even){spi.writePixels(data,even*2);++calls;data+=even*2;count-=even;}
    }
    if(count){uint16_t pixel;memcpy(&pixel,data,2);spi.write16(pixel);++calls;}
    return calls;
}
} // namespace LcdPixelTransfer
