#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>

#ifdef __cplusplus
    template <typename T, size_t N>
    constexpr size_t array_size(T (&)[N]) { return N; }
#endif