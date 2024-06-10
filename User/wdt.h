#pragma once

#include "user.h"

namespace wdt
{
    struct task_t
    {
        TickType_t deadline{ 0 };
        TickType_t last_time{ 0 };
        const char* name = NULL;
    };

    task_t* register_task(uint32_t interval_ms, const char* name);
} // namespace wdt