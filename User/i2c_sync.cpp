#include "i2c_sync.h"

#include "i2c.h"

#define ACQUIRE_MUTEX() if (xSemaphoreTake(mutex_handle, pdMS_TO_TICKS(timeout)) != pdTRUE) return HAL_BUSY; \
    current_invoker = xTaskGetCurrentTaskHandle()
#define RELEASE_MUTEX() xSemaphoreGive(mutex_handle); current_invoker = NULL

namespace i2c
{
    static SemaphoreHandle_t mutex_handle = NULL;
    static StaticSemaphore_t srMutexBuffer;
    static TaskHandle_t current_invoker = NULL;

    HAL_StatusTypeDef init()
    {
        if (mutex_handle) return HAL_ERROR;
        
        mutex_handle = xSemaphoreCreateMutexStatic(&srMutexBuffer);
        return mutex_handle ? HAL_OK : HAL_ERROR;
    }

    //This function blocks on a task notification (no busy wait)
    HAL_StatusTypeDef write_byte(uint16_t dev_addr, uint8_t data)
    {
        const uint32_t timeout = 100; //ms
        assert_param(mutex_handle);

        ACQUIRE_MUTEX();
        HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit_IT(&hi2c1, dev_addr, &data, 1);
        if (ret == HAL_OK || ret == HAL_BUSY) //In case we've missed a notification last time, check now
        {
            //Transfer completed interrupt will give us a notification
            ret = (ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(timeout)) > 0) ? HAL_OK : HAL_ERROR;
        }

        RELEASE_MUTEX();
        return ret;
    }
    //This function blocks on a task notification (no busy wait)
    HAL_StatusTypeDef read(uint16_t dev_addr, volatile uint8_t* buffer, uint16_t len)
    {
        const uint32_t timeout = 10; //ms
        assert_param(mutex_handle);

        ACQUIRE_MUTEX();
        HAL_StatusTypeDef ret = HAL_I2C_Master_Receive_DMA(&hi2c1, dev_addr, const_cast<uint8_t*>(buffer), len);
        if (ret == HAL_OK || ret == HAL_BUSY) //In case we've missed a notification last time, check now
        {
            //Transfer completed interrupt will give us a notification
            ret = (ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(timeout)) > 0) ? HAL_OK : HAL_ERROR;
        }

        RELEASE_MUTEX();
        return ret;
    }
    //Busy-wait
    HAL_StatusTypeDef mem_read(uint16_t dev_addr, uint8_t mem_addr, uint8_t* buffer, uint16_t len)
    {
        const uint32_t timeout = 100; //ms
        assert_param(mutex_handle);

        ACQUIRE_MUTEX();
        HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(&hi2c1, dev_addr, mem_addr, I2C_MEMADD_SIZE_8BIT, buffer, len, timeout);

        RELEASE_MUTEX();
        //DBG("I2C mem_read returns %u", ret);
        return ret;
    }
    //Busy-wait
    HAL_StatusTypeDef mem_write(uint16_t dev_addr, uint8_t mem_addr, uint8_t* buffer, uint16_t len)
    {
        const uint32_t timeout = 500; //ms
        assert_param(mutex_handle);

        ACQUIRE_MUTEX();
        HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(&hi2c1, dev_addr, mem_addr, I2C_MEMADD_SIZE_8BIT, buffer, len, timeout);

        RELEASE_MUTEX();
        //DBG("I2C mem_write returns %u", ret);
        return ret;
    }
} // namespace i2c

_BEGIN_STD_C
void i2c_event_handler()
{
    if (!i2c::current_invoker) return;

    BaseType_t woken;
    vTaskNotifyGiveFromISR(i2c::current_invoker, &woken);
    portYIELD_FROM_ISR(woken);
}
_END_STD_C