#include "user.h"

#include "wdt.h"
#include "task_handles.h"
#include "interop.h"
#include "i2c_sync.h"

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

DEFINE_STATIC_TASK(MY_CLI, 512);
DEFINE_STATIC_TASK(MY_ADC, 256);
DEFINE_STATIC_TASK(MY_IO, 256);
DEFINE_STATIC_TASK(MY_DISP, 512);
DEFINE_STATIC_TASK(MY_WDT, 256);

_BEGIN_STD_C
void StartMainTask(void *argument)
{
    const uint32_t delay = 20;
    static TickType_t last_wake;
    static wdt::task_t* pwdt;
    static TaskHandle_t handle;

    HAL_IWDG_Refresh(&hiwdg);

    handle = xTaskGetCurrentTaskHandle();
    assert_param(handle);
    interop::init();

    START_STATIC_TASK(MY_CLI, 1, handle);
    HAL_IWDG_Refresh(&hiwdg);
    START_STATIC_TASK(MY_WDT, 1, handle);
    HAL_IWDG_Refresh(&hiwdg);

    DBG("Single-threaded init");
    DBG("I2C Init...");
    if (i2c::init() != HAL_OK) DIE_WITH_CLI("Failed to initialize I2C");
    DBG("USB Init...");
    MX_USB_DEVICE_Init();
    HAL_IWDG_Refresh(&hiwdg);

    DBG("[@ %lu] Starting tasks. Multithreaded init.");
    START_STATIC_TASK(MY_ADC, 1, handle);
    HAL_IWDG_Refresh(&hiwdg);
    START_STATIC_TASK(MY_IO, 1, handle);
    HAL_IWDG_Refresh(&hiwdg);
    START_STATIC_TASK(MY_DISP, 1, handle);

    HAL_IWDG_Refresh(&hiwdg);
    vTaskDelay(pdMS_TO_TICKS(100));

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
enum class states : uint16_t
{
    init,
    manual,
    automatic,
    emergency,
    lamp_test
};
void supervize_led(led_states s);

void user_main(wdt::task_t* pwdt)
{
    static const char* state_names[] = {
        "INIT",
        "MANUAL",
        "AUTO",
        "EMERGENCY",
        "LAMP_TEST"
    };
    static led_states led = led_states::INIT;
    static states state = states::init;
    static states prev_state = states::init;
    static_assert(array_size(state_names) >= (static_cast<uint16_t>(states::lamp_test) + 1));

    /***
     * The following stuff is handled in separate tasks:
     *  debug CLI
     *  STM32 ADC reading
     *  SR IO sync
     *  Coprocessor poll
     *  MAX6675 poll
     *  Display update
     *  Modbus requests
     *  Modbus buffer sync
     * 
     * The following remains for the main task to handle:
     *  Status LED
     *  Manual pump speed update
     *  State machine
    */
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
        LL_GPIO_SetOutputPin(OUT_LED_GPIO_Port, OUT_LED_Pin);
        break;
    default:
        LL_GPIO_ResetOutputPin(OUT_LED_GPIO_Port, OUT_LED_Pin);
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