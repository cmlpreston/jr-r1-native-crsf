#include "Arduino.h"

#include <cstdarg>

namespace {
const auto startedAt = std::chrono::steady_clock::now();
}

ConsoleStream Serial;

long map(long value, long inMinimum, long inMaximum,
         long outMinimum, long outMaximum)
{
    if (inMaximum == inMinimum) return outMinimum;
    return (value - inMinimum) * (outMaximum - outMinimum) /
        (inMaximum - inMinimum) + outMinimum;
}

uint32_t millis()
{
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startedAt).count());
}

uint64_t micros()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startedAt).count());
}

void delay(uint32_t milliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void delayMicroseconds(uint32_t microseconds)
{
    std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
}

size_t ConsoleStream::write(const uint8_t *data, size_t length)
{
    return std::fwrite(data, 1, length, stdout);
}

void ConsoleStream::flush() { std::fflush(stdout); }
void ConsoleStream::print(const char *text) { std::fputs(text, stdout); }
void ConsoleStream::println(const char *text) { std::puts(text); }

int ConsoleStream::printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    const int result = std::vprintf(format, args);
    va_end(args);
    return result;
}

