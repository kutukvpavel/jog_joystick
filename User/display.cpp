#include "display.h"

#include "task_handles.h"
#include "wdt.h"

namespace display
{
    void init()
    {

    }
} // namespace display

_BEGIN_STD_C
STATIC_TASK_BODY(MY_DISP)
{
    static wdt::task_t* pwdt;

    display::init();
    pwdt = wdt::register_task(500, "display");
    INIT_NOTIFY(MY_DISP);

    for (;;)
    {
        
        pwdt->last_time = xTaskGetTickCount();
    }
}
_END_STD_C