#include "Preferences.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {
fs::path stateRoot()
{
    const char *overridePath = std::getenv("JR_R1_SIM_STATE");
    if (overridePath != nullptr) return fs::path(overridePath);
#ifdef JR_R1_SIM_STATE_DIR
    return fs::path(JR_R1_SIM_STATE_DIR);
#else
    return fs::path("state");
#endif
}
}

bool Preferences::begin(const char *name, bool readOnly)
{
    name_ = name == nullptr ? "default" : name;
    readOnly_ = readOnly;
    if (!readOnly_) fs::create_directories(stateRoot() / name_);
    return true;
}

void Preferences::end() {}

std::string Preferences::pathFor(const char *key) const
{
    return (stateRoot() / name_ / (key == nullptr ? "unnamed" : key)).string();
}

std::vector<uint8_t> Preferences::readValue(const char *key) const
{
    std::ifstream input(pathFor(key), std::ios::binary);
    if (!input) return {};
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(input), {});
}

size_t Preferences::writeValue(const char *key, const void *value, size_t length)
{
    if (readOnly_ || value == nullptr) return 0;
    fs::create_directories(stateRoot() / name_);
    std::ofstream output(pathFor(key), std::ios::binary | std::ios::trunc);
    if (!output) return 0;
    output.write(static_cast<const char *>(value), static_cast<std::streamsize>(length));
    return output ? length : 0;
}

template <typename T>
T scalar(const std::vector<uint8_t> &bytes, T fallback)
{
    if (bytes.size() != sizeof(T)) return fallback;
    T value;
    std::memcpy(&value, bytes.data(), sizeof(value));
    return value;
}

bool Preferences::getBool(const char *key, bool fallback) const { return scalar(readValue(key), fallback); }
uint8_t Preferences::getUChar(const char *key, uint8_t fallback) const { return scalar(readValue(key), fallback); }
uint16_t Preferences::getUShort(const char *key, uint16_t fallback) const { return scalar(readValue(key), fallback); }
int8_t Preferences::getChar(const char *key, int8_t fallback) const { return scalar(readValue(key), fallback); }

String Preferences::getString(const char *key, const char *fallback) const
{
    const auto bytes = readValue(key);
    return bytes.empty() ? String(fallback) : String(std::string(bytes.begin(), bytes.end()));
}

size_t Preferences::getBytesLength(const char *key) const { return readValue(key).size(); }

size_t Preferences::getBytes(const char *key, void *destination, size_t length) const
{
    const auto bytes = readValue(key);
    if (destination == nullptr || bytes.size() < length) return 0;
    std::memcpy(destination, bytes.data(), length);
    return length;
}

size_t Preferences::putBool(const char *key, bool value) { return writeValue(key, &value, sizeof(value)); }
size_t Preferences::putUChar(const char *key, uint8_t value) { return writeValue(key, &value, sizeof(value)); }
size_t Preferences::putUShort(const char *key, uint16_t value) { return writeValue(key, &value, sizeof(value)); }
size_t Preferences::putChar(const char *key, int8_t value) { return writeValue(key, &value, sizeof(value)); }
size_t Preferences::putString(const char *key, const char *value) {
    return writeValue(key, value == nullptr ? "" : value,
                      value == nullptr ? 0 : std::strlen(value));
}
size_t Preferences::putBytes(const char *key, const void *value, size_t length) { return writeValue(key, value, length); }
