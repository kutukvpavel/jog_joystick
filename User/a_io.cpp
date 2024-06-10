#include "a_io.h"

#include "task_handles.h"
#include "average/average.h"
#include "../Core/Inc/adc.h"
#include "../Drivers/STM32F1xx_HAL_Driver/Inc/stm32f1xx_ll_adc.h"
#include "../Core/Inc/tim.h"
#include "wdt.h"

#define PACKED_FOR_DMA __packed __aligned(sizeof(uint32_t))
#define CHANNEL_REPETITION 1
#define MAP(i) { &buffer[i].ch_4, &buffer[i].ch_5, &buffer[i].ch_6, &buffer[i].ch_7, &buffer[i].ch_8, &buffer[i].ch_9, &buffer[i].vref }

namespace a_io
{
    struct PACKED_FOR_DMA a_buffer_t
    {
        uint16_t ch_4;
        uint16_t ch_5;
        uint16_t ch_6;
        uint16_t ch_7;
        uint16_t ch_8;
        uint16_t ch_9;
        uint16_t vref;
    };
    volatile a_buffer_t buffer[CHANNEL_REPETITION] = { };
    volatile uint16_t* mappings[in::LEN][CHANNEL_REPETITION] = MAP(0);
    struct channel
    {
        average* a;
        float v;
    };
    channel ch[in::LEN];

    void init()
    {
        DBG("A_IO init...");
        for (size_t i = 0; i < in::LEN; i++)
        {
            ch[i].a = new average(10);
        }
        //Here sizeof()-magic is not intended to compute array size, ignore the warning
        //[ instead it computes whole DMA buffer length in half-words ]
        HAL_ADC_Start_DMA(&hadc1, const_cast<uint32_t*>(reinterpret_cast<volatile uint32_t*>(&buffer)), sizeof(buffer) / sizeof(uint16_t));
        HAL_TIM_Base_Start(&htim3); //Start ADC trigger timer
    }

    void process_data()
    {
        for (size_t i = 0; i < CHANNEL_REPETITION; i++)
        {
            ch[in::vref].a->enqueue(*(mappings[i][in::vref]) * ( 3.3f / 0xFFFu ));
        }
        ch[in::vref].v = ch[in::vref].a->get_average();
        for (size_t j = 0; j < CHANNEL_REPETITION; j++)
        {
            for (size_t i = 0; i < in::LEN; i++)
            {
                if (i == in::vref) continue;
                ch[i].a->enqueue(__LL_ADC_CALC_DATA_TO_VOLTAGE(ch[in::vref].v, *(mappings[j][i]), LL_ADC_RESOLUTION_12B));
            }
        }
    }

    float get_input(in i)
    {
        assert_param(i < in::LEN);

        return ch[i].v;
    }
} // namespace a_io

_BEGIN_STD_C
STATIC_TASK_BODY(MY_ADC)
{
    static wdt::task_t* pwdt;

    a_io::init();
    pwdt = wdt::register_task(500, "a_io");
    INIT_NOTIFY(MY_ADC);

    for (;;)
    {
        if (!ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20))) continue;
        a_io::process_data();
        pwdt->last_time = xTaskGetTickCount();
    }
}
_END_STD_C