#include "axis.h"

#include "a_io.h"

namespace axis
{
    struct debouncer
    {
        uint32_t enable;
        uint32_t direction;
    };
    struct instance
    {
        GPIO_TypeDef* port_n;
        uint32_t pin_n;
        GPIO_TypeDef* port_p;
        uint32_t pin_p;
        a_io::in speed_input;
        state last_state;
        debouncer d;
    };

    const uint32_t debouncer_limit = 5;
    static instance axes[static_cast<size_t>(types::LEN)] = {
        {
            .port_n = IN_X_N_GPIO_Port,
            .pin_n = IN_X_N_Pin,
            .port_p = IN_X_P_GPIO_Port,
            .pin_p = IN_X_P_Pin,
            .speed_input = a_io::in::x_speed
        },
        {
            .port_n = IN_Y_N_GPIO_Port,
            .pin_n = IN_Y_N_Pin,
            .port_p = IN_Y_P_GPIO_Port,
            .pin_p = IN_Y_P_Pin,
            .speed_input = a_io::in::y_speed
        },
        {
            .port_n = IN_Z_N_GPIO_Port,
            .pin_n = IN_Z_N_Pin,
            .port_p = IN_Z_P_GPIO_Port,
            .pin_p = IN_Z_P_Pin,
            .speed_input = a_io::in::z_speed
        },
        {
            .port_n = IN_A_N_GPIO_Port,
            .pin_n = IN_A_N_Pin,
            .port_p = IN_A_P_GPIO_Port,
            .pin_p = IN_A_P_Pin,
            .speed_input = a_io::in::a_speed
        }
    };

    void debounce(bool* now, bool* last, uint32_t* debouncer)
    {
        if (*now != *last) 
        {
            if (*debouncer > debouncer_limit)
            {
                *last = *now;
                *debouncer = 0;
            }
            else
            {
                (*debouncer)++;
                *now = *last;
            }
        }
        else
        {
            *debouncer = 0;
        }
    }

    state poll(types t)
    {
        auto i = &axes[static_cast<size_t>(t)];
        state res = { };

        bool n = LL_GPIO_IsInputPinSet(i->port_n, i->pin_n) > 0;
        bool p = LL_GPIO_IsInputPinSet(i->port_p, i->pin_p) > 0;

        res.enabled = (n != p);
        res.direction = p && !n;
        res.speed = a_io::get_input(i->speed_input);

        debounce(&(res.enabled), &(i->last_state.enabled), &(i->d.enable));
        debounce(&(res.direction), &(i->last_state.direction), &(i->d.direction));

        return res;
    }

    bool get_fast()
    {
        static uint32_t debounce = 0;
        static bool last = false;

        bool current = LL_GPIO_IsInputPinSet(IN_FAST_GPIO_Port, IN_FAST_Pin) > 0;
        if (last != current)
        {
            if (debounce++ > debouncer_limit)
            {
                last = current;
                debounce = 0;
            }
        }
        else
        {
            debounce = 0;
        }

        return last;
    }
} // namespace axis
