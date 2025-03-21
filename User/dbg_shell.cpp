#include "user.h"

#include "ushell/inc/sys_command_line.h"
#include "../Core/Inc/usart.h"
#include "task_handles.h"
#include "wdt.h"
#include "i2c_sync.h"
#include "a_io.h"
#include "nvs.h"
#include "axis.h"
#include "cmd_streamer.h"

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
    const char axis_letters[] = { 'X' };

    uint8_t parse_and_set_axis_speed(int argc, char** argv, void (*set_func)(axis::types, float))
    {
        if (argc < 3) return 1;
        uint32_t axis;
        if (sscanf(argv[1], "%" SCNu32, &axis) != 1) return 2;
        if (axis >= static_cast<uint32_t>(axis::types::LEN)) return 3;
        float speed;
        if (sscanf(argv[2], "%f", &speed) != 1) return 2;
        set_func(static_cast<axis::types>(axis), speed);
        return 0;
    }

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

    uint8_t nvs_save(int argc, char** argv)
    {
        return nvs::save();
    }
    uint8_t nvs_reset(int argc, char** argv)
    {
        return nvs::reset();
    }
    uint8_t nvs_test(int argc, char** argv)
    {
        return nvs::test();
    }
    uint8_t nvs_load(int argc, char** argv)
    {
        return nvs::load();
    }
    uint8_t nvs_dump(int argc, char** argv)
    {
        nvs::dump_hex();
        return 0;
    }
    uint8_t nvs_report(int argc, char** argv)
    {
        printf(
            "\tNVS ver: stored = %hu, required = %hu; %s\n",
            nvs::get_stored_version(),
            nvs::get_required_version(),
            nvs::get_version_match() ? "MATCH" : "DEFAULTS USED!"
        );
        puts("\tAxis rapid feed rate:");
        for (size_t i = 0; i < TOTAL_AXES; i++)
        {
            printf("\t\t%c: %f\n", axis_letters[i], nvs::get_rapid_speed(static_cast<axis::types>(i)));
        }
        puts("\tAxis max feed rate:");
        for (size_t i = 0; i < TOTAL_AXES; i++)
        {
            printf("\t\t%c: %f\n", axis_letters[i], nvs::get_max_speed(static_cast<axis::types>(i)));
        }
        puts("\tAxis min feed rate:");
        for (size_t i = 0; i < TOTAL_AXES; i++)
        {
            printf("\t\t%c: %f\n", axis_letters[i], nvs::get_min_speed(static_cast<axis::types>(i)));
        }
        printf("\tPot low threshold: %f\n", nvs::get_low_pot_threshold());
        
        return 0;
    }
    uint8_t hw_report(int argc, char** argv)
    {
        puts("\tAxes jog speeds:");
        for (size_t i = 0; i < TOTAL_AXES; i++)
        {
            float s = cmd_streamer::get_axis_jog_speed(static_cast<axis::types>(i));
            printf("\t\t%c: %f\n", axis_letters[i], s);
        }
        printf("\tNC error responses count = %lu\n", cmd_streamer::get_error_count());
        printf("\tNC OK responses count = %lu\n", cmd_streamer::get_ok_count());
        printf("\tNC response timeouts = %lu\n", cmd_streamer::get_timeouts());
        printf("\tStreamer state = %lu\n", static_cast<uint32_t>(cmd_streamer::get_state()));

        return 0;
    }
    uint8_t usb_test(int argc, char** argv)
    {
        printf("\tCDC Connected = %" PRIu16 "\n\tCDC can transmit = %" PRIu16 "\n",
            static_cast<uint16_t>(CDC_IsConnected()), static_cast<uint16_t>(CDC_Can_Transmit()));
        if (CDC_Can_Transmit() == USBD_OK) CDC_PUTS("Hello, USB");

        return 0;
    }

    uint8_t set_rapid_speed(int argc, char** argv)
    {
        return parse_and_set_axis_speed(argc, argv, nvs::set_rapid_speed);
    }
    uint8_t set_max_speed(int argc, char** argv)
    {
        return parse_and_set_axis_speed(argc, argv, nvs::set_max_speed);
    }
    uint8_t set_min_speed(int argc, char** argv)
    {
        return parse_and_set_axis_speed(argc, argv, nvs::set_min_speed);
    }
    uint8_t set_pot_low(int argc, char** argv)
    {
        if (argc < 2) return 1;
        float threshold;
        if (sscanf(argv[1], "%f", &threshold) != 1) return 2;
        if (threshold < 0 || threshold > 0.2) return 3;
        nvs::set_low_pot_threshold(threshold);
        return 0;
    }
} // namespace cli_commands

void init()
{
    static_assert(sizeof(cli_commands::axis_letters) == TOTAL_AXES);

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    CLI_INIT(&huart3);

    CLI_ADD_CMD("info", "Get device info", &cli_commands::info);

    CLI_ADD_CMD("periph_report", "Report peripheral state", &cli_commands::peripherals_report);
    CLI_ADD_CMD("os_report", "Report FreeRTOS stats", &cli_commands::os_report);

    CLI_ADD_CMD("nvs_save", "Save current non-volatile data into EEPROM", &cli_commands::nvs_save);
    CLI_ADD_CMD("nvs_load", "Load non-volatile data from EEPROM", &cli_commands::nvs_load);
    CLI_ADD_CMD("nvs_dump", "Perform EEPROM HEX-dump", &cli_commands::nvs_dump);
    CLI_ADD_CMD("nvs_reset", "Reset NVS (sets NVS partiton version to 0 [invalid], doesn't actually erase the EEPROM)",
        &cli_commands::nvs_reset);
    CLI_ADD_CMD("nvs_test", "Test EEPROM readback, performs sequential number write and read, and does nvs_save afterwards",
        &cli_commands::nvs_test);
    CLI_ADD_CMD("nvs_report", "Report NVS contents in human-readable format", &cli_commands::nvs_report);

    CLI_ADD_CMD("hw_report", "Report HW state", &cli_commands::hw_report);

    CLI_ADD_CMD("usb_test", "Report USB state and send a test string", &cli_commands::usb_test);

    CLI_ADD_CMD("set_rapid_speed", "Set rapid feed speed for axis, expects axis index (int) and a float", 
        &cli_commands::set_rapid_speed);
    CLI_ADD_CMD("set_max_speed", "Set max feed rate for axis, expects axis index (int) and a float", 
        &cli_commands::set_max_speed);
    CLI_ADD_CMD("set_min_speed", "Set min feed rate for axis, expects axis index (int) and a float", 
        &cli_commands::set_min_speed);
    CLI_ADD_CMD("set_pot_low", "Set potentiometer blanking distance from 0 (to suppress uneven region near track end)", 
        &cli_commands::set_pot_low);
}