#pragma once

#include "Arduino.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class Preferences {
public:
    bool begin(const char *name, bool readOnly = false);
    void end();

    bool getBool(const char *key, bool fallback = false) const;
    uint8_t getUChar(const char *key, uint8_t fallback = 0) const;
    uint16_t getUShort(const char *key, uint16_t fallback = 0) const;
    int8_t getChar(const char *key, int8_t fallback = 0) const;
    String getString(const char *key, const char *fallback = "") const;
    size_t getBytesLength(const char *key) const;
    size_t getBytes(const char *key, void *destination, size_t length) const;

    size_t putBool(const char *key, bool value);
    size_t putUChar(const char *key, uint8_t value);
    size_t putUShort(const char *key, uint16_t value);
    size_t putChar(const char *key, int8_t value);
    size_t putString(const char *key, const char *value);
    size_t putBytes(const char *key, const void *value, size_t length);

private:
    std::vector<uint8_t> readValue(const char *key) const;
    size_t writeValue(const char *key, const void *value, size_t length);
    std::string pathFor(const char *key) const;

    std::string name_;
    bool readOnly_ = false;
};

