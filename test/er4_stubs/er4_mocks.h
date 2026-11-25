#pragma once
#include <cstdint>
#include <cassert>
#include <vector>
#define ICACHE_RAM_ATTR
struct crsf_header_t { uint8_t sync_byte,frame_size,type; };
constexpr uint8_t CRSF_ADDRESS_RADIO_TRANSMITTER=0xea;
enum Connection {connected,disconnected,wifiUpdate};
inline Connection connectionState=disconnected;
inline bool connectionHasModelMatch=false,OtaIsFullRes=false,isArmed=false;
enum SwitchMode {smWideOr8ch,smHybridOr16ch};
inline SwitchMode OtaSwitchModeCurrent=smWideOr8ch;
inline uint32_t ChannelData[16]={};
inline uint32_t mockNow=0;inline int irqDepth=0;
inline uint32_t millis(){return mockNow;}
inline void noInterrupts(){++irqDepth;}
inline void interrupts(){assert(irqDepth>0);--irqDepth;}
struct RxConfig{};inline RxConfig config;
struct CRSFRouter {
 std::vector<std::vector<uint8_t>> replies;
 void deliverMessageTo(uint8_t address,const crsf_header_t *f){assert(address==0xea);const auto p=reinterpret_cast<const uint8_t*>(f);replies.emplace_back(p,p+f->frame_size+2);}
};
inline CRSFRouter crsfRouter;
struct RXOTAConnector {unsigned queued=0,discarded=0;uint8_t GetFifoFullPct()const{return queued?1:0;}void discardQueued(){queued=0;++discarded;}};
inline RXOTAConnector otaConnector;
struct StubbornSender {bool active=false;unsigned resets=0,resyncs=0;bool IsActive()const{return active;}void ResetState(){active=false;++resets;}bool observed=false;void RequestResync(bool ack){observed=ack;active=true;++resyncs;}};
inline StubbornSender DataDlSender;

// Electrical boundary for the direct read-only adapter integration test.
constexpr int LOW=0,HIGH=1,OUTPUT_OPEN_DRAIN=3,SERIAL_8N1=0,SERIAL_RX_ONLY=1;
inline uint32_t mockUs=0;
inline uint32_t micros(){return mockUs;}
struct PinEvent{uint8_t pin;int value;};
inline std::vector<PinEvent> pinWrites,pinModes;
inline void digitalWrite(uint8_t pin,int value){pinWrites.push_back({pin,value});}
inline int digitalRead(uint8_t){return HIGH;}
inline void pinMode(uint8_t pin,int mode){pinModes.push_back({pin,mode});}
struct MockSerial {
 std::vector<uint8_t> input;unsigned baud=0;
 void begin(unsigned b,int format,int direction){assert(format==SERIAL_8N1&&direction==SERIAL_RX_ONLY);baud=b;}
 bool overrun=false,rxError=false;
 bool hasOverrun(){const bool b=overrun;overrun=false;return b;}
 bool hasRxError(){const bool b=rxError;rxError=false;return b;}
 unsigned available()const{return input.size();}
 int read(){assert(!input.empty());const int b=input.front();input.erase(input.begin());return b;}
};
inline MockSerial Serial;
