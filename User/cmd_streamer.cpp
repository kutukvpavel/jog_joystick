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
        const char ok_response[] = "ok";

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
            assert_param(sizeof(echo_buf) >= get_available());

            while (c = getc())
            {
                if (c == '\r') continue;
                echo_buf[i++] = c;
                if (c == '\n') break;
            }
            echo_buf[i] = '\0';
            CDC_Transmit_FS(reinterpret_cast<uint8_t*>(echo_buf), i + 1);

            //Exclude '\n from now on
            if (--i != (sizeof(ok_response)) - 1) return HAL_ERROR;
            return (strncmp(echo_buf, ok_response, i) == 0) ? HAL_OK : HAL_ERROR;
        }
    } // namespace receiver

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
    static TickType_t waiting_start_time = configINITIAL_TICK_COUNT;
    static uint32_t error_responses = 0;
    static uint32_t ok_responses = 0;

    HAL_StatusTypeDef init()
    {
        mutex_handle = xSemaphoreCreateMutexStatic(&mutex_buffer);
        assert_param(mutex_handle);
        return receiver::init();
    }

    void validate_response()
    {
        if (!receiver::get_available()) return;
        if (receiver::validate_response() != HAL_OK) error_responses++;
        else 
        {
            state = transmitter_state::ready;
            ok_responses++;
        }
    }
    uint32_t get_error_count()
    {
        return error_responses;
    }
    uint32_t get_ok_count()
    {
        return ok_responses;
    }
    transmitter_state get_state()
    {
        return state;
    }

    void stream_next()
    {
        static char jog_buffer[64] = "$J=G91 "; //7 characters
        static char cancel_buffer[] = { 0x85, 0x00 };
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

        char* transmit_ptr = NULL;
        if (active) //Stream jog commands
        {
            total_feed_rate = PER_MIN(sqrtf(total_feed_rate));
            size_t index = 7; //constant part of jog command line

            for (size_t i = 0; i < TOTAL_AXES; i++)
            {
                auto& a = data[i];
                if (a.jog_step == 0) continue;

                index += snprintf(jog_buffer + index, sizeof(jog_buffer) - index, "%c%.3f ", a.designator, a.jog_step);
            }
            snprintf(jog_buffer + index, sizeof(jog_buffer) - index, "F%.0f\n", total_feed_rate);

            transmit_ptr = jog_buffer;
            state = transmitter_state::waiting_for_ack;
            waiting_start_time = xTaskGetTickCount();
        }
        else if (prev_active) //Abort jog
        {
            transmit_ptr = cancel_buffer;
        }
        prev_active = active;

        if (transmit_ptr) 
        {
            size_t len = strlen(transmit_ptr);
            HAL_UART_Transmit_IT(&huart2, reinterpret_cast<uint8_t*>(transmit_ptr), len);
            CDC_Transmit_FS(reinterpret_cast<uint8_t*>(transmit_ptr), len);
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
            if (ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(20)) > 0)
            {
                cmd_streamer::validate_response();
            }
            else
            {
                if ((xTaskGetTickCount() - cmd_streamer::waiting_start_time) > pdMS_TO_TICKS(1000))
                {
                    cmd_streamer::state = cmd_streamer::transmitter_state::ready;
                }
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
        if (cmd_streamer::receiver::UART_ByteBuffer == '\n')
        {
            BaseType_t woken;
            vTaskNotifyGiveFromISR(STATIC_TASK_HANDLE(MY_IO), &woken);
            portYIELD_FROM_ISR(woken);
        }
    }

    HAL_UART_Receive_IT(&huart2, &cmd_streamer::receiver::UART_ByteBuffer, 1);
}
_END_STD_C