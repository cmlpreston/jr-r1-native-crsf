#include "mcp23017_input.h"

#include <Wire.h>

namespace {

constexpr uint8_t IODIRA = 0x00;
constexpr uint8_t IODIRB = 0x01;
constexpr uint8_t GPPUA = 0x0c;
constexpr uint8_t GPPUB = 0x0d;
constexpr uint8_t GPIOA = 0x12;

} // namespace

bool Mcp23017Input::writeRegister(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(address_);
    Wire.write(reg);
    Wire.write(value);
    const bool ok = Wire.endTransmission() == 0;
    if (!ok) ++readErrors_;
    return ok;
}

bool Mcp23017Input::readRegisters(uint8_t reg, uint8_t *values, size_t count)
{
    Wire.beginTransmission(address_);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        ++readErrors_;
        return false;
    }
    const size_t received = Wire.requestFrom(address_, count);
    if (received != count) {
        while (Wire.available()) Wire.read();
        ++readErrors_;
        return false;
    }
    for (size_t index = 0; index < count; ++index)
        values[index] = static_cast<uint8_t>(Wire.read());
    return true;
}

bool Mcp23017Input::begin()
{
    // Restore the last configuration after reconnect. Never turn display reset
    // outputs into inputs or overwrite their latches during input polling.
    online_ = writeRegister(0x14, outputLatch_ & 0xff) &&
              writeRegister(0x15, outputLatch_ >> 8) &&
              writeRegister(GPPUA, pullupMask_ & 0xff) &&
              writeRegister(GPPUB, pullupMask_ >> 8) &&
              writeRegister(IODIRA, inputMask_ & 0xff) &&
              writeRegister(IODIRB, inputMask_ >> 8);
    return online_;
}

bool Mcp23017Input::configure(uint16_t inputs, uint16_t pullups, uint16_t latch)
{
    inputMask_ = inputs;
    pullupMask_ = pullups & inputs;
    outputLatch_ = latch;
    return begin();
}

bool Mcp23017Input::writeOutput(uint8_t pin, bool high)
{
    if (pin >= 16) return false;
    const uint16_t bit = uint16_t{1} << pin;
    outputLatch_ = high ? outputLatch_ | bit : outputLatch_ & ~bit;
    const bool ok = writeRegister(pin < 8 ? 0x14 : 0x15,
        pin < 8 ? outputLatch_ & 0xff : outputLatch_ >> 8);
    if (!ok) online_ = false;
    return ok;
}

bool Mcp23017Input::setOutput(uint8_t pin, bool high)
{
    if (pin >= 16 || !writeOutput(pin, high)) return false;
    inputMask_ &= ~(uint16_t{1} << pin);
    const bool ok = writeRegister(pin < 8 ? IODIRA : IODIRB,
        pin < 8 ? inputMask_ & 0xff : inputMask_ >> 8);
    if (!ok) online_ = false;
    return ok;
}

bool Mcp23017Input::setInput(uint8_t pin, bool pullup)
{
    if (pin >= 16) return false;
    const uint16_t bit = uint16_t{1} << pin;
    inputMask_ |= bit;
    pullupMask_ = pullup ? pullupMask_ | bit : pullupMask_ & ~bit;
    const bool ok = writeRegister(pin < 8 ? GPPUA : GPPUB,
        pin < 8 ? pullupMask_ & 0xff : pullupMask_ >> 8) &&
        writeRegister(pin < 8 ? IODIRA : IODIRB,
        pin < 8 ? inputMask_ & 0xff : inputMask_ >> 8);
    if (!ok) online_ = false;
    return ok;
}

bool Mcp23017Input::read(uint16_t &ports)
{
    uint8_t values[2];
    if (!readRegisters(GPIOA, values, sizeof(values))) {
        online_ = false;
        return false;
    }
    ports = static_cast<uint16_t>(values[0]) |
            static_cast<uint16_t>(values[1]) << 8U;
    online_ = true;
    return true;
}
