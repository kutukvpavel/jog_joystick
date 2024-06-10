#pragma once

#include "FreeRTOS.h"
#include "task.h"

#define MY_ADC 0
#define MY_IO 1
#define MY_DISP 2
#define MY_CLI 3
#define MY_WDT 4

#define STATIC_TASK_HANDLE(name) task_handle_##name
#define DECLARE_STATIC_TASK(name) extern TaskHandle_t task_handle_##name ; \
    extern void start_task_##name (void *argument)
#define STATIC_TASK_BODY(name) void start_task_##name (void *argument)
#define INIT_NOTIFY(name)   DBG("Task " #name " init OK!"); \
    assert_param(argument); \
    xTaskNotifyGive(*reinterpret_cast<TaskHandle_t*>(argument))

_BEGIN_STD_C

DECLARE_STATIC_TASK(MY_ADC);
DECLARE_STATIC_TASK(MY_IO);
DECLARE_STATIC_TASK(MY_DISP);
DECLARE_STATIC_TASK(MY_CLI);
DECLARE_STATIC_TASK(MY_WDT);

_END_STD_C
