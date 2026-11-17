#include "Er6gEscListen.h"
#include "Er6gEscRead.h"
#include "Er6gEscStartupPeer.h"
#include "surpass_protocol.h"
#include <Arduino.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <rom/gpio.h>
#include <soc/gpio_sig_map.h>
#include <atomic>
#include <esp_timer.h>
#include <cstdio>
#include <cstring>
namespace {
constexpr uart_port_t Port=UART_NUM_2;
constexpr gpio_num_t Pin=GPIO_NUM_2;
QueueHandle_t events=nullptr;
esp_timer_handle_t entryTimer=nullptr;
std::atomic<uint32_t> pulseReleasedUs{0};
std::atomic_bool holdReady{false},releaseRequested{false};
uint8_t entryState=0; // 0=none, 1=low, 2=waiting for startup, 3=handshake
uint32_t pulseStartedUs=0,pulseDurationUs=0,sendNotBeforeUs=0,entryPulses=0,entryStartups=0;
void endEntryPulse(void*){
    gpio_set_level(Pin,1); // Release only; GPIO remains open-drain until loop detaches.
    pulseReleasedUs.store(micros(),std::memory_order_release);
}
bool attempted=false,ready=false,attached=false,sending=false,head=false,active=false,awaitingBus=false;
bool bootProbe=false,bootRan=false;
uint32_t bootReadyUs=0,bootDurationUs=0;
uint8_t bootStage=0,bootValue=255;
const char *bootResult="Not run";
bool allowed(){return bootProbe||er6gEscReadWifiAllowed()
#ifdef ER6G_ESC_DRIVE_MODE
 ||er6gEscReadRfAllowed()
#endif
 ;}
esp_err_t initError=ESP_OK;
std::atomic_bool busy{false},pending{false};
uint8_t requestedOperation=0,requestedParameter=0,requestedValue=0;
uint8_t writeStage=0,writeDesired=0;
uint8_t operation=0,parameter=0,tx[3],length=0,echoAt=0,lastValue=255;
uint32_t txStartedUs=0,started=0,lastRxUs=0,bytes=0,startups=0,overflows=0,frameErrors=0,parityErrors=0;
uint32_t txStarted=0,txCompleted=0,echoMatched=0,echoMismatch=0,timeouts=0;
uint8_t previous=0,previousEntry=0;
const char *status="Idle; no automatic transmissions";
struct Sample{uint32_t us;uint8_t byte;char kind;};Sample samples[64];uint32_t traceTotal=0;
void record(char kind,uint8_t b){samples[traceTotal%64]={micros(),b,kind};++traceTotal;}
void release(){
    // IDF4.4 input mode disables the pad AND detaches the output matrix.
    // Do not call gpio_matrix_out here: ESP32 ROM re-enables output, turning
    // this supposedly released wire into a push-pull HIGH that masks ESC RX.
    gpio_set_direction(Pin,GPIO_MODE_INPUT);
    gpio_set_level(Pin,1);
    gpio_set_pull_mode(Pin,GPIO_PULLUP_ONLY);
    attached=sending=false;
}
void finish(const char *message){if(entryTimer)esp_timer_stop(entryTimer);release();status=message;head=false;entryState=0;holdReady.store(false);releaseRequested.store(false);active=awaitingBus=false;busy.store(false,std::memory_order_release);}
}
bool er6gEscListenOwnsPin(int pin){return pin==int(Pin)
#ifdef ER6G_ESC_DRIVE_MODE
 &&er6gStartupProgramming()
#endif
 ;}
bool er6gEscReadStart(uint8_t op,uint8_t param,bool confirmed){
    if(!confirmed||!ready||!allowed()||op>3||(op==1&&param>=15))return false;
    bool expected=false;if(!busy.compare_exchange_strong(expected,true))return false;
    requestedOperation=op;requestedParameter=param;
    pending.store(true,std::memory_order_release);return true;
}
#ifdef ER6G_ESC_DRIVE_MODE
bool er6gEscWriteStart(uint8_t param,uint8_t value,bool confirmed){
#if ER6G_ESC_WRITES
    if(!confirmed||!ready||!er6gEscReadRfAllowed()||!er6gEscReadSessionReady()||!Surpass::valid(param,value))return false;
    bool expected=false;if(!busy.compare_exchange_strong(expected,true))return false;
    requestedOperation=4;requestedParameter=param;requestedValue=value;
    pending.store(true,std::memory_order_release);return true;
#else
    (void)param;(void)value;(void)confirmed;return false;
#endif
}
bool er6gEscWriteValue(uint8_t &value){value=lastValue;return !busy.load()&&!std::strcmp(status,"Write readback verified");}
#endif
bool er6gEscReadReleaseEntry(bool confirmed){
    if(!confirmed||!allowed()||!holdReady.load(std::memory_order_acquire))return false;
    releaseRequested.store(true,std::memory_order_release);return true;
}
void er6gEscListenLoop(){
#ifdef ER6G_ESC_DRIVE_MODE
    if(!er6gStartupProgramming())return; // No GPIO/UART calls on an ordinary PWM boot.
#endif
    if(!attempted){
        attempted=true;release();uart_config_t config={};config.baud_rate=800;
        config.data_bits=UART_DATA_8_BITS;config.parity=UART_PARITY_DISABLE;
        config.stop_bits=UART_STOP_BITS_1;config.flow_ctrl=UART_HW_FLOWCTRL_DISABLE;config.source_clk=UART_SCLK_APB;
        initError=uart_param_config(Port,&config);
        if(initError==ESP_OK)initError=uart_set_pin(Port,UART_PIN_NO_CHANGE,int(Pin),UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);
        if(initError==ESP_OK)initError=uart_driver_install(Port,512,0,12,&events,0);
        if(initError==ESP_OK)initError=uart_set_rx_full_threshold(Port,1);
        if(initError==ESP_OK)initError=uart_set_rx_timeout(Port,2);
        if(initError==ESP_OK){
            esp_timer_create_args_t timerConfig={};timerConfig.callback=endEntryPulse;
            timerConfig.dispatch_method=ESP_TIMER_TASK;timerConfig.name="esc-entry-release";
            initError=esp_timer_create(&timerConfig,&entryTimer);
        }
        ready=initError==ESP_OK;
    }
    if(!ready)return;
    if(pending.exchange(false,std::memory_order_acquire)){
        operation=requestedOperation;parameter=requestedParameter;lastValue=255;head=false;echoAt=0;
        writeStage=operation==4?1:0;writeDesired=requestedValue;
        length=operation==1||operation==4?3:2;tx[0]=operation==4?0x1b:0x1c;tx[1]=length==3?parameter:0xff;tx[2]=operation==4?writeDesired:0;
        status="Queued; waiting for idle high";started=millis();active=awaitingBus=true;entryState=0;sendNotBeforeUs=0;
    }
    bool transactionAllowed=allowed();
#ifdef ER6G_ESC_DRIVE_MODE
    // Entering Wi-Fi must never extend permission for an RF-owned write.
    if(operation==4)transactionAllowed=er6gEscReadRfAllowed()&&er6gEscReadSessionReady();
#endif
    if(active&&!transactionAllowed){finish("Aborted; transaction safety gate ended");}
    if(entryState==1){
        const bool held=operation==3;
        if(held&&releaseRequested.exchange(false,std::memory_order_acquire)){
            esp_timer_stop(entryTimer);endEntryPulse(nullptr);
        }
        const uint32_t limit=held?180010000:79000;
        const uint32_t released=pulseReleasedUs.load(std::memory_order_acquire);
        if(!released){
            if(uint32_t(micros()-pulseStartedUs)>limit){
                pulseDurationUs=micros()-pulseStartedUs;finish("Entry release timer late; pin released");
            }
            return; // Break-related RX/events are discarded after release.
        }
        pulseDurationUs=released-pulseStartedUs;holdReady.store(false);release();
        uart_flush_input(Port);xQueueReset(events);previous=0;head=false;lastRxUs=micros();
        if(pulseDurationUs>limit){finish("Entry pulse timing overrun; no handshake");return;}
        entryState=2;started=millis();status="Entry pulse released; waiting for ESC startup";
    }
    if(sending&&uint32_t(micros()-txStartedUs)>=uint32_t(length)*12500&&uart_wait_tx_done(Port,0)==ESP_OK){++txCompleted;sending=false;release();}
    uart_event_t event;
    for(unsigned n=0;n<8&&xQueueReceive(events,&event,0)==pdTRUE;++n){
        if(event.type==UART_FIFO_OVF||event.type==UART_BUFFER_FULL)++overflows;
        else if(event.type==UART_FRAME_ERR)++frameErrors;
        else if(event.type==UART_PARITY_ERR)++parityErrors;
        if(active&&(event.type==UART_FIFO_OVF||event.type==UART_BUFFER_FULL||event.type==UART_FRAME_ERR||event.type==UART_PARITY_ERR))finish("UART error; no retry");
    }
    uint8_t input[16];const int count=uart_read_bytes(Port,input,sizeof(input),0);
    for(int i=0;i<count;++i){
        const uint8_t b=input[i];record('R',b);++bytes;lastRxUs=micros();
        if(previous==0x1b&&b==0xff)++startups;
        previous=b;
        if(!active||awaitingBus)continue;
        if(entryState==2){
            if(previousEntry==0x1b&&b==0xff){
                ++entryStartups;entryState=3;awaitingBus=true;head=false;
                sendNotBeforeUs=micros()+40000;status="ESC startup received; startup response queued";
            }
            previousEntry=b;continue;
        }
        if(echoAt<length){
            if(b!=tx[echoAt]){++echoMismatch;finish("Echo mismatch; no retry");}
            else{++echoAt;++echoMatched;}
        }else if(operation==1||operation==4){
            if(!head){head=b==0x1b;continue;}head=false;
            if(operation==4&&writeStage==1){
                if(b!=0x0a){finish("Write acknowledgement invalid; outcome unknown");continue;}
                // ACK is not proof of the stored value. Release the bus and
                // allow 100ms before one readback; no write is queued again.
                writeStage=2;tx[0]=0x1c;tx[1]=parameter;tx[2]=0;
                echoAt=0;awaitingBus=true;started=millis();sendNotBeforeUs=micros()+100000;
                status="Write acknowledged; verifying readback";continue;
            }
            if(operation==4){
                lastValue=b;finish(Surpass::valid(parameter,b)&&b==writeDesired?"Write readback verified":"Write readback mismatch; outcome unknown");
            }else if(b==0xff){finish("ESC startup received; send startup response first");}
            else if(Surpass::valid(parameter,b)){lastValue=b;finish("Read reply received");}
            else finish("Invalid read value; no retry");
        }
    }
    if(!active)return;
    if(uint32_t(millis()-started)>=1800){++timeouts;finish("Timeout; no retry");return;}
    if(awaitingBus){
        if(gpio_get_level(Pin)!=1||uint32_t(micros()-lastRxUs)<2500)return;
        if(sendNotBeforeUs&&int32_t(micros()-sendNotBeforeUs)<0)return;
        if((operation==2||operation==3)&&entryState==0){
            gpio_set_level(Pin,1);gpio_set_direction(Pin,GPIO_MODE_INPUT_OUTPUT_OD);
            gpio_matrix_out(Pin,SIG_GPIO_OUT_IDX,false,false);
            pulseReleasedUs.store(0,std::memory_order_relaxed);releaseRequested.store(false);previousEntry=0;
            // Release via an independent timer even if the main loop is delayed.
            pulseStartedUs=micros();gpio_set_level(Pin,0);
            if(esp_timer_start_once(entryTimer,operation==3?180000000:69000)!=ESP_OK){finish("Entry timer failed; pin released");return;}
            entryState=1;awaitingBus=false;++entryPulses;holdReady.store(operation==3,std::memory_order_release);
            status=operation==3?"Startup hold low; release explicitly or after 180 seconds":"Entry pulse low; automatic release scheduled";return;
        }
        // HIGH in open-drain mode releases the wire. Route TX LAST: the IDF
        // gpio_set_direction output-enable helper resets the output matrix to GPIO.
        gpio_set_level(Pin,1);gpio_set_direction(Pin,GPIO_MODE_INPUT_OUTPUT_OD);
        const auto error=uart_set_pin(Port,int(Pin),int(Pin),UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);
        gpio_set_direction(Pin,GPIO_MODE_INPUT_OUTPUT_OD);gpio_set_pull_mode(Pin,GPIO_PULLUP_ONLY);
        if(error!=ESP_OK){finish("TX pin routing failed");return;}
        gpio_matrix_out(Pin,U2TXD_OUT_IDX,false,false);
        attached=true;awaitingBus=false;status="Sending once; waiting for echo/reply";
        txStartedUs=micros();
        const int sent=uart_tx_chars(Port,reinterpret_cast<const char*>(tx),length);
        ++txStarted;for(int i=0;i<sent;++i)record('T',tx[i]);
        if(sent!=length){finish("Short TX; no retry");return;}sending=true;
    }
    if((operation==0||operation==2||operation==3)&&!sending&&echoAt==length){
        finish(operation>=2?"Entry startup answered and echoed; verify with a read":"Startup response sent and echoed; ESC acceptance not confirmed");
    }
}
// Dedicated read-only bench boot window. Called BEFORE ExpressLRS options/RF
// setup. Send one startup response immediately, even if bootloader latency
// meant the ESC announcement was missed. No pulse, PWM or setting writes.
void er6gEscReadBootProbe(){
    if(bootRan)return;
#ifdef ER6G_ESC_DRIVE_MODE
    er6gStartupModeBegin();
    bootRan=true;
    if(!er6gStartupProgramming()){release();bootResult="Drive; pin released until valid PWM";return;}
#ifdef ER6G_ESC_SINGLE_STAGE
    if(!er6gStartupEntryRequested()){
        bootResult="Programming locked; no fresh entry request";return;
    }
#else
    Er6gStartup::Record startupRecord;
    if(er6gStartupResume(startupRecord)){
        bootValue=uint8_t(startupRecord.value);
        bootResult=startupRecord.result==Er6gStartup::ReadVerified?"PASS; programming ready":"Entry failed; power cycle to Drive";
    }else bootResult="Programming locked; no verified startup record";
    er6gStartupCancel(); // Default next boot is full app / PWM, never automatic re-entry.
    return;
#endif
#endif
    bootRan=true;bootProbe=true;bootResult="Trying one early startup response";bootStage=1;
    const uint32_t began=micros();
    er6gEscListenLoop();bootReadyUs=micros();
    Er6gStartup::Record handoff;
    if(ready&&er6gStartupResume(handoff)){
        bootStage=4;bootValue=uint8_t(handoff.value);
        bootResult=handoff.result==Er6gStartup::ReadVerified?"PASS; small startup stage verified read":
                   handoff.result==Er6gStartup::NoStartup?"Startup stage saw no ESC startup":"Startup stage failed; full firmware recovered";
        er6gStartupArmNextBoot();bootProbe=false;bootDurationUs=micros()-began;return;
    }
    uint32_t nextActionUs=micros();
    if(!ready){bootResult="UART initialisation failed";bootStage=4;}
    while(ready&&uint32_t(micros()-began)<3000000&&bootStage!=4){
        er6gEscListenLoop();
        if(bootStage==1&&int32_t(micros()-nextActionUs)>=0){
            if(!er6gEscReadStart(0,0,true)){bootResult="Startup response unavailable";break;}
            bootStage=2;
        }else if(bootStage==2&&!busy.load()){
            if(std::strcmp(status,"Startup response sent and echoed; ESC acceptance not confirmed")){
                bootResult="Startup response failed";break;
            }
            nextActionUs=micros()+1250000;bootStage=3;
        }else if(bootStage==3&&int32_t(micros()-nextActionUs)>=0){
            if(!er6gEscReadStart(1,0,true)){bootResult="Boot read unavailable";break;}
            bootStage=5;
        }else if(bootStage==5&&!busy.load()){
            if(!std::strcmp(status,"Read reply received")){
                bootValue=lastValue;bootResult="PASS; startup answered and read verified";
            }else bootResult="Boot read failed";
            bootStage=4;
        }
        delay(1); // Yield; UART clocks the bits independently of this loop.
    }
    if(bootStage!=4&&!std::strcmp(bootResult,"Trying one early startup response"))
        bootResult="Boot probe timed out";
    if(active)finish("Boot window ended; pin released");
    er6gStartupArmNextBoot();bootProbe=false;bootDurationUs=micros()-began;
}
const char *er6gEscListenDiagnostics(){
    static char text[2300];uint32_t baud=0;const auto error=ready?uart_get_baudrate(Port,&baud):initError;
#ifdef ER6G_ESC_DRIVE_MODE
#if ER6G_ESC_WRITES
    const char *mode=er6gStartupProgramming()?"mode=programming output=5 gpio=2 uart=2 writes=rf-only pwm=reserved":"mode=drive output=5 gpio=2 writes=rf-only pwm=normal";
#else
    const char *mode=er6gStartupProgramming()?"mode=programming output=5 gpio=2 uart=2 writes=disabled pwm=reserved":"mode=drive output=5 gpio=2 writes=disabled pwm=normal";
#endif
#else
    const char *mode="mode=read-only output=5 gpio=2 uart=2 writes=disabled pwm=disabled";
#endif
    int n=snprintf(text,sizeof(text),
        "%s ready=%u initError=%d baud=%lu baudError=%d "
        "level=%d busy=%u operation=%u parameter=%u value=%u status=%s bytes=%lu startups=%lu overflows=%lu frameErrors=%lu parityErrors=%lu "
        "txStarted=%lu txCompleted=%lu echoMatched=%lu echoMismatch=%lu echoPending=%u timeouts=%lu entryPulses=%lu entryStartups=%lu pulseDurationUs=%lu entryState=%u bootReadyUs=%lu bootDurationUs=%lu bootValue=%u bootResult=%s %s trace=",
        mode,ready,int(initError),(unsigned long)baud,int(error),gpio_get_level(Pin),busy.load(),operation,parameter,lastValue,status,
        (unsigned long)bytes,(unsigned long)startups,(unsigned long)overflows,(unsigned long)frameErrors,(unsigned long)parityErrors,
        (unsigned long)txStarted,(unsigned long)txCompleted,(unsigned long)echoMatched,(unsigned long)echoMismatch,
        unsigned(length-echoAt),(unsigned long)timeouts,(unsigned long)entryPulses,(unsigned long)entryStartups,
        (unsigned long)pulseDurationUs,unsigned(entryState),(unsigned long)bootReadyUs,
        (unsigned long)bootDurationUs,unsigned(bootValue),bootResult,er6gStartupDiagnostics());
    if(n<0)return "format error";
    size_t used=size_t(n)<sizeof(text)?size_t(n):sizeof(text)-1;
    const uint32_t first=traceTotal>64?traceTotal-64:0;
    for(uint32_t i=first;i<traceTotal&&used<sizeof(text)-1;++i){const auto &s=samples[i%64];
        n=snprintf(text+used,sizeof(text)-used,"%s%lu:%c%02x",i==first?"":",",(unsigned long)s.us,s.kind,s.byte);
        if(n<0)break;
        used+=size_t(n)<sizeof(text)-used?size_t(n):sizeof(text)-used-1;}
    return text;
}

#ifdef ER6G_ESC_DRIVE_MODE
bool er6gEscReadSessionReady(){return er6gStartupProgramming()&&bootValue<9&&!std::strncmp(bootResult,"PASS",4);}
bool er6gEscReadBusy(){return busy.load();}
bool er6gEscReadValue(uint8_t &value){value=lastValue;return !busy.load()&&!std::strcmp(status,"Read reply received");}
void er6gEscReadAbort(){if(active||busy.load()){pending.store(false);finish("RF session ended; pin stays reserved until power cycle");}}
#endif
