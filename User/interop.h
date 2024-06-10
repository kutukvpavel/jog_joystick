#pragma once

#include "user.h"

namespace interop
{
    enum class cmds
    {
        lamp_test_custom,
        lamp_test_predefined,
        modbus_keepalive_failed
    };
    struct cmd_t
    {
        cmds c;
        const void* arg;
    };

    void init();

    HAL_StatusTypeDef enqueue(cmds c, const void* arg);
    HAL_StatusTypeDef try_receive(cmds c, const void** arg); //Try to receive matching cmd
} // namespace interop
