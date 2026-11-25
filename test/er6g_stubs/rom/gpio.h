#pragma once
#include <Arduino.h>
inline void gpio_matrix_out(int pin,int signal,bool invert,bool enableInvert){assert(pin==2&&(signal==256||signal==198)&&!invert&&!enableInvert);// ESP32 ROM matrix routing also enables the pad output.
outputSignal=signal;outputEnabled=true;}
