#pragma once

#include "user.h"

#define TOTAL_AXES (static_cast<size_t>(axis::types::LEN))

namespace axis
{
    enum class types
    {
        x,

        LEN
    };
    struct state
    {
        bool jog_enabled;
        bool trigger_auto_pos;
        bool trigger_auto_neg;
        bool direction;
        float speed;
    };

    void init();

    state poll(types t);
    bool get_fast();
} // namespace axis
