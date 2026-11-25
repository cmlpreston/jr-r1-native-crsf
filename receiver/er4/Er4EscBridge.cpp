#include "targets.h"
#include "Er4EscBridge.h"
#include "esc_programmer_server.h"
#include "OTA.h"
#include "config.h"
#include "rxtx_intf.h"
#include "CRSFRouter.h"
#include "RXOTAConnector.h"
#include "stubborn_sender.h"
#include <cstdio>
#include <Arduino.h>

// Bench build defaults: RF handshake only. No ESC GPIO is touched until the
// direct shared-wire interface is enabled with ER4_ESC_INTERFACE=1.
#ifndef ER4_ESC_INTERFACE
#define ER4_ESC_INTERFACE 0
#endif
#ifndef ER4_ESC_WRITES
#define ER4_ESC_WRITES 0
#endif
static_assert(!ER4_ESC_WRITES || ER4_ESC_INTERFACE,"Writes require the measured interface");
extern CRSFRouter crsfRouter;
extern RxConfig config;
static EscProgrammerServer bridge;
extern RXOTAConnector otaConnector;
extern StubbornSender DataDlSender;
static bool lastSelected=false;
static volatile bool lastDownlinkAck=false;
void ICACHE_RAM_ATTR er4EscRecordAck(bool value){lastDownlinkAck=value;}
static uint32_t malformed=0;
static char rejectedFrame[43]={};
static uint32_t received=0,accepted=0,replied=0,blocked=0,edges=0,flushes=0;
static uint16_t lastRx=0,lastTx=0;
static uint32_t lastCh12=0;
static bool lastArmed=false;
const char *er4EscDiagnostics(){
    static char text[320];
    snprintf(text,sizeof(text),"received=%lu accepted=%lu replied=%lu blocked=%lu edges=%lu flushes=%lu lastRx=%u lastTx=%u CH12=%lu armed=%u malformed=%lu raw=%s",
        (unsigned long)received,(unsigned long)accepted,(unsigned long)replied,(unsigned long)blocked,(unsigned long)edges,(unsigned long)flushes,lastRx,lastTx,(unsigned long)lastCh12,lastArmed,(unsigned long)malformed,rejectedFrame);
    return text;
}
// Direct shared wire: output 3/GPIO3 receives; output 4/GPIO9 transmits
// open-drain. HIGH releases DATA, LOW pulls it down. No servo outputs remain.
static constexpr uint8_t TxPin=9;
#if ER4_ESC_INTERFACE
static bool initialized=false,txActive=false;
static uint8_t txBytes[3],txLength=0,txBit=0;
static uint32_t nextBit=0,lastBusActivity=0;
#endif
// Fixed-size electrical evidence survives RF deselection and Wi-Fi entry.
// Formatting happens only when /config is read, never in the serial bit loop.
#if ER4_ESC_INTERFACE
struct ElectricalTrace {uint32_t atUs;uint8_t value;char kind;};
static ElectricalTrace electricalTrace[32];
static uint32_t traceTotal=0,rxActive=0,rxInactive=0,rxOverruns=0,rxErrors=0;
static uint32_t txStarted=0,txCompleted=0,txAborted=0,timingFaults=0,maxLateUs=0;
static uint32_t startupInactive=0;
static uint8_t previousInactive=0,lastAbortBit=0;
static void recordElectrical(char kind,uint8_t value){
    electricalTrace[traceTotal%32]={micros(),value,kind};++traceTotal;
}
#endif
const char *er4EscElectricalDiagnostics(){
#if ER4_ESC_INTERFACE
    static char text[1800];
    const auto &d=bridge.diagnostics();
    int n=snprintf(text,sizeof(text),
        "interface=1 writes=%u baud=800 rxPin=3 txPin=9 rxLevel=%d txLevel=%d "
        "active=%u stage=%u echoPending=%u rxActive=%lu rxInactive=%lu overruns=%lu rxErrors=%lu "
        "txStarted=%lu txCompleted=%lu txAborted=%lu timingFaults=%lu maxLateUs=%lu lastAbortBit=%u "
        "echoMatched=%lu echoMismatch=%lu startups=%lu startupInactive=%lu readsOk=%lu invalidReplies=%lu "
        "timeouts=%lu busFaults=%lu lastStatus=%u lastStage=%u lastEchoPending=%u lastParameter=%u "
        "traceTotal=%lu trace=",
        unsigned(ER4_ESC_WRITES),digitalRead(3),digitalRead(TxPin),bridge.active(),bridge.stage(),bridge.pendingEcho(),
        (unsigned long)rxActive,(unsigned long)rxInactive,(unsigned long)rxOverruns,(unsigned long)rxErrors,
        (unsigned long)txStarted,(unsigned long)txCompleted,(unsigned long)txAborted,(unsigned long)timingFaults,
        (unsigned long)maxLateUs,lastAbortBit,(unsigned long)d.echoMatched,(unsigned long)d.echoMismatch,
        (unsigned long)d.startups,(unsigned long)startupInactive,(unsigned long)d.readsOk,(unsigned long)d.invalidReplies,
        (unsigned long)d.timeouts,(unsigned long)d.busFaults,d.lastStatus,d.lastStage,d.lastEchoPending,d.lastParameter,
        (unsigned long)traceTotal);
    if(n<0)return "format error";
    size_t used=size_t(n)<sizeof(text)?size_t(n):sizeof(text)-1;
    const uint32_t first=traceTotal>32?traceTotal-32:0;
    for(uint32_t i=first;i<traceTotal&&used<sizeof(text)-1;++i){
        const auto &e=electricalTrace[i%32];
        n=snprintf(text+used,sizeof(text)-used,"%s%lu:%c%02x",i==first?"":",",(unsigned long)e.atUs,e.kind,e.value);
        if(n<0)break;
        used+=size_t(n)<sizeof(text)-used?size_t(n):sizeof(text)-used-1;
    }
    return text;
#else
    return "interface=0 writes=0";
#endif
}
bool ICACHE_RAM_ATTR er4EscSelected(){
    return connectionState==connected && connectionHasModelMatch &&
        (!OtaIsFullRes||OtaSwitchModeCurrent!=smWideOr8ch) &&
        ChannelData[EscWire::SelectorChannel-1]>1600 && ChannelData[EscWire::SelectorChannel-1]<=2047 && !isArmed;
}
static uint8_t capabilities(){
    return uint8_t((!OtaIsFullRes||OtaSwitchModeCurrent!=smWideOr8ch)?EscWire::SelectorAvailable:0)|
        (ER4_ESC_INTERFACE?EscWire::Interface:0)|(ER4_ESC_WRITES?EscWire::Writes:0);
}
bool er4EscHandle(const crsf_header_t *frame){
    EscWire::Message request;
    const bool selected=er4EscSelected();
    if(!EscWire::parse(frame->type,(const uint8_t*)frame+3,frame->frame_size-2,false,request)){
        // EC is shared with the driving receiver. Never execute generic writes
        // on this dedicated bridge; unselected discovery/reads must not queue
        // stale replies for the next handover either.
        if(frame->type==0x2d || (!selected && (frame->type==0x28 || frame->type==0x2c))){
            ++blocked;
            if(frame->type==0x2d && frame->frame_size>=4 && ((const uint8_t*)frame)[5]==EscWire::Parameter){
                ++malformed;const unsigned n=frame->frame_size+2<21?frame->frame_size+2:21;
                for(unsigned i=0;i<n;++i)snprintf(rejectedFrame+2*i,3,"%02x",((const uint8_t*)frame)[i]);
            }
            return true;
        }
        return false;
    }
    ++received;lastRx=request.sequence;lastCh12=ChannelData[EscWire::SelectorChannel-1];lastArmed=isArmed;
    bridge.update(millis(),selected,capabilities());if(bridge.handle(request,millis()))++accepted;return true;
}
void er4EscLoop(){
    const bool selected=er4EscSelected();
    if(!selected && (lastSelected || DataDlSender.IsActive() || otaConnector.GetFifoFullPct())){
        noInterrupts();DataDlSender.ResetState();otaConnector.discardQueued();interrupts();++flushes;
    }
    if(selected && !lastSelected){
        // The handset's downlink ACK belongs to a different receiver until an
        // explicit ELRS resync. Do not lose the first reply to that transition.
        noInterrupts();DataDlSender.RequestResync(lastDownlinkAck);interrupts();++edges;
    }
    lastSelected=selected;
    // RX OTA telemetry also independently checks this selection immediately
    // before constructing a packet (integration hook); no second RF owner.
    bridge.update(millis(),selected,capabilities());
#if ER4_ESC_INTERFACE
    if(!initialized){
        digitalWrite(TxPin,HIGH);pinMode(TxPin,OUTPUT_OPEN_DRAIN);
        Serial.begin(800,SERIAL_8N1,SERIAL_RX_ONLY);initialized=true;
    }
    if(Serial.hasOverrun())++rxOverruns;
    if(Serial.hasRxError())++rxErrors;
    if(!bridge.active()){
        if(txActive){++txAborted;lastAbortBit=txBit;recordElectrical('A',txBit);}
        digitalWrite(TxPin,HIGH);txActive=false;
        for(unsigned n=0;n<8&&Serial.available();++n){
            const uint8_t b=Serial.read();++rxInactive;recordElectrical('i',b);
            if(previousInactive==0x1b&&b==0xff)++startupInactive;
            previousInactive=b;
        }
    }else{
        previousInactive=0;
        // Bound receive work. RX sees the exact TX echo on the shared wire.
        for(unsigned n=0;n<8&&Serial.available();++n){
            const uint8_t b=Serial.read();++rxActive;recordElectrical('R',b);
            bridge.receive(b,millis());lastBusActivity=micros();
        }
        const uint32_t now=micros();
        if(!txActive&&uint32_t(now-lastBusActivity)>=2500){txLength=bridge.takeTx(txBytes);if(txLength){txActive=true;txBit=0;nextBit=now;++txStarted;
            for(unsigned i=0;i<txLength;++i)recordElectrical('T',txBytes[i]);}}
        if(txActive&&int32_t(now-nextBit)>=0){
            // A scheduling overrun is a failed transaction, never a repeated
            // write. Actual timing under RF load still needs bench capture.
            const uint32_t late=now-nextBit;if(late>maxLateUs)maxLateUs=late;
            if(late>150){++timingFaults;++txAborted;lastAbortBit=txBit;recordElectrical('F',txBit);digitalWrite(TxPin,HIGH);txActive=false;bridge.busFault();}
            else{
                const unsigned bit=txBit%10,byte=txBit/10;
                const bool high=bit==9||(bit>0&&((txBytes[byte]>>(bit-1))&1));
                digitalWrite(TxPin,high?HIGH:LOW);++txBit;nextBit+=1250;
                if(txBit==txLength*10){++txCompleted;recordElectrical('C',txLength);txActive=false;lastBusActivity=now;digitalWrite(TxPin,HIGH);}
            }
        }
    }
#endif
    if(selected){EscWire::Message response;if(bridge.reply(response)){
        uint8_t bytes[21];EscWire::build(response,true,bytes,sizeof(bytes));++replied;lastTx=response.sequence;
        crsfRouter.deliverMessageTo(CRSF_ADDRESS_RADIO_TRANSMITTER,(crsf_header_t*)bytes);
    }}
}
