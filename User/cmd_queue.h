#pragma once

#include "user.h"

#ifdef __cplusplus

#include "axis.h"

namespace cmd_queue
{
    HAL_StatusTypeDef enqueue(axis::types t, const axis::state *s);
} // namespace cmd_queue

#endif

_BEGIN_STD_C
    void cmd_queue_uart_rx_callback(UART_HandleTypeDef *huart);
_END_STD_C