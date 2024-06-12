#pragma once

#include "user.h"

#ifdef __cplusplus

#include "axis.h"

namespace cmd_streamer
{
    HAL_StatusTypeDef set_axis_state(axis::types t, const axis::state *s);

    float get_axis_jog_speed(axis::types t);
} // namespace cmd_queue

#endif

_BEGIN_STD_C
    void cmd_queue_uart_rxcplt_callback(UART_HandleTypeDef *huart);
_END_STD_C