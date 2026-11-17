#pragma once
#include <cstddef>
#include <cstdint>

// Private per-model observations only. Never contains settings or control state.
// A temporary file + atomic replacement preserves the last complete checkpoint.
namespace RunRecordStore {
bool read(uint8_t model, void *data, size_t bytes);
bool write(uint8_t model, const void *data, size_t bytes);
bool erase(uint8_t model);
}
