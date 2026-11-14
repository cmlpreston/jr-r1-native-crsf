// This harness is built with the actual pinned Arduino_HWSPI.cpp, and generated
// inclusions of the installed ESP32 HAL functions. Only registers/GPIO are fake.
#include <SPI.h>
#include "xiao_lcd_pixel_bus.h"
#include <cstdio>
#include <random>
#include <initializer_list>
#include <sys/mman.h>
#include <unistd.h>

uint32_t lcdFakeClock=0;
std::vector<std::pair<int,int>> lcdFakePins;
SPIClass SPI;

int main(){
    SPIClass spi;XiaoLcdPixelBus bus(4,-1,5,6,-1,&spi);
    assert(bus.begin(80000000,SPI_MODE3));
    bus.beginWrite();bus.endWrite();
    assert(spi.sck==5&&spi.mosi==6&&spi.miso==-1);
    assert(spi.settings.speed==80000000&&spi.settings.mode==SPI_MODE3&&spi.settings.order==MSBFIRST);
    assert(spi.begins==1&&spi.ends==1);
    for(auto pin:lcdFakePins)assert(pin.first==4); // Grounded CS causes no GPIO writes.
    assert(bus.snapshot().supported&&!bus.snapshot().native);
    std::mt19937 random(0x7796);
    for(unsigned trial=0;trial<1400;++trial){
        const uint32_t count=trial<100?trial:random()%16000;
        std::vector<uint32_t> words((count*2+7)/4);
        auto *data=reinterpret_cast<uint8_t *>(words.data())+(trial%4);
        for(uint32_t n=0;n<count*2;++n)data[n]=static_cast<uint8_t>(random());
        std::vector<uint8_t> reference;
        for(uint32_t n=0;n<count;++n){reference.push_back(data[n*2+1]);reference.push_back(data[n*2]);}
        // The original library assumes uint16 alignment. Compare both paths
        // there; native also handles byte-unaligned caller data safely.
        for(bool native:{false,true}){
            if(!native&&(trial%2))continue;
            spi.clear();bus.setNative(native);bus.beginWrite();
            bus.writePixels(reinterpret_cast<uint16_t *>(data),count);bus.endWrite();
            assert(spi.device.wire==reference);
            const auto stats=bus.snapshot();
            assert(stats.native==native&&stats.pixels==count&&stats.bytes==uint64_t(count)*2);
            assert(stats.backendCalls==spi.pixelApiCalls+spi.byteApiCalls+spi.shortApiCalls);
            assert(stats.pixelCalls==(count?1u:0u));
        }
    }
    // The last odd pixel sits immediately before an inaccessible page.
    const size_t page=static_cast<size_t>(sysconf(_SC_PAGESIZE));
    auto memory=static_cast<uint8_t *>(mmap(nullptr,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0));
    assert(memory!=MAP_FAILED);assert(mprotect(memory+page,page,PROT_NONE)==0);
    for(unsigned count:{1u,3u,31u,33u,479u}){
        auto data=memory+page-count*2;
        for(unsigned n=0;n<count*2;++n)data[n]=static_cast<uint8_t>(n*31);
        bus.setNative(true);spi.clear();bus.writePixels(reinterpret_cast<uint16_t *>(data),count);
        assert(spi.device.wire.size()==count*2);
        for(unsigned n=0;n<count;++n){assert(spi.device.wire[n*2]==data[n*2+1]);assert(spi.device.wire[n*2+1]==data[n*2]);}
    }
    assert(munmap(memory,page*2)==0);
    // Explicit odd-width partial rectangles with a larger source stride. The
    // unchanged driver may issue one span per clipped row, and may use a source
    // pointer two bytes off word alignment. Address/command GPIO is inherited.
    alignas(4) uint16_t bitmap[27*9];
    for(unsigned n=0;n<27*9;++n)bitmap[n]=static_cast<uint16_t>(n*983);
    for(unsigned x:{0u,1u,4u})for(unsigned width:{1u,7u,19u}){
        std::vector<uint8_t> oldBytes;
        std::vector<std::pair<int,int>> oldPins;
        for(bool native:{false,true}){
            bus.setNative(native);spi.clear();lcdFakePins.clear();
            bus.beginWrite();bus.writeCommand(0x2a);bus.write16(x);bus.write16(x+width-1);
            bus.writeCommand(0x2b);bus.write16(2);bus.write16(6);bus.writeCommand(0x2c);
            for(unsigned y=2;y<7;++y)bus.writePixels(bitmap+y*27+x,width);
            bus.endWrite();
            if(!native){oldBytes=spi.device.wire;oldPins=lcdFakePins;}
            else {assert(spi.device.wire==oldBytes);assert(lcdFakePins==oldPins);}
        }
    }
    // Compare complete 480x320 and 320x480 images split into 16-row buffers,
    // then partial/odd rectangles and isolated row-cache runs in both modes.
    for(unsigned width:{320u,480u}){
        const unsigned height=width==480?320:480;
        std::vector<uint32_t> frame(width*height/2);
        auto *data=reinterpret_cast<uint16_t *>(frame.data());
        for(unsigned n=0;n<width*height;++n)data[n]=static_cast<uint16_t>(random());
        std::vector<uint8_t> oldImage;
        for(bool native:{false,true}){
            bus.setNative(native);spi.clear();
            for(unsigned y=0;y<height;y+=16){
                bus.beginWrite();bus.writePixels(data+y*width,width*16);bus.endWrite();
            }
            if(!native)oldImage=spi.device.wire;else assert(spi.device.wire==oldImage);
            const auto stats=bus.snapshot();
            assert(stats.backendCalls==(native?height/16:4800));
            assert(spi.device.fifoCalls==4800); // No DMA/FIFO-transfer reduction claim.
            assert(stats.elapsedUs&&stats.maxWriteUs);
        }
    }
    bus.setNative(true);spi.clear();lcdFakeClock=0xfffffff0U;
    alignas(4) uint16_t strip[480]={};bus.writePixels(strip,480);
    assert(bus.snapshot().elapsedUs==151&&bus.snapshot().maxWriteUs==151);
    bus.resetStats();assert(bus.snapshot().native&&bus.snapshot().pixels==0&&bus.snapshot().elapsedUs==0);
    bus.setNative(false);assert(!bus.snapshot().native&&bus.snapshot().backendCalls==0);
    puts("Pinned LCD bus/HAL: byte-exact legacy/native, guarded odd/alignment reads, partial rectangles/commands, panel setup/transactions, 4800->20/30 API calls with unchanged FIFO count, timing wrap PASS");
}
