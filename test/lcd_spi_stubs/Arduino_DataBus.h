#pragma once
#include <stdint.h>
#include <stddef.h>
#define GFX_NOT_DEFINED -1
#define SPI_DEFAULT_FREQ 40000000
#define GFX_INLINE inline
class Arduino_DataBus {
public:
    virtual ~Arduino_DataBus()=default;
    virtual bool begin(int32_t=-1,int8_t=-1)=0;
    virtual void beginWrite()=0;
    virtual void endWrite()=0;
    virtual void writeCommand(uint8_t)=0;
    virtual void writeCommand16(uint16_t)=0;
    virtual void writeCommandBytes(uint8_t *,uint32_t)=0;
    virtual void write(uint8_t)=0;
    virtual void write16(uint16_t)=0;
    virtual void writeRepeat(uint16_t,uint32_t)=0;
    virtual void writeBytes(uint8_t *,uint32_t)=0;
    virtual void writePixels(uint16_t *,uint32_t)=0;
    virtual void writePattern(uint8_t *,uint8_t,uint32_t)=0;
protected:
    int32_t _speed=0;int8_t _dataMode=0;
};
