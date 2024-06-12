#include "user.h"

#include "wdt.h"
#include "task_handles.h"
#include "i2c_sync.h"
#include "axis.h"
#include "cmd_streamer.h"
#include "display.h"
#include "nvs.h"
#include "compat_api.h"

#include "../USB_DEVICE/App/usb_device.h"
#include "../Core/Inc/iwdg.h"

#define DEFINE_STATIC_TASK(name, stack_size) \
    StaticTask_t task_buffer_##name; \
    StackType_t task_stack_##name [stack_size]; \
    TaskHandle_t task_handle_##name = NULL
#define START_STATIC_TASK(name, priority, arg) \
    task_handle_##name = xTaskCreateStatic(start_task_##name, #name, array_size(task_stack_##name), &(arg), \
    priority, task_stack_##name, &task_buffer_##name); \
    ulTaskNotifyTake(pdFALSE, portMAX_DELAY)
#define DIE_WITH_CLI(m) do { ERR(m); while (true) { HAL_IWDG_Refresh(&hiwdg); vTaskDelay(pdMS_TO_TICKS(100)); } } while (0);

static inline void user_main(wdt::task_t* pwdt);

DEFINE_STATIC_TASK(MY_CLI, 256);
DEFINE_STATIC_TASK(MY_ADC, 128);
DEFINE_STATIC_TASK(MY_IO, 128);
DEFINE_STATIC_TASK(MY_DISP, 256);
DEFINE_STATIC_TASK(MY_WDT, 128);

static void die_init_failed();

_BEGIN_STD_C
void StartDefaultTask(void *argument)
{
    const uint32_t delay = 5;
    static TickType_t last_wake;
    static wdt::task_t* pwdt;
    static TaskHandle_t handle;

    HAL_IWDG_Refresh(&hiwdg);
    LL_GPIO_ResetOutputPin(OUT_LED_GPIO_Port, OUT_LED_Pin);

    handle = xTaskGetCurrentTaskHandle();
    assert_param(handle);

    START_STATIC_TASK(MY_CLI, 1, handle);
    HAL_IWDG_Refresh(&hiwdg);
    START_STATIC_TASK(MY_WDT, 3, handle);
    HAL_IWDG_Refresh(&hiwdg);

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) ERR("IWDG reset detected!");
    __HAL_RCC_CLEAR_RESET_FLAGS();

    DBG("Single-threaded init");
    DBG("Compat API Init...");
    if (compat_api_init() != HAL_OK) die_init_failed();
    DBG("I2C Init...");
    if (i2c::init() != HAL_OK) die_init_failed();
    DBG("NVS Init...");
    if (nvs::init() != HAL_OK) die_init_failed();
    if (nvs::load() != HAL_OK) DIE_WITH_CLI("Failed to load NVS data");
    DBG("USB Init...");
    MX_USB_DEVICE_Init();
    HAL_IWDG_Refresh(&hiwdg);

    DBG("Starting tasks. Multithreaded init.");
    START_STATIC_TASK(MY_ADC, 2, handle);
    HAL_IWDG_Refresh(&hiwdg);
    START_STATIC_TASK(MY_IO, 2, handle);
    HAL_IWDG_Refresh(&hiwdg);
    START_STATIC_TASK(MY_DISP, 1, handle);

    HAL_IWDG_Refresh(&hiwdg);
    vTaskDelay(pdMS_TO_TICKS(100));
    LL_GPIO_SetOutputPin(OUT_LED_GPIO_Port, OUT_LED_Pin);

    pwdt = wdt::register_task(2000, "main");
    last_wake = xTaskGetTickCount();
    for (;;)
    {
        user_main(pwdt);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(delay));
        pwdt->last_time = xTaskGetTickCount();
    }
}
_END_STD_C

enum class led_states
{
    HEARTBEAT,
    ERROR,
    COMMUNICATION,
    INIT
};
void supervize_led(led_states s);

void user_main(wdt::task_t* pwdt)
{
    static led_states led = led_states::HEARTBEAT;

    /***
     * The following stuff is handled in separate tasks:
     *  debug CLI
     *  STM32 ADC reading
     *  Display update
     *  FluidNC command queue
     *
     * The following remains for the main task to handle:
     *  Status LED
     *  Digital input poll
     *  State machine and command issuing
     */

    bool fast = axis::get_fast();
    display::state jog_state = fast ? display::state::jog_fast : display::state::jog;

    for (size_t i = 0; i < static_cast<size_t>(axis::types::LEN); i++)
    {
        auto a = static_cast<axis::types>(i);
        auto s = axis::poll(a);

        //Rapid feed override
        if (fast) s.speed = nvs::get_rapid_speed(a);

        display::set_axis_state(a, s.enabled ? jog_state : display::state::idle, s.direction, s.speed);

        cmd_streamer::set_axis_state(a, &s);
    }

    supervize_led(led);
}

void supervize_led(led_states s)
{
    enum class substates
    {
        on, off, blinking
    };

    const uint32_t heartbeat_delay_ms = 1000;
    const uint32_t comm_delay_ms = 200;
    const uint32_t comm_repetitions = 3;

    static TickType_t next_toggle = 0;
    static substates substate = substates::off;
    static int next_substate_switch = 0;
    static led_states prev_state = led_states::INIT;
    static TickType_t current_toggle_period = 0;

    bool state_change = (prev_state != s);
    TickType_t tick = xTaskGetTickCount();

    if ((tick < next_toggle) && !state_change) return;
    switch (substate)
    {
    case substates::blinking:
        HAL_GPIO_TogglePin(OUT_LED_GPIO_Port, OUT_LED_Pin);
        next_toggle = tick + current_toggle_period;
        break;
    case substates::on:
        LL_GPIO_ResetOutputPin(OUT_LED_GPIO_Port, OUT_LED_Pin);
        break;
    default:
        LL_GPIO_SetOutputPin(OUT_LED_GPIO_Port, OUT_LED_Pin);
        break;
    }

    if ((--next_substate_switch > 0) && !state_change) return;
    if (next_substate_switch < 0) next_substate_switch = 0;
    switch (s)
    {
    case led_states::HEARTBEAT:
        substate = substates::blinking;
        current_toggle_period = heartbeat_delay_ms;
        break;
    case led_states::ERROR:
        substate = substates::on;
        break;
    case led_states::COMMUNICATION:
        current_toggle_period = comm_delay_ms;
        next_substate_switch = comm_repetitions + 1;
        if (substate == substates::blinking)
        {
            substate = substates::off;
        }
        else
        {
            substate = substates::blinking;
        }
        break;
    case led_states::INIT:
        substate = substates::off;
    default:
        break;
    }
}

void die_init_failed()
{
    DIE_WITH_CLI("Init failed!");
}