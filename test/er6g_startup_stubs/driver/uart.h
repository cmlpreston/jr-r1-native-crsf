#pragma once
#include <Arduino.h>
using esp_err_t=int;constexpr int ESP_OK=0;
using uart_port_t=int;constexpr int UART_NUM_2=2,UART_DATA_8_BITS=8,UART_PARITY_DISABLE=0,UART_STOP_BITS_1=1,UART_HW_FLOWCTRL_DISABLE=0,UART_SCLK_APB=1;
struct uart_config_t{int baud_rate,data_bits,parity,stop_bits,flow_ctrl,source_clk;};
inline std::deque<uint8_t> uartInput,transmitted;
inline bool failEcho=false,failUart=false,noReply=false;inline uint8_t replyValue=1;
inline int uart_param_config(int port,const uart_config_t*c){assert(port==2&&c->baud_rate==800&&c->data_bits==8&&c->stop_bits==1&&c->parity==0);return failUart?-1:0;}
inline int uart_set_pin(int port,int tx,int rx,int rts,int cts){assert(port==2&&rx==2&&rts==-1&&cts==-1&&(tx==-1||tx==2));if(tx==2){outputSignal=198;gpioMode=1;}return 0;}
inline int uart_driver_install(int port,int rx,int tx,int queue,void*ptr,int flags){assert(port==2&&rx==512&&tx==0&&queue==0&&ptr==nullptr&&flags==0);return 0;}
inline int uart_set_rx_full_threshold(int port,int n){assert(port==2&&n==1);return 0;}
inline int uart_set_rx_timeout(int port,int n){assert(port==2&&n==2);return 0;}
inline int uart_read_bytes(int port,uint8_t*b,unsigned n,int ticks){assert(port==2&&n==1&&ticks==0);if(uartInput.empty())return 0;*b=uartInput.front();uartInput.pop_front();return 1;}
inline int uart_write_bytes(int port,const uint8_t*b,unsigned n){
 assert(port==2&&gpioMode==3&&outputSignal==198);
 assert((n==2&&b[0]==0x1c&&b[1]==255)||(n==3&&b[0]==0x1c&&b[1]==0&&b[2]==0));
 for(unsigned i=0;i<n;++i){transmitted.push_back(b[i]);uartInput.push_back(failEcho?0:b[i]);}
 listenerUs+=n*12500;
 if(n==3&&!noReply){uartInput.push_back(0x1b);uartInput.push_back(replyValue);}return n;
}
inline int uart_wait_tx_done(int port,int ticks){assert(port==2&&ticks==100);return 0;}
