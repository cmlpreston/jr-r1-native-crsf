#pragma once

#include <Arduino.h>

// Minimal input-only MCP23017 driver for the physical-control prototype. It
// shares the existing Wire instance with the TFT touch controller and does not
// claim or reinitialise the I2C bus.
class Mcp23017Input {
public:
    explicit Mcp23017Input(uint8_t address) : address_(address) {}

    bool begin();
    bool read(uint16_t &ports);
    bool online() const { return online_; }
    uint32_t readErrors() const { return readErrors_; }

private:
    bool writeRegister(uint8_t reg, uint8_t value);
    bool readRegisters(uint8_t reg, uint8_t *values, size_t count);

    uint8_t address_;
    bool online_ = false;
    uint32_t readErrors_ = 0;
};
