#pragma once
#include <Arduino.h>
using gpio_num_t=int;constexpr int GPIO_NUM_2=2,GPIO_MODE_INPUT=1,GPIO_FLOATING=0,GPIO_PULLUP_ONLY=1,GPIO_MODE_INPUT_OUTPUT_OD=3;
inline unsigned inputSet=0;
inline int gpio_set_direction(gpio_num_t pin,int mode){assert(pin==2);
#ifdef ER6G_READ_TEST
assert(mode==GPIO_MODE_INPUT||mode==GPIO_MODE_INPUT_OUTPUT_OD);
#else
assert(mode==GPIO_MODE_INPUT);
#endif
// IDF4.4 output enable AND disable detach the peripheral output matrix.
outputSignal=256;outputEnabled=mode==GPIO_MODE_INPUT_OUTPUT_OD;
gpioMode=mode;++inputSet;return 0;}
inline int gpio_set_pull_mode(gpio_num_t pin,int mode){assert(pin==2&&(mode==GPIO_FLOATING||mode==GPIO_PULLUP_ONLY));return 0;}
inline int gpio_get_level(gpio_num_t pin){assert(pin==2);return padLevel;}

inline int gpio_set_level(gpio_num_t pin,int value){assert(pin==2);
#ifdef ER6G_READ_TEST
assert(value==1||(value==0&&gpioMode==GPIO_MODE_INPUT_OUTPUT_OD&&outputSignal==256));
#else
assert(value==1);
#endif
outputLatch=value;return 0;}
