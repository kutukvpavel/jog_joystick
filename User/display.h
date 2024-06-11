#pragma once

#include "user.h"

#include "axis.h"

namespace display
{
    enum class state
    {
        idle,
        jog,
        jog_fast,

        LEN
    };

    HAL_StatusTypeDef set_axis_state(axis::types t, state s, bool direction, float speed);
} // namespace display
