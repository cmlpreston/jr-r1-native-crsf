#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
inline uint32_t millis() { return 100; }
struct MockEsp { uint32_t getFreeHeap() { return 123456; } };
inline MockEsp ESP;
struct MockSerial {
    std::string output;
    void println(const char *s = "") { output += std::string(s) + "\n"; }
    template<class... Args> void printf(const char *format, Args... args) {
        char line[512];
        snprintf(line, sizeof(line), format, args...);
        output += line;
    }
};
inline MockSerial Serial;
