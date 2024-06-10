#include "wdt.h"

#include "../Core/Inc/iwdg.h"
#include "../Core/Inc/gpio.h"
#include "task_handles.h"

#define MY_MAX_WDG_TASKS 16u
#define MY_WDT_INTERVAL 100u //ms

namespace wdt
{
   task_t tasks[MY_MAX_WDG_TASKS];
   size_t registered_tasks = 0;

   task_t* register_task(uint32_t interval_ms, const char* name)
   {
      assert_param(registered_tasks < array_size(tasks));
      assert_param(interval_ms > (3 * MY_WDT_INTERVAL));

      task_t* instance = &(tasks[registered_tasks++]);
      instance->deadline = pdMS_TO_TICKS(interval_ms);
      instance->last_time = xTaskGetTickCount();
      instance->name = name;

      DBG("Registered task WDT: %s = #%u @ %lu", name, registered_tasks, instance->last_time);
      return instance;
   }

   void process(TickType_t now)
   {
      if (registered_tasks == 0) return;
      //While no tasks have yet been registered we consider WDT feeding to be a responsiblity of the single present thread
      for(size_t i = 0; i < registered_tasks; i++){
         TickType_t diff = now - tasks[ i ].last_time;
         if( diff > tasks[i].deadline )
         {
            //Let hardware WDT reset us
            auto& t = tasks[i];
            if (t.name) ERR("Task %s WDT, last = %lu, now = %lu!", t.name, t.last_time, now);
            vTaskDelay(pdMS_TO_TICKS(100));
            vTaskSuspendAll();
            taskDISABLE_INTERRUPTS();
            while( 1 );
         }
      }
      HAL_IWDG_Refresh(&hiwdg);
   }
} // namespace name

_BEGIN_STD_C
STATIC_TASK_BODY(MY_WDT)
{
   static TickType_t last_wake;

   INIT_NOTIFY(MY_WDT);

   last_wake = xTaskGetTickCount();
   for (;;)
   {
      wdt::process(last_wake);
      vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(MY_WDT_INTERVAL));
      last_wake = xTaskGetTickCount();
   }
}
_END_STD_C