#pragma once
#include <cstddef>
constexpr unsigned MALLOC_CAP_SPIRAM=1, MALLOC_CAP_INTERNAL=2, MALLOC_CAP_8BIT=4;
void *heap_caps_malloc(size_t bytes,unsigned capabilities);
