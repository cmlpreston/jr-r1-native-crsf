#pragma once
#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <utility>
#include <cassert>
#include <cstring>
#define SPI_HAS_TRANSACTION 1
#define MSBFIRST 1
#define SPI_MODE0 0
#define SPI_MODE3 3
#define OUTPUT 1
#define HIGH 1
#define LOW 0
extern uint32_t lcdFakeClock;
inline uint32_t micros(){return lcdFakeClock++;}
extern std::vector<std::pair<int,int>> lcdFakePins;
inline void pinMode(int pin,int){lcdFakePins.emplace_back(pin,2);}
inline void digitalWrite(int pin,int value){lcdFakePins.emplace_back(pin,value);}
struct SPISettings {
    uint32_t speed=0;uint8_t order=0,mode=0;
    SPISettings()=default;
    SPISettings(uint32_t s,uint8_t o,uint8_t m):speed(s),order(o),mode(m){}
};
struct LcdFakeDevice;
struct LcdFakeCommandBit {
    LcdFakeDevice *owner=nullptr;bool send=false;
    void operator=(unsigned value);
    operator unsigned()const{return 0;}
};
struct LcdFakeDevice {
    struct {unsigned wr_bit_order=0;}ctrl;
    struct {unsigned usr_mosi_dbitlen=0;}mosi_dlen;
    struct {unsigned usr_miso_dbitlen=0;}miso_dlen;
    uint32_t data_buf[16]={};
    struct {LcdFakeCommandBit update,usr;}cmd;
    std::vector<uint8_t> wire;
    uint32_t fifoCalls=0;
    LcdFakeDevice(){cmd.update.owner=cmd.usr.owner=this;cmd.usr.send=true;}
};
inline void LcdFakeCommandBit::operator=(unsigned value){
    if(!value||!send)return;
    const auto data=reinterpret_cast<const uint8_t *>(owner->data_buf);
    const unsigned count=(owner->mosi_dlen.usr_mosi_dbitlen+1)/8;
    assert(count&&count<=64);
    owner->wire.insert(owner->wire.end(),data,data+count);
    ++owner->fifoCalls;lcdFakeClock+=10;
}
struct spi_t {LcdFakeDevice *dev;};
void spiWritePixelsNL(spi_t *,const void *,uint32_t);
void spiWriteNL(spi_t *,const void *,uint32_t);
void spiWriteShortNL(spi_t *,uint16_t);
class SPIClass {
public:
    LcdFakeDevice device;
    spi_t spi{&device};
    SPISettings settings;
    int sck=-2,miso=-2,mosi=-2;
    unsigned begins=0,ends=0,pixelApiCalls=0,byteApiCalls=0,shortApiCalls=0;
    void begin(int a,int b,int c){sck=a;miso=b;mosi=c;}
    void beginTransaction(SPISettings s){settings=s;++begins;}
    void endTransaction(){++ends;}
    void writePixels(const void *data,uint32_t byteCount){
        assert((reinterpret_cast<uintptr_t>(data)&3u)==0&&byteCount%4==0);
        ++pixelApiCalls;spiWritePixelsNL(&spi,data,byteCount);
    }
    void writeBytes(const uint8_t *data,uint32_t byteCount){++byteApiCalls;spiWriteNL(&spi,data,byteCount);}
    void write16(uint16_t value){++shortApiCalls;spiWriteShortNL(&spi,value);}
    uint8_t transfer(uint8_t value){device.wire.push_back(value);return value;}
    void writePattern(uint8_t *data,uint8_t count,uint32_t repeat){while(repeat--)writeBytes(data,count);}
    void clear(){device.wire.clear();device.fifoCalls=pixelApiCalls=byteApiCalls=shortApiCalls=0;}
};
extern SPIClass SPI;
