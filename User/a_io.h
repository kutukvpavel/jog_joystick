#pragma once

#include "user.h"

namespace a_io
{
    enum in
    {
        overall_speed,
        spindle_speed,
        x_speed,
        y_speed,
        z_speed,
        a_speed,
        vref,

        LEN
    };

    float get_input(in i);
} // namespace a_io
