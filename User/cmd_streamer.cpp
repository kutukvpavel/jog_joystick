#include "cmd_streamer.h"

#include "task_handles.h"
#include "wdt.h"

#include "../Core/Inc/usart.h"
#include "../USB_DEVICE/App/usbd_cdc_if.h"

#include "math.h"

#define UART_RX_BUFFER_SIZE 128
#define UART_RX_BUFFER_MASK ( UART_RX_BUFFER_SIZE - 1)
#define TAU_MS 25.0f
#define PER_MIN(per_s) ((per_s) * 60)

namespace cmd_streamer
{
    namespace receiver
    {
        const char ok_response[] = "ok\r";

        static uint8_t UART_RxBuf[UART_RX_BUFFER_SIZE];
        static size_t UART_RxHead = 0;
        static size_t UART_RxTail = 0;
        static uint8_t UART_ByteBuffer;
        static uint32_t BufferOverrunEvents = 0;

        HAL_StatusTypeDef init()
        {
            return HAL_UART_Receive_IT(&huart2, &UART_ByteBuffer, 1);
        }

        size_t get_available()
        {
            return (UART_RxHead - UART_RxTail) & UART_RX_BUFFER_MASK;
        }

        char getc()
        {
            if ( UART_RxHead == UART_RxTail ) return '\0';
            UART_RxTail = (UART_RxTail + 1) & UART_RX_BUFFER_MASK;
            return static_cast<char>(UART_RxBuf[UART_RxTail]);
        }

        HAL_StatusTypeDef validate_response()
        {
            static char echo_buf[64];

            char c;
            size_t i = 0;
            size_t len = get_available();
            assert_param(sizeof(echo_buf) >= len);

            while (c = getc())
            {
                echo_buf[i++];
            }
            echo_buf[i] = '\n';
            echo_buf[i + 1] = '\0';
            CDC_Transmit_FS(reinterpret_cast<uint8_t*>(echo_buf), len + 1);

            if (len != (sizeof(ok_response)) - 1) return HAL_ERROR;
            return (strncmp(echo_buf, ok_response, len) == 0) ? HAL_OK : HAL_ERROR;
        }
    } // namespace receiver

    enum class transmitter_state
    {
        ready,
        waiting_for_ack
    };
    struct axis_cmd_data
    {
        char designator;
        float jog_speed; //mm/s
        float jog_step; //mm
    };
    static axis_cmd_data data[TOTAL_AXES] = { 
        {
            .designator = 'X'
        },
        {
            .designator = 'Y'
        },
        {
            .designator = 'Z'
        },
        {
            .designator = 'A'
        }
    };
    static SemaphoreHandle_t mutex_handle = NULL;
    static StaticSemaphore_t mutex_buffer;
    static transmitter_state state = transmitter_state::ready;
    static uint32_t error_responses = 0;

    HAL_StatusTypeDef init()
    {
        mutex_handle = xSemaphoreCreateMutexStatic(&mutex_buffer);
        assert_param(mutex_handle);
        return receiver::init();
    }

    void validate_response()
    {
        if (receiver::validate_response() != HAL_OK) error_responses++;
        else state = transmitter_state::ready;
    }

    void stream_next()
    {
        static char buffer[64] = "$J=G91 "; //7 characters
        static bool prev_active = false;

        bool active = false;
        float total_feed_rate = 0;

        while (xSemaphoreTake(mutex_handle, portMAX_DELAY) != pdTRUE);

        for (size_t i = 0; i < TOTAL_AXES; i++)
        {
            auto& a = data[i];
            if (a.jog_speed != 0)
            {
                total_feed_rate += a.jog_speed * a.jog_speed;
                a.jog_step = a.jog_speed * (TAU_MS / 1000);
                active = true;
            }
            else
            {
                a.jog_step = 0;
            }
        }

        xSemaphoreGive(mutex_handle);

        bool transmit = false;

        if (active) //Stream jog commands
        {
            total_feed_rate = PER_MIN(sqrtf(total_feed_rate));
            size_t index = 7; //constant part of jog command line
            for (size_t i = 0; i < TOTAL_AXES; i++)
            {
                auto& a = data[i];
                if (a.jog_step == 0) continue;

                index += snprintf(buffer + index, sizeof(buffer) - index, "%c%.2f ", a.designator, a.jog_step);
            }
            snprintf(buffer + index, sizeof(buffer) - index, "F%.0f\r", total_feed_rate);
            transmit = true;
        }
        else if (prev_active) //Abort jog
        {
            buffer[0] = 0x85;
            buffer[1] = 0x00;
            transmit = true;
        }
        prev_active = active;

        if (transmit) 
        {
            state = transmitter_state::waiting_for_ack;
            size_t len = strlen(buffer);
            HAL_UART_Transmit_IT(&huart2, reinterpret_cast<uint8_t*>(buffer), len);
            CDC_Transmit_FS(reinterpret_cast<uint8_t*>(buffer), len);
        }
    }

    HAL_StatusTypeDef set_axis_state(axis::types t, const axis::state *s)
    {
        auto a = &data[static_cast<size_t>(t)];

        if (xSemaphoreTake(mutex_handle, 0) != pdTRUE) return HAL_BUSY;

        a->jog_speed = s->enabled ? (s->speed * (s->direction ? 1 : -1)) : 0;

        xSemaphoreGive(mutex_handle);
        return HAL_OK;
    }

    float get_axis_jog_speed(axis::types t)
    {
        auto a = &data[static_cast<size_t>(t)];

        return a->jog_speed;
    }
} // namespace cmd_streamer

_BEGIN_STD_C
STATIC_TASK_BODY(MY_IO)
{
    const TickType_t delay = pdMS_TO_TICKS(TAU_MS);
    static wdt::task_t* pwdt;
    static TickType_t last_wake;

    cmd_streamer::init();
    pwdt = wdt::register_task(500, "cmd_io");
    INIT_NOTIFY(MY_IO);

    last_wake = xTaskGetTickCount();
    for (;;)
    {
        switch (cmd_streamer::state)
        {
        case cmd_streamer::transmitter_state::ready:
            cmd_streamer::stream_next();
            break;
        case cmd_streamer::transmitter_state::waiting_for_ack:
            if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20)) > 0)
            {
                cmd_streamer::validate_response();
            }
            //last_wake = xTaskGetTickCount();
            break;
        }
        vTaskDelayUntil(&last_wake, delay);
        pwdt->last_time = xTaskGetTickCount();
    }
}
void cmd_queue_uart_rxcplt_callback(UART_HandleTypeDef *huart)
{
    if (huart != &huart2) return;

    size_t tmphead;
    /* calculate buffer index */
    tmphead = (cmd_streamer::receiver::UART_RxHead + 1) & UART_RX_BUFFER_MASK;

    if (tmphead == cmd_streamer::receiver::UART_RxTail)
    {
        /* error: receive buffer overflow */
        cmd_streamer::receiver::BufferOverrunEvents++;
        cmd_streamer::receiver::UART_RxHead = 0;
        cmd_streamer::receiver::UART_RxTail = 0;
    }
    else
    {
        /* store new index */
        cmd_streamer::receiver::UART_RxHead = tmphead;
        /* store received data in buffer */
        cmd_streamer::receiver::UART_RxBuf[tmphead] = cmd_streamer::receiver::UART_ByteBuffer;
        if (cmd_streamer::receiver::UART_ByteBuffer == '\r')
        {
            BaseType_t woken;
            vTaskNotifyGiveFromISR(STATIC_TASK_HANDLE(MY_IO), &woken);
            portYIELD_FROM_ISR(woken);
        }
    }

    HAL_UART_Receive_IT(&huart2, &cmd_streamer::receiver::UART_ByteBuffer, 1);
}
_END_STD_C