#pragma once

#include <inttypes.h>

#include "../Core/Inc/tim.h"

HAL_StatusTypeDef compat_api_init();
void delay_us(uint16_t us);
