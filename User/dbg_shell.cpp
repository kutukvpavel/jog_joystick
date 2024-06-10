#include "user.h"

#include "ushell/inc/sys_command_line.h"
#include "../Core/Inc/usart.h"
#include "task_handles.h"
#include "wdt.h"
#include "i2c_sync.h"
#include "a_io.h"
#include "interop.h"

static void init();

_BEGIN_STD_C
STATIC_TASK_BODY(MY_CLI)
{
    static wdt::task_t* pwdt;

    //Init debug commands
    init();
    pwdt = wdt::register_task(1000, "dbg");
    INIT_NOTIFY(MY_CLI);

    //run loop
    for (;;)
    {
        pwdt->last_time = xTaskGetTickCount();
        CLI_RUN();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
_END_STD_C

namespace cli_commands
{
    //Common API

    //Actual commands
    uint8_t info(int argc, char** argv)
    {
        puts(MY_FIRMWARE_INFO_STR);
        return 0;
    }
    uint8_t peripherals_report(int argc, char** argv)
    {
        printf("\n\tA_IO state:\n"
            "\t\tOverall = %f\n"
            "\t\tSpindle = %f\n"
            "\t\tX = %f\n"
            "\t\tY = %f\n"
            "\t\tZ = %f\n"
            "\t\tA = %f\n"
            "\t\tVref = %f\n",
            a_io::get_input(a_io::in::overall_speed),
            a_io::get_input(a_io::in::spindle_speed),
            a_io::get_input(a_io::in::x_speed),
            a_io::get_input(a_io::in::y_speed),
            a_io::get_input(a_io::in::z_speed),
            a_io::get_input(a_io::in::a_speed),
            a_io::get_input(a_io::in::vref));
        return 0;
    }
    uint8_t os_report(int argc, char** argv)
    {
#if configUSE_TRACE_FACILITY
        static const TaskHandle_t* tasks[] =
        {
            &STATIC_TASK_HANDLE(MY_ADC),
            &STATIC_TASK_HANDLE(MY_IO),
            &STATIC_TASK_HANDLE(MY_DISP),
            &STATIC_TASK_HANDLE(MY_CLI),
            &STATIC_TASK_HANDLE(MY_WDT)
        };

        printf("\tMinimum ever free heap = %u\n", xPortGetMinimumEverFreeHeapSize());
        for (size_t i = 0; i < array_size(tasks); i++)
        {
            TaskStatus_t details;
            vTaskGetInfo(*(tasks[i]), &details, pdTRUE, eRunning);
            printf("\tTask %s:\n"
                "\t\tStack high water mark = %hu\n",
                details.pcTaskName,
                details.usStackHighWaterMark
            );
        }
        return 0;
#else
        puts("FreeRTOS trace facility is disabled for release builds.");
        return 1;
#endif
    }
} // namespace cli_commands

void init()
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    CLI_INIT(&huart2);

    CLI_ADD_CMD("info", "Get device info", &cli_commands::info);

    CLI_ADD_CMD("periph_report", "Report peripheral state", &cli_commands::peripherals_report);
    CLI_ADD_CMD("os_report", "Report FreeRTOS stats", &cli_commands::os_report);
}