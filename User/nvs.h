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
    float get_min_speed(axis::types t);

    void set_rapid_speed(axis::types t, float s);
    void set_max_speed(axis::types t, float s);
    void set_min_speed(axis::types t, float s);

    float get_low_pot_threshold();
    void set_low_pot_threshold(float v);
} // namespace nvs
