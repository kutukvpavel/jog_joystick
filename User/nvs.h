#pragma once

#include "user.h"

#include "axis.h"

namespace nvs
{
    HAL_StatusTypeDef init();
    HAL_StatusTypeDef save();
    HAL_StatusTypeDef load();
    HAL_StatusTypeDef reset();
    void dump_hex();
    HAL_StatusTypeDef test();
    
    uint8_t get_stored_version();
    uint8_t get_required_version();
    bool get_version_match();

    float get_rapid_speed(axis::types t);
    float get_max_speed(axis::types t);
} // namespace nvs
