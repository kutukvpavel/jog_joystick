#include "user.h"

#include "ushell/inc/sys_command_line.h"
#include "../Core/Inc/usart.h"
#include "task_handles.h"
#include "wdt.h"
#include "nvs.h"
#include "coprocessor.h"
#include "i2c_sync.h"
#include "sr_io.h"
#include "a_io.h"
#include "pumps.h"
#include "thermo.h"
#include "interop.h"
#include "modbus_regs.h"
#include "display.h"
#include "ethernet.h"
#include "../Core/Inc/dfu.h"

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
    void print_sr_reg_core(sr_buf_t* src, size_t len)
    {
        for (size_t i = 0; i < len; i++)
        {
            printf("\tWord #%u = 0x%X = 0b ", i, src[i]);
            for (size_t j = 0; j < (sizeof(sr_buf_t) * __CHAR_BIT__); j++)
            {
                putc((src[i] & (1u << (sizeof(sr_buf_t) * __CHAR_BIT__ - j - 1))) ? '1' : '0', stdout);
                if (j % 8 == 7) putc(' ', stdout);
            }
            putc('\n', stdout);
        }
    }
    void print_input_invert()
    {
        sr_buf_t* inv = nvs::get_input_inversion();
        print_sr_reg_core(inv, sr_io::input_buffer_len);
    }
    void print_remote_output_mask()
    {
        sr_buf_t* mask = nvs::get_remote_output_mask();
        print_sr_reg_core(mask, sr_io::output_buffer_len);
    }

    //Actual commands
    uint8_t info(int argc, char** argv)
    {
        puts(MY_FIRMWARE_INFO_STR);
        return 0;
    }
    uint8_t peripherals_report(int argc, char** argv)
    {
        fputs("\tSR_IO state:\n\t\tInputs = 0b ", stdout);
        for (size_t i = 0; i < sr_io::in::IN_LEN; i++)
        {
            putc(sr_io::get_input(static_cast<sr_io::in>(sr_io::in::IN_LEN - i - 1)) ? '1' : '0', stdout);
            if (i % 8 == 7) putc(' ', stdout);
        }
        fputs("\n\t\tOutputs = 0b ", stdout);
        for (size_t i = 0; i < sr_io::out::OUT_LEN; i++)
        {
            putc(sr_io::get_output(static_cast<sr_io::out>(sr_io::out::OUT_LEN - i - 1)) ? '1' : '0', stdout);
            if (i % 8 == 7) putc(' ', stdout);
        }
        printf("\n\tA_IO state:\n"
            "\t\tAmbient = %f\n"
            "\t\tThermocouple = %f\n"
            "\t\tVbat = %f\n"
            "\t\tVref = %f\n",
            a_io::get_input(a_io::in::ambient_temp),
            a_io::get_input(a_io::in::analog_thermocouple),
            a_io::get_input(a_io::in::vbat),
            a_io::get_input(a_io::in::vref));
        puts("\tMAX6675 Thermocouple state:");
        for (size_t i = 0; i < MY_TEMP_CHANNEL_NUM; i++)
        {
            printf("\t\t#%u = %f, present = %u\n", i, thermo::get_temperatures()[i], thermo::get_thermocouple_present(i));
        }
        return 0;
    }
    uint8_t pumps_report(int argc, char** argv)
    {
        printf("\tPumps enabled: %u\n", pumps::get_enabled());
        for (size_t i = 0; i < MY_PUMPS_NUM; i++)
        {
            printf("\tUnit #%u:\n"
                "\t\tMissing: %u\n"
                "\t\tOverload: %u\n"
                "\t\tIndicated speed = %f\n"
                "\t\tSpeed fraction = %f\n"
                "\t\tLoad fraction = %f\n"
                "\t\tPaused: %u\n"
                "\t\tRunning: %u\n",
                i,
                pumps::get_missing(i),
                pumps::get_overload(i),
                pumps::get_indicated_speed(i),
                pumps::get_speed_fraction(i),
                pumps::get_load_fraction(i),
                pumps::get_paused(i),
                pumps::get_running(i));
        }
        return 0;
    }
    uint8_t pumps_debug(int argc, char** argv)
    {
        pumps::print_debug_info();
        return 0;
    }
    uint8_t coproc_report(int argc, char** argv)
    {
        printf("\tManual override = %f\n", coprocessor::get_manual_override());
        for (size_t i = 0; i < MY_PUMPS_NUM; i++)
        {
            printf("\tUnit #%u:\n"
                "\t\tDrv missing: %u\n"
                "\t\tDrv error: %u\n"
                "\t\tEncoder position = %u\n"
                "\t\tEncoder btn pressed: %u\n"
                "\t\tDrv load err: %f\n",
                i,
                coprocessor::get_drv_missing(i),
                coprocessor::get_drv_error(i),
                coprocessor::get_encoder_value(i),
                coprocessor::get_encoder_btn_pressed(i),
                coprocessor::get_drv_load(i));
        }
        return 0;
    }
    uint8_t os_report(int argc, char** argv)
    {
#if configUSE_TRACE_FACILITY
        static const TaskHandle_t* tasks[] =
        {
            &STATIC_TASK_HANDLE(MY_ADC),
            &STATIC_TASK_HANDLE(MY_COPROC),
            &STATIC_TASK_HANDLE(MY_IO),
            &STATIC_TASK_HANDLE(MY_THERMO),
            &STATIC_TASK_HANDLE(MY_DISP),
            &STATIC_TASK_HANDLE(MY_CLI),
            &STATIC_TASK_HANDLE(MY_MODBUS),
            &STATIC_TASK_HANDLE(MY_FP),
            &STATIC_TASK_HANDLE(MY_ETH),
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
    uint8_t eth_report(int argc, char** argv)
    {
        eth::print_dbg();
        return 0;
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
        puts("\tInput inversion:");
        print_input_invert();
        puts("\tRemote output maks:");
        print_remote_output_mask();
        printf("\tModbus address = %u\n", *nvs::get_modbus_addr());
        printf("\tModbus keepalive threshold = %u\n", *nvs::get_modbus_keepalive_threshold());
        auto motor_params = nvs::get_motor_params();
        auto motor_regs = nvs::get_motor_regs();
        for (size_t i = 0; i < MY_PUMPS_NUM; i++)
        {
            printf("\tMotor #%u params:\n", i);
            auto& mp = motor_params[i];
            printf(
                "\t\tVol.rate-to-RPS coef = %f\n"
                "\t\tTeeth = %u\n"
                "\t\tMicrosteps = %u\n"
                "\t\tDir = %u\n"
                "\t\tMax rate (RPS) = %f\n"
                "\t\tMax load (drv error units) = %f\n",
                mp.volume_rate_to_rps,
                mp.teeth,
                mp.microsteps,
                mp.dir,
                mp.max_rate_rps,
                mp.max_load_err
            );
            printf("\tMotor #%u regs:\n", i);
            auto& mr = motor_regs[i];
            printf(
                "\t\tVol.rate = %f\n"
                "\t\tRPS = %f\n"
                "\t\tError = %f\n"
                "\t\tStatus = %u\n",
                mr.volume_rate,
                mr.rps,
                mr.err,
                mr.status
            );
        }
        puts("\tCommon pump params:");
        auto& pump_params = *nvs::get_pump_params();
        printf(
            "\t\tInvert EN = %u\n"
            "\t\tVol.rate resolution (man) = %f\n",
            pump_params.invert_enable,
            pump_params.volume_rate_resolution
        );
        return 0;
    }
    uint8_t set_modbus_keepalive(int argc, char** argv)
    {
        if (argc < 2) return 1;
        uint16_t v;
        if (sscanf(argv[1], "%hu", &v) != 1) return 2;
        *nvs::get_modbus_keepalive_threshold() = v;
        return 0;
    }
    uint8_t set_modbus_addr(int argc, char** argv)
    {
        if (argc < 2) return 1;
        uint16_t v;
        if (sscanf(argv[1], "%hu", &v) != 1) return 2;
        *nvs::get_modbus_addr() = v;
        return 0;
    }
    uint8_t set_pump_coef(int argc, char** argv)
    {
        if (argc < 2) return 1;

        if (argc > 2)
        {
            size_t i;
            float v;
            if (sscanf(argv[1], "%u", &i) != 1) return 2;
            if (i > MY_PUMPS_MAX) return 3;
            printf("\tWriting pump #%u\n",i);
            if (sscanf(argv[2], "%f", &v) != 1) return 2;
            nvs::get_motor_params()[i].volume_rate_to_rps = v;
        }
        else
        {
            printf("\tWriting all pumps\n");
            float v;
            if (sscanf(argv[1], "%f", &v) != 1) return 2;
            for (size_t i = 0; i < MY_PUMPS_NUM; i++)
            {
                nvs::get_motor_params()[i].volume_rate_to_rps = v;
            }
        }
        return 0;
    }
    uint8_t reload_params(int argc, char** argv)
    {
        pumps::reload_motor_params();
        pumps::reload_params();
        return 0;
    }

    uint8_t get_coproc_err_rate(int argc, char** argv)
    {
        printf("\t%lu\n", coprocessor::get_crc_err_stats());
        return 0;
    }
    uint8_t get_coproc_init(int argc, char** argv)
    {
        puts(coprocessor::get_initialized() ? "\tYes" : "\tNo");
        return 0;
    }
    uint8_t coproc_reinit(int argc, char** argv)
    {
        return coprocessor::reinit();
    }
    uint8_t coproc_scan(int argc, char** argv)
    {
        for (size_t i = 0x08; i < 0x7F; i++)
        {
            HAL_StatusTypeDef ret = i2c::write_byte(i << 1u, 0xA0);
            if (ret == HAL_BUSY) ERR("I2C synchronizing context is busy.");
            if (ret == HAL_OK) printf("\tFound: 0x%x\n", i);
        }
        return 0;
    }
    uint8_t get_thermo_err_rate(int argc, char** argv)
    {
        printf("\t%lu\n", thermo::get_recv_err_rate());
        return 0;
    }

    uint8_t input_invert(int argc, char** argv)
    {
        if (argc < 2)
        {
            print_input_invert();
            return 0;
        }
        
        sr_buf_t* inv = nvs::get_input_inversion();
        if (argc == 2)
        {
            size_t idx;
            if (sscanf(argv[1], "%u", &idx) != 1) return 2;
            *inv ^= (1u << idx);
        }
        else if (argc == (sr_io::input_buffer_len + 2))
        {
            sr_buf_t b[sr_io::input_buffer_len];
            for (size_t i = 0; i < sr_io::input_buffer_len; i++)
            {
                if (sscanf(argv[i + 1], "%hx", &(b[i])) != 1) return 2;
            }
            memcpy(inv, b, sizeof(b));
        }
        else
        {
            return 1;
        }

        print_input_invert();
        return 0;
    }
    uint8_t remote_output_mask(int argc, char** argv)
    {
        if (argc < 2)
        {
            print_remote_output_mask();
            return 0;
        }

        sr_buf_t* mask = nvs::get_remote_output_mask();
        if (argc == 2)
        {
            size_t idx;
            if (sscanf(argv[1], "%u", &idx) != 1) return 2;
            *mask ^= (1u << idx);
        }
        else if (argc == (sr_io::output_buffer_len + 2))
        {
            sr_buf_t b[sr_io::output_buffer_len];
            for (size_t i = 0; i < sr_io::output_buffer_len; i++)
            {
                if (sscanf(argv[i + 1], "%hx", &(b[i])) != 1) return 2;
            }
            memcpy(mask, b, sizeof(b));
        }
        else
        {
            return 1;
        }

        print_remote_output_mask();
        return 0;
    }

    uint8_t lamp_test_custom(int argc, char** argv)
    {
        static uint8_t pattern;
        uint16_t buf;

        if (argc < 2) return 1;
        if (sscanf(argv[1], "%hx", &buf) != 1) return 2;
        pattern = static_cast<uint8_t>(buf & 0xFF);
        printf("\tInitialing lamp test interop with pattern = 0x%hX\n", pattern);
        HAL_StatusTypeDef ret = interop::enqueue(interop::cmds::lamp_test_custom, &pattern);
        if (ret == HAL_OK) return 0;
        return ret + 2;
    }
    uint8_t lamp_test_predef(int argc, char** argv)
    {
        static display::test_modes m = display::test_modes::digits;

        if (argc > 1)
        {
            if (sscanf(argv[1], "%" SCNu8, reinterpret_cast<uint8_t*>(&m)) != 1) return 2;
            if (m >= display::test_modes::TST_LEN) 
            {
                m = display::test_modes::digits;
                return 2;
            }
        }
        HAL_StatusTypeDef ret = interop::enqueue(interop::cmds::lamp_test_predefined, &m);
        if (ret == HAL_OK) return 0;
        return ret + 2;
    }

    uint8_t modbus_report(int argc, char** argv)
    {
        static bool install = false;

        if (argc > 1)
        {
            install = (argv[1][0] - '0') > 0;
        }
        mb_regs::print_dbg(install);
        return 0;
    }
    uint8_t modbus_rs485_test(int argc, char** argv)
    {
        mb_regs::send_dbg_rs485();
        return 0;
    }
    uint8_t modbus_dump_instance(int argc, char** argv)
    {
        if (argc < 2) return 1;
        uint16_t i;
        if (sscanf(argv[1], "%" SCNu16, &i) != 1) return 2;
        mb_regs::dump_instance_regs(i);
        return 0;
    }
    uint8_t modbus_force_apply(int argc, char** argv)
    {
        if (argc < 2) return 1;
        uint16_t i;
        if (sscanf(argv[1], "%" SCNu16, &i) != 1) return 2;
        mb_regs::force_apply_instance_input_regs(i);
        return 0;
    }

    uint8_t dfu(int argc, char** argv)
    {
        dfu_perform_wwdg_reset();
        return 0; //Never returns
    }
} // namespace cli_commands

void init()
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    CLI_INIT(&huart1);

    CLI_ADD_CMD("info", "Get device info", &cli_commands::info);

    CLI_ADD_CMD("periph_report", "Report peripheral state", &cli_commands::peripherals_report);
    CLI_ADD_CMD("coproc_report", "Report coprocessor-controled devices' state", &cli_commands::coproc_report);
    CLI_ADD_CMD("pumps_report", "Report pump state", &cli_commands::pumps_report);
    CLI_ADD_CMD("pumps_debug", "Print pump debug info", &cli_commands::pumps_debug);
    CLI_ADD_CMD("os_report", "Report FreeRTOS stats", &cli_commands::os_report);
    CLI_ADD_CMD("eth_report", "Report ethernet module stats", &cli_commands::eth_report);

    CLI_ADD_CMD("nvs_save", "Save current non-volatile data into EEPROM", &cli_commands::nvs_save);
    CLI_ADD_CMD("nvs_load", "Load non-volatile data from EEPROM", &cli_commands::nvs_load);
    CLI_ADD_CMD("nvs_dump", "Perform EEPROM HEX-dump", &cli_commands::nvs_dump);
    CLI_ADD_CMD("nvs_reset", "Reset NVS (sets NVS partiton version to 0 [invalid], doesn't actually erase the EEPROM)",
        &cli_commands::nvs_reset);
    CLI_ADD_CMD("nvs_test", "Test EEPROM readback, performs sequential number write and read, and does nvs_save afterwards",
        &cli_commands::nvs_test);
    CLI_ADD_CMD("nvs_report", "Report NVS contents in human-readable format", &cli_commands::nvs_report);
    //CLI_ADD_CMD("nvs_calc_crc", "Calculate and report current NVS contents' CRC, doesn't modify storage.crc", &cli_commands::nvs_calc_crc);
    CLI_ADD_CMD("set_modbus_keepalive", "Set modbus keep alive timeout threshold (seconds). Expects a uint16_t.",
        &cli_commands::set_modbus_keepalive);
    CLI_ADD_CMD("set_modbus_addr", "Set modbus station address (default = 1). Expects a uint16_t.", &cli_commands::set_modbus_addr);
    CLI_ADD_CMD("set_pump_coef", "Set VolumeRate-To-RPS conversion factor (Hz/a.u.), expects ", &cli_commands::set_pump_coef);
    CLI_ADD_CMD("reload_params", "Reload pump params without a reboot", &cli_commands::reload_params);

    CLI_ADD_CMD("get_coproc_err_rate", "Print coprocessor CRC error count since boot",
        &cli_commands::get_coproc_err_rate);
    CLI_ADD_CMD("get_coproc_init", "Prints whether coprocessor init-byte has been sent",
        &cli_commands::get_coproc_init);
    CLI_ADD_CMD("coproc_reinit", "Try to resend init-byte and read coprocessor afterwards",
        &cli_commands::coproc_reinit);
    CLI_ADD_CMD("coproc_scan", "Scan I2C bus by trying to write to addresses from 0x08 to 0x7F. Prints only found devs or HAL_BUSY",
        &cli_commands::coproc_scan);

    CLI_ADD_CMD("get_thermo_err_rate", "Get MAX6675 SPI RX error count since boot", &cli_commands::get_thermo_err_rate);

    CLI_ADD_CMD("input_invert", "Get/set SR_IO input inversion register. No args = print current, "
        "1 arg = index of the input to toggle inversion bit for, "
        "N>2 args = N inv register words in hex without 0x (number of words is printed with no args given)",
        &cli_commands::input_invert);
    CLI_ADD_CMD("remote_output_mask", "Get/set SR_IO modbus remote commanded output mask. Args = see input_invert help.",
        &cli_commands::remote_output_mask);

    CLI_ADD_CMD("lamp_test_custom", "Invoke display lamp test interop. Expects 1 arg: byte pattern.",
        &cli_commands::lamp_test_custom);
    CLI_ADD_CMD("lamp_test_predef", "Invoke display lamp test interop. Argument (optional): uint8_t pattern index.",
        &cli_commands::lamp_test_predef);

    CLI_ADD_CMD("modbus_report", 
        "Report modbus error info and install/remove CDC receive callback into this console (toggle). "
        "Single arg > 0 => install, ==0 => remove. No args = just print.",
        &cli_commands::modbus_report);
    CLI_ADD_CMD("modbus_rs485_test", "Send test string over RS485 to test RTS signaling", &cli_commands::modbus_rs485_test);
    CLI_ADD_CMD("modbus_dump_instance", "Dump interface instance register file, expects a uint16 (instance index)", 
        &cli_commands::modbus_dump_instance);
    CLI_ADD_CMD("modbus_force_apply", "Force-apply input registers from selected interface instance, expects a uint16 (instance index)", 
        &cli_commands::modbus_force_apply);

    CLI_ADD_CMD("dfu", "Enter DFU mode. Hard reset is required to exit form it.", &cli_commands::dfu);
}