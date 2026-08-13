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
    // Every expander pin starts as an active-low input with its MCP23017
    // pull-up enabled. A switch therefore needs only to connect its port to GND.
    online_ = writeRegister(IODIRA, 0xff) &&
              writeRegister(IODIRB, 0xff) &&
              writeRegister(GPPUA, 0xff) &&
              writeRegister(GPPUB, 0xff);
    return online_;
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
