#pragma once
#include "../er6g_stubs/Arduino.h"
#include <stdexcept>
using byte=uint8_t;
inline void esp_restart(){throw std::runtime_error("mock restart");}
constexpr int pdMS_TO_TICKS(int ms){return ms;}
