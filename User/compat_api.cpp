#include "compat_api.h"

HAL_StatusTypeDef compat_api_init()
{
    return HAL_TIM_Base_Start(&htim2); //Start microseconds timer
}

void delay_us(uint16_t us)
{
    __HAL_TIM_SET_COUNTER(&htim2, 0);
	while (__HAL_TIM_GET_COUNTER(&htim2) < us);
}