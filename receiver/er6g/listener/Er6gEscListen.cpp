#include "Er6gEscListen.h"
#include <Arduino.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <cstdio>

namespace {
constexpr uart_port_t Port=UART_NUM_2;
constexpr gpio_num_t Pin=GPIO_NUM_2;
QueueHandle_t events=nullptr;
bool attempted=false,ready=false;
esp_err_t initError=ESP_OK;
uint32_t byteCount=0,startups=0,overflows=0,frameErrors=0,parityErrors=0;
uint8_t previous=0;
struct Sample{uint32_t us;uint8_t byte;};
Sample samples[64];
}
bool er6gEscListenOwnsPin(int pin){return pin==int(Pin);}
void er6gEscListenLoop(){
    if(!attempted){
        attempted=true;
        // No TX pad is attached. Leave the ESC wire undriven, including on error.
        gpio_set_direction(Pin,GPIO_MODE_INPUT);
        gpio_set_pull_mode(Pin,GPIO_FLOATING);
        uart_config_t config={};
        config.baud_rate=800;config.data_bits=UART_DATA_8_BITS;
        config.parity=UART_PARITY_DISABLE;config.stop_bits=UART_STOP_BITS_1;
        config.flow_ctrl=UART_HW_FLOWCTRL_DISABLE;config.source_clk=UART_SCLK_APB;
        initError=uart_param_config(Port,&config);
        if(initError==ESP_OK)initError=uart_set_pin(Port,UART_PIN_NO_CHANGE,int(Pin),UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);
        if(initError==ESP_OK)initError=uart_driver_install(Port,512,0,12,&events,0);
        ready=initError==ESP_OK;
    }
    if(!ready)return;
    uart_event_t event;
    for(unsigned n=0;n<8&&xQueueReceive(events,&event,0)==pdTRUE;++n){
        if(event.type==UART_FIFO_OVF||event.type==UART_BUFFER_FULL)++overflows;
        else if(event.type==UART_FRAME_ERR)++frameErrors;
        else if(event.type==UART_PARITY_ERR)++parityErrors;
    }
    uint8_t bytes[16];const int count=uart_read_bytes(Port,bytes,sizeof(bytes),0);
    for(int i=0;i<count;++i){
        const uint8_t b=bytes[i];samples[byteCount%64]={micros(),b};++byteCount;
        if(previous==0x1b&&b==0xff)++startups;
        previous=b;
    }
}
const char *er6gEscListenDiagnostics(){
    static char text[1800];uint32_t baud=0;
    const esp_err_t baudError=ready?uart_get_baudrate(Port,&baud):initError;
    int n=snprintf(text,sizeof(text),
        "mode=listen-only output=5 gpio=2 uart=2 tx=disabled pwm=disabled ready=%u initError=%d "
        "baud=%lu baudError=%d level=%d bytes=%lu startups=%lu overflows=%lu frameErrors=%lu parityErrors=%lu trace=",
        ready,int(initError),(unsigned long)baud,int(baudError),gpio_get_level(Pin),
        (unsigned long)byteCount,(unsigned long)startups,(unsigned long)overflows,
        (unsigned long)frameErrors,(unsigned long)parityErrors);
    if(n<0)return "format error";
    size_t used=size_t(n)<sizeof(text)?size_t(n):sizeof(text)-1;
    const uint32_t first=byteCount>64?byteCount-64:0;
    for(uint32_t i=first;i<byteCount&&used<sizeof(text)-1;++i){
        const auto &s=samples[i%64];
        n=snprintf(text+used,sizeof(text)-used,"%s%lu:%02x",i==first?"":",",(unsigned long)s.us,s.byte);
        if(n<0)break;
        used+=size_t(n)<sizeof(text)-used?size_t(n):sizeof(text)-used-1;
    }
    return text;
}
