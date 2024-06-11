#pragma once

#include "user.h"

#include "axis.h"

namespace nvs
{
    HAL_StatusTypeDef init();

    float get_rapid_speed(axis::types t);
} // namespace nvs
