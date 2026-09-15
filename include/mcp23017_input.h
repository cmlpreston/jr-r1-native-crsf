#pragma once

#include <Arduino.h>

// MCP23017 input/output driver. It
// shares the existing Wire instance with the TFT touch controller and does not
// claim or reinitialise the I2C bus.
class Mcp23017Input {
public:
    explicit Mcp23017Input(uint8_t address) : address_(address) {}

    bool begin();
    bool configure(uint16_t inputMask, uint16_t pullupMask, uint16_t outputLatch);
    bool setOutput(uint8_t pin, bool high);
    bool setInput(uint8_t pin, bool pullup);
    bool writeOutput(uint8_t pin, bool high);
    bool read(uint16_t &ports);
    bool online() const { return online_; }
    uint32_t readErrors() const { return readErrors_; }

private:
    bool writeRegister(uint8_t reg, uint8_t value);
    bool readRegisters(uint8_t reg, uint8_t *values, size_t count);

    uint8_t address_;
    uint16_t inputMask_ = 0xffff;
    uint16_t pullupMask_ = 0xffff;
    uint16_t outputLatch_ = 0;
    bool online_ = false;
    uint32_t readErrors_ = 0;
};
