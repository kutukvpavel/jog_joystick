#pragma once

#include "user.h"
#include "i2c.h"

_BEGIN_STD_C
void i2c_dma_handler(void);
_END_STD_C

#ifdef __cplusplus

namespace i2c
{
    HAL_StatusTypeDef init();
    
    HAL_StatusTypeDef write_byte(uint16_t dev_addr, uint8_t data);
    HAL_StatusTypeDef read(uint16_t dev_addr, volatile uint8_t* buffer, uint16_t len);
    HAL_StatusTypeDef mem_read(uint16_t dev_addr, uint8_t mem_addr, uint8_t* buffer, uint16_t len);
    HAL_StatusTypeDef mem_write(uint16_t dev_addr, uint8_t mem_addr, uint8_t* buffer, uint16_t len);
} // namespace i2c

#endif