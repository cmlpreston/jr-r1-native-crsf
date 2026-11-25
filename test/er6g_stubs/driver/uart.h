#pragma once
#include <Arduino.h>
using esp_err_t=int;constexpr int ESP_OK=0;
using uart_port_t=int;constexpr int UART_NUM_2=2,UART_PIN_NO_CHANGE=-1;
constexpr int UART_DATA_8_BITS=8,UART_PARITY_DISABLE=0,UART_STOP_BITS_1=1,UART_HW_FLOWCTRL_DISABLE=0,UART_SCLK_APB=1;
struct uart_config_t{int baud_rate,data_bits,parity,stop_bits,flow_ctrl,source_clk;};
using QueueHandle_t=void*;
struct uart_event_t{int type;};
constexpr int UART_FIFO_OVF=1,UART_BUFFER_FULL=2,UART_FRAME_ERR=3,UART_PARITY_ERR=4,pdTRUE=1;
inline std::deque<uint8_t> uartInput;
inline std::deque<uart_event_t> uartEvents;
inline bool failInstall=false;
inline unsigned configs=0,installs=0,pinRoutes=0;
inline esp_err_t uart_param_config(uart_port_t p,const uart_config_t *c){assert(p==2&&c->baud_rate==800&&c->data_bits==8&&c->parity==0&&c->stop_bits==1&&c->flow_ctrl==0);++configs;return ESP_OK;}
inline esp_err_t uart_set_pin(uart_port_t p,int tx,int rx,int rts,int cts){assert(p==2&&rx==2&&rts==-1&&cts==-1);
#ifdef ER6G_READ_TEST
assert(tx==-1||(tx==2&&gpioMode==3));
#else
assert(tx==-1);
#endif
if(tx==2){outputSignal=198;gpioMode=1;}
++pinRoutes;return ESP_OK;}
inline esp_err_t uart_driver_install(uart_port_t p,int rx,int tx,int queue,QueueHandle_t *q,int){assert(p==2&&rx==512&&tx==0&&queue==12);++installs;*q=reinterpret_cast<void*>(1);return failInstall?-1:ESP_OK;}
inline int xQueueReceive(QueueHandle_t,uart_event_t *e,int ticks){assert(ticks==0);if(uartEvents.empty())return 0;*e=uartEvents.front();uartEvents.pop_front();return pdTRUE;}
inline int uart_read_bytes(uart_port_t p,uint8_t *out,unsigned limit,int ticks){assert(p==2&&limit==16&&ticks==0);unsigned n=0;while(n<limit&&!uartInput.empty()){out[n++]=uartInput.front();uartInput.pop_front();}return n;}
inline esp_err_t uart_get_baudrate(uart_port_t p,uint32_t *baud){assert(p==2);*baud=800;return ESP_OK;}

#ifdef ER6G_READ_TEST
inline std::deque<uint8_t> transmitted;
inline bool txDone=false,shortTx=false;
inline int uart_set_rx_full_threshold(uart_port_t p,int n){assert(p==2&&n==1);return ESP_OK;}
inline int uart_set_rx_timeout(uart_port_t p,int n){assert(p==2&&n==2);return ESP_OK;}
inline int uart_tx_chars(uart_port_t p,const char *b,unsigned n){
 assert(p==2&&gpioMode==3&&outputSignal==198&&(n==2||n==3));
 assert(n==2?(uint8_t(b[0])==0x1c&&uint8_t(b[1])==255):(uint8_t(b[1])<15&&((uint8_t(b[0])==0x1c&&b[2]==0)||(uint8_t(b[0])==0x1b&&uint8_t(b[2])<9))));
 unsigned sent=shortTx?n-1:n;for(unsigned i=0;i<sent;++i)transmitted.push_back(b[i]);return sent;
}
inline int uart_wait_tx_done(uart_port_t p,int ticks){assert(p==2&&ticks==0);return txDone?ESP_OK:-1;}
#endif

#ifdef ER6G_READ_TEST
inline int uart_flush_input(uart_port_t p){assert(p==2);uartInput.clear();return ESP_OK;}
inline int xQueueReset(QueueHandle_t){uartEvents.clear();return 1;}
#endif
