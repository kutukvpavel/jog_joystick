#pragma once

#include "user.h"

#define TOTAL_AXES (static_cast<size_t>(axis::types::LEN))

namespace axis
{
    enum class types
    {
        x,
        y,
        z,
        a,

        LEN
    };
    struct state
    {
        bool enabled;
        bool direction;
        float speed;
    };

    void init();

    state poll(types t);
    bool get_fast();
} // namespace axis
