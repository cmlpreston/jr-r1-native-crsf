#include "lcd_pixel_transport.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <random>
#include <initializer_list>
#include <sys/mman.h>
#include <unistd.h>

struct Sink {
    std::vector<uint8_t> bytes;
    uint32_t calls=0;
    void writePixels(const void *data,uint32_t countBytes){
        assert((reinterpret_cast<uintptr_t>(data)&3u)==0);
        assert(countBytes&&countBytes%4==0); // HAL must never need a partial word.
        const auto p=static_cast<const uint8_t *>(data);
        for(uint32_t n=0;n<countBytes;n+=2){bytes.push_back(p[n+1]);bytes.push_back(p[n]);}
        ++calls;
    }
    void write16(uint16_t value){bytes.push_back(value>>8);bytes.push_back(value&255);++calls;}
};

void check(const uint8_t *data,uint32_t count){
    Sink sink;uint32_t scratch=0;
    const auto calls=LcdPixelTransfer::writeNative(sink,data,count,scratch);
    assert(calls==sink.calls&&sink.bytes.size()==size_t(count)*2);
    for(uint32_t n=0;n<count;++n){assert(sink.bytes[n*2]==data[n*2+1]);assert(sink.bytes[n*2+1]==data[n*2]);}
    if(!(reinterpret_cast<uintptr_t>(data)&1u))assert(scratch==0);
}

int main(){
    Sink empty;uint32_t scratch=0;
    assert(LcdPixelTransfer::writeNative(empty,nullptr,0,scratch)==0&&empty.bytes.empty());
    std::mt19937 random(0x535049);
    for(unsigned trial=0;trial<4000;++trial){
        const uint32_t count=trial<100?trial:random()%20000;
        std::vector<uint32_t> aligned((size_t(count)*2+7)/4);
        auto data=reinterpret_cast<uint8_t *>(aligned.data())+(trial%4);
        for(uint32_t n=0;n<count*2;++n)data[n]=static_cast<uint8_t>(random());
        check(data,count);
    }
    // Place the last pixel immediately against an inaccessible page. A direct
    // odd-count HAL call would load one word past the buffer into that page.
    const size_t page=static_cast<size_t>(sysconf(_SC_PAGESIZE));
    void *memory=mmap(nullptr,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
    assert(memory!=MAP_FAILED);assert(mprotect(static_cast<uint8_t *>(memory)+page,page,PROT_NONE)==0);
    for(uint32_t count:{1u,2u,3u,31u,32u,33u,479u,480u,481u}){
        auto data=static_cast<uint8_t *>(memory)+page-count*2;
        for(uint32_t n=0;n<count*2;++n)data[n]=static_cast<uint8_t>(n*17);
        check(data,count);
    }
    assert(munmap(memory,page*2)==0);
    // Twenty 480x16 LVGL strips send the same 307,200 image bytes through twenty
    // outer SPIClass calls rather than the pinned 4,800 32-pixel calls. The HAL
    // still performs its own 64-byte FIFO transfers; this is not a timing claim.
    alignas(4) uint16_t strip[480*16]={};Sink frame;scratch=0;
    for(unsigned n=0;n<20;++n)LcdPixelTransfer::writeNative(frame,strip,480*16,scratch);
    assert(frame.calls==20&&frame.bytes.size()==480u*320u*2&&scratch==0);
    puts("LCD native pixels: byte order, byte lengths, alignments, odd/tail/guard-page reads and 4800->20 outer calls PASS");
}
