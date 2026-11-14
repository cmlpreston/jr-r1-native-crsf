#pragma once

#include <databus/Arduino_HWSPI.h>
#include "lcd_pixel_transport.h"

// Only the approved XIAO HWSPI construction uses this subclass. The verified
// base still owns pins, mode, begin/end transactions, commands and reset setup.
class XiaoLcdPixelBus final : public Arduino_HWSPI {
public:
    XiaoLcdPixelBus(int8_t dc,int8_t cs,int8_t sck,int8_t mosi,int8_t miso,SPIClass *spi)
        :Arduino_HWSPI(dc,cs,sck,mosi,miso,spi),spi_(*spi) {stats_.supported=true;}
    void writePixels(uint16_t *data,uint32_t count) override {
        if(!count)return;
        const uint32_t started=micros();
        ++stats_.pixelCalls;stats_.pixels+=count;stats_.bytes+=uint64_t(count)*2;
        if(stats_.native)stats_.backendCalls+=LcdPixelTransfer::writeNative(spi_,data,count,stats_.scratchPixels);
        else {
            Arduino_HWSPI::writePixels(data,count);
#if defined(LITTLE_FOOT_PRINT)
            stats_.backendCalls+=count*2;
#else
            stats_.backendCalls+=count/SPI_MAX_PIXELS_AT_ONCE+(count%SPI_MAX_PIXELS_AT_ONCE!=0);
#endif
        }
        const uint32_t elapsed=uint32_t(micros())-started;
        stats_.elapsedUs+=elapsed;
        if(elapsed>stats_.maxWriteUs)stats_.maxWriteUs=elapsed;
    }
    void setNative(bool enabled){stats_.native=enabled;resetStats();}
    void resetStats(){const bool native=stats_.native;stats_={};stats_.supported=true;stats_.native=native;}
    LcdPixelTransfer::Snapshot snapshot() const{return stats_;}
private:
    SPIClass &spi_;
    // Boot always restores the verified transport. The native path is an A/B
    // candidate until real panel/cycle/input measurements justify a default.
    LcdPixelTransfer::Snapshot stats_;
};
