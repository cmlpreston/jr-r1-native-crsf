#pragma once
#include <cstddef>
#include <cstdint>
using gpio_num_t = int;
constexpr int GPIO_NUM_1=1, GPIO_NUM_3=3, GPIO_NUM_4=4, GPIO_NUM_5=5;
constexpr int GPIO_NUM_8=8, GPIO_NUM_17=17, GPIO_NUM_43=43;
void delay(uint32_t milliseconds);
void delayMicroseconds(uint32_t microseconds);

constexpr int GPIO_NUM_9=9, GPIO_NUM_44=44;
