#pragma once
#include <cstddef>
constexpr int MALLOC_CAP_DMA = 1;
inline size_t heap_caps_get_largest_free_block(int) { return 65432; }
