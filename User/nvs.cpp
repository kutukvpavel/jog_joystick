#include "nvs.h"

#include "i2c_sync.h"

namespace nvs
{
    HAL_StatusTypeDef init()
    {
        return HAL_OK;
    }

    float get_rapid_speed(axis::types t)
    {
        return 10;
    }
} // namespace nvs
