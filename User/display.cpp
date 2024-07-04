#include "display.h"

#include "task_handles.h"
#include "wdt.h"
#include "LCD/LiquidCrystal_PCF8574.h"

namespace display
{
    struct display_data
    {
        float speed;
        state s;
        bool direction;
    };

    const size_t display_width = 20;
    const size_t display_heigth = 4;
    const size_t line_buffer_length = display_width + 1;
    static char speed_buffer[line_buffer_length];
    static char state_buffer[line_buffer_length];
    static display_data data[TOTAL_AXES] = { };
    static SemaphoreHandle_t data_mutex = NULL;
    static StaticSemaphore_t data_mutex_buffer;
    static LiquidCrystal_PCF8574* lcd = new LiquidCrystal_PCF8574(0b01111110);

    void init()
    {
        DBG("Display init...");
        data_mutex = xSemaphoreCreateMutexStatic(&data_mutex_buffer);
        assert_param(data_mutex);
        speed_buffer[line_buffer_length - 1] = '\0';
        state_buffer[line_buffer_length - 1] = '\0';
        lcd->begin(display_width, display_heigth);
        //Print labels
        lcd->clear();
        lcd->home();
        lcd->print("  X  " "  Y  " "  Z  " "  A  ");
        const char units[] = "mm/s";
        lcd->setCursor(display_width - (sizeof(units) - 1), display_heigth - 1);
        lcd->print(units);
    }

    HAL_StatusTypeDef set_axis_state(axis::types t, state s, bool direction, float speed)
    {
        if (xSemaphoreTake(data_mutex, 0) != pdTRUE) return HAL_BUSY;

        auto a = &data[static_cast<size_t>(t)];
        a->s = s;
        a->direction = direction;
        a->speed = speed;

        xSemaphoreGive(data_mutex);
        return HAL_OK;
    }
    void set_initialized()
    {
        lcd->setBacklight(255);
    }

    void process_data()
    {
        const size_t field_len = (line_buffer_length - 1) / TOTAL_AXES;
        static_assert(field_len == 5); //Change following line if field length changes:
        const char speed_format[] = "%4.1f ";
        const char state_idle[field_len + 1] = "     ";
        const char state_jog_n[field_len + 1] = "  -  ";
        const char state_jog_p[field_len + 1] = "  +  ";
        const char state_jog_fast_n[field_len + 1] = " --- ";
        const char state_jog_fast_p[field_len + 1] = " +++ ";

        while (xSemaphoreTake(data_mutex, portMAX_DELAY) != pdTRUE);

        for (size_t i = 0; i < TOTAL_AXES; i++)
        {
            snprintf(speed_buffer + field_len * i, field_len + 1, speed_format, data[i].speed);
            const char* state_designator;
            switch (data[i].s)
            {
            case state::jog:
                state_designator = data[i].direction ? state_jog_p : state_jog_n;
                break;
            case state::jog_fast:
                state_designator = data[i].direction ? state_jog_fast_p : state_jog_fast_n;
                break;
            default:
                state_designator = state_idle;
                break;
            }
            strncpy(state_buffer + field_len * i, state_designator, field_len + 1);
        }

        xSemaphoreGive(data_mutex);

        //Write 2 lines to LCD
        lcd->setCursor(0, 1);
        lcd->print(speed_buffer);
        lcd->setCursor(0, 2);
        lcd->print(state_buffer);
    }
} // namespace display

_BEGIN_STD_C
STATIC_TASK_BODY(MY_DISP)
{
    const uint32_t delay = 100;
    static TickType_t last_wake;
    static wdt::task_t* pwdt;

    display::init();
    pwdt = wdt::register_task(500, "display");
    INIT_NOTIFY(MY_DISP);

    last_wake = xTaskGetTickCount();
    for (;;)
    {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(delay));
        display::process_data();
        pwdt->last_time = xTaskGetTickCount();
    }
}
_END_STD_C