#include "axis.h"

#include "a_io.h"
#include "nvs.h"

namespace axis
{
    struct debouncer
    {
        uint32_t enable;
        uint32_t direction;
        uint32_t trigger_n;
        uint32_t trigger_p;
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
        GPIO_TypeDef* port_trigger_n;
        uint32_t pin_trigger_n;
        GPIO_TypeDef* port_trigger_p;
        uint32_t pin_trigger_p;
    };

    const uint32_t debouncer_limit = 5;
    static instance axes[static_cast<size_t>(types::LEN)] = {
        {
            .port_n = IN_X_N_GPIO_Port,
            .pin_n = IN_X_N_Pin,
            .port_p = IN_X_P_GPIO_Port,
            .pin_p = IN_X_P_Pin,
            .speed_input = a_io::in::x_speed,
            .port_trigger_n = IN_A_N_GPIO_Port,
            .pin_trigger_n = IN_A_N_Pin,
            .port_trigger_p = IN_A_P_GPIO_Port,
            .pin_trigger_p = IN_A_P_Pin,
        }
    };
    static float mapping_k = 1;

    void init()
    {
        mapping_k = 1.0f / (1.0f - nvs::get_low_pot_threshold());
    }

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
        float multiplier = mapping_k * (a_io::get_input(i->speed_input) - nvs::get_low_pot_threshold());
        if (multiplier < 0) multiplier = 0;

        res.jog_enabled = (n != p);
        res.direction = p && !n;
        res.speed = multiplier * (nvs::get_max_speed(t) - nvs::get_min_speed(t)) + nvs::get_min_speed(t);
        res.trigger_auto_neg = LL_GPIO_IsInputPinSet(i->port_trigger_n, i->pin_trigger_n);
        res.trigger_auto_pos = LL_GPIO_IsInputPinSet(i->port_trigger_p, i->pin_trigger_p);

        debounce(&(res.jog_enabled), &(i->last_state.jog_enabled), &(i->d.enable));
        debounce(&(res.direction), &(i->last_state.direction), &(i->d.direction));
        debounce(&(res.trigger_auto_neg), &(i->last_state.trigger_auto_neg), &(i->d.trigger_n));
        debounce(&(res.trigger_auto_pos), &(i->last_state.trigger_auto_pos), &(i->d.trigger_p));

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
