#include "cmd_queue.h"

#include "task_handles.h"
#include "wdt.h"

#include "../Core/Inc/usart.h"

namespace cmd_queue
{
    struct axis_cmd_data
    {
        char designator;
        const char* n_limit;
        const char* p_limit;
    };
    static axis_cmd_data data[TOTAL_AXES] = { 
        {
            .designator = 'X',
            .n_limit = "-100",
            .p_limit = "100"
        },
        {
            .designator = 'Y',
            .n_limit = "-100",
            .p_limit = "100"
        },
        {
            .designator = 'Z',
            .n_limit = "-100",
            .p_limit = "100"
        },
        {
            .designator = 'A',
            .n_limit = "-100",
            .p_limit = "100"
        }
    };
    struct queue_data
    {
        axis::types t;
        axis::state s;
    };

    const size_t queue_length = 16;
    const size_t item_size = sizeof(queue_data);
    static QueueHandle_t queue;
    static StaticQueue_t buffer;
    static uint8_t storage[queue_length * item_size];

    void init()
    {
        queue = xQueueCreateStatic(queue_length, item_size, storage, &buffer);
        assert_param(queue);
    }

    void process_data()
    {
        static char buffer[32];

        queue_data cmd;
        if (xQueueReceive(queue, &cmd, 0) != pdTRUE) return;

        auto d = &data[static_cast<size_t>(cmd.t)];
        auto s = &cmd.s;

        if (s->enabled)
        {
            snprintf(buffer, sizeof(buffer), "$J=G53 %c%s F%.1f\r", d->designator, s->direction ? d->p_limit : d->n_limit, s->speed);
        }
        else
        {
            buffer[0] = 0x85;
            buffer[1] = 0x00;
        }

        HAL_UART_Transmit(&huart2, reinterpret_cast<uint8_t*>(buffer), strlen(buffer), 100);
    }

    HAL_StatusTypeDef enqueue(axis::types t, const axis::state *s)
    {
        queue_data d = { .t = t, .s = *s };
        return (xQueueSend(queue, &d, 0) == pdPASS) ? HAL_OK : HAL_BUSY;
    }
} // namespace cmd_queue

_BEGIN_STD_C
STATIC_TASK_BODY(MY_IO)
{
    const uint32_t delay = 20;
    static TickType_t last_wake;
    static wdt::task_t* pwdt;

    cmd_queue::init();
    pwdt = wdt::register_task(500, "cmd_io");
    INIT_NOTIFY(MY_IO);

    for (;;)
    {
        cmd_queue::process_data();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(delay));
        pwdt->last_time = xTaskGetTickCount();
    }
}
void cmd_queue_uart_rx_callback(UART_HandleTypeDef *huart)
{
    
}
_END_STD_C