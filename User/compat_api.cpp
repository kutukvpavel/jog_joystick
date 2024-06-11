#include "compat_api.h"

#include "../Core/Inc/tim.h"

void delay_us(uint16_t us)
{
    __HAL_TIM_SET_COUNTER(&htim2, 0);
	while (__HAL_TIM_GET_COUNTER(&htim2) < us);
}