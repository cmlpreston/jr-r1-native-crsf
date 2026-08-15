#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

using gpio_num_t = int;
constexpr gpio_num_t GPIO_NUM_1 = 1;
constexpr gpio_num_t GPIO_NUM_2 = 2;
constexpr gpio_num_t GPIO_NUM_4 = 4;
constexpr gpio_num_t GPIO_NUM_5 = 5;
constexpr gpio_num_t GPIO_NUM_17 = 17;

constexpr int LOW = 0;
constexpr int HIGH = 1;
constexpr int INPUT = 0;
constexpr int OUTPUT = 1;
constexpr int INPUT_PULLUP = 2;

template <typename T> constexpr T min(T a, T b) { return a < b ? a : b; }
template <typename T> constexpr T max(T a, T b) { return a > b ? a : b; }
template <typename A, typename B, typename C>
constexpr A constrain(A value, B low, C high) {
    const A convertedLow = static_cast<A>(low);
    const A convertedHigh = static_cast<A>(high);
    return value < convertedLow ? convertedLow
        : (value > convertedHigh ? convertedHigh : value);
}

long map(long value, long inMinimum, long inMaximum,
         long outMinimum, long outMaximum);
uint32_t millis();
uint64_t micros();
void delay(uint32_t milliseconds);
void delayMicroseconds(uint32_t microseconds);

class String {
public:
    String() = default;
    String(const char *value) : value_(value == nullptr ? "" : value) {}
    String(std::string value) : value_(std::move(value)) {}
    const char *c_str() const { return value_.c_str(); }
    size_t length() const { return value_.size(); }
    void toCharArray(char *destination, size_t capacity) const {
        if (destination == nullptr || capacity == 0) return;
        std::snprintf(destination, capacity, "%s", value_.c_str());
    }
private:
    std::string value_;
};

class Stream {
public:
    virtual ~Stream() = default;
    virtual size_t write(const uint8_t *data, size_t length) = 0;
    size_t write(uint8_t value) { return write(&value, 1); }
    virtual void flush() {}
    size_t print(const char *text) {
        return write(reinterpret_cast<const uint8_t *>(text), std::strlen(text));
    }
};

class ConsoleStream final : public Stream {
public:
    size_t write(const uint8_t *data, size_t length) override;
    void flush() override;
    void print(const char *text);
    void println(const char *text = "");
    int printf(const char *format, ...);
};

extern ConsoleStream Serial;
