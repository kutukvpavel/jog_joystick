#pragma once

#include "../Middlewares/Third_Party/FreeRTOS/Source/include/FreeRTOS.h"
#include "../Middlewares/Third_Party/FreeRTOS/Source/include/task.h"
#include "../Middlewares/Third_Party/FreeRTOS/Source/include/queue.h"
#include "../Middlewares/Third_Party/FreeRTOS/Source/include/semphr.h"

#include "my_types.h"
#include "main.h"
#include "ushell/inc/sys_command_line.h"

#include <stdio.h>

#define MY_FIRMWARE_INFO_STR "cnc_jogger-v0.1-a"
