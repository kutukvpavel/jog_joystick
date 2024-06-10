#include "interop.h"

#define QUEUE_LEN 16
#define ITEM_SIZE sizeof(cmd_t)

namespace interop
{
    QueueHandle_t queue;
    
    static StaticQueue_t buffer;
    static uint8_t storage[QUEUE_LEN * ITEM_SIZE];

    void init()
    {
        queue = xQueueCreateStatic(16, ITEM_SIZE, storage, &buffer);
        assert_param(queue);
    }

    HAL_StatusTypeDef enqueue(cmds c, const void* arg)
    {
        if (!queue) return HAL_ERROR;

        cmd_t instance = {
            .c = c,
            .arg = arg
        };
        return (xQueueSend(queue, &instance, 0) == pdPASS) ? HAL_OK : HAL_BUSY; //Copies instance into its own buffer
    }
    HAL_StatusTypeDef try_receive(cmds c, const void** arg)
    {
        if (!queue) return HAL_ERROR;

        cmd_t buf;
        if (xQueuePeek(queue, &buf, 0) != pdTRUE) return HAL_BUSY;
        if (buf.c != c) return HAL_BUSY;
        DBG("Matched interop: %lu, arg ptr = %p", static_cast<uint32_t>(c), buf.arg);
        *arg = buf.arg;
        return (xQueueReceive(queue, &buf, 0) == pdTRUE) ? HAL_OK : HAL_ERROR;
    }
} // namespace interop
