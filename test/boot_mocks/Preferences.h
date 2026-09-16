#pragma once
#include <cstring>
#include <map>
#include <string>
#include <vector>
class Preferences {
public:
    inline static std::map<std::string, std::vector<unsigned char>> storage;
    inline static bool failWrites = false;
    inline static unsigned writes = 0;
    bool begin(const char *, bool) { return true; }
    void end() {}
    bool isKey(const char *key) { return storage.count(key) != 0; }
    size_t getBytesLength(const char *key) { return storage.at(key).size(); }
    size_t getBytes(const char *key, void *out, size_t size) {
        const auto &bytes = storage.at(key);
        if (bytes.size() != size) return 0;
        memcpy(out, bytes.data(), size);
        return size;
    }
    size_t putBytes(const char *key, const void *data, size_t size) {
        if (failWrites) return 0;
        ++writes;
        const auto *bytes = static_cast<const unsigned char *>(data);
        storage[key] = std::vector<unsigned char>(bytes, bytes + size);
        return size;
    }
};
