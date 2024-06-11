#pragma once

#include "user.h"

namespace interop
{
    enum class cmds
    {
        
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
