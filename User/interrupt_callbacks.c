#include "user.h"

#include "ushell/inc/sys_command_line.h"
#include "task_handles.h"
#include "i2c_sync.h"
#include "cmd_streamer.h"

static void (*uart_dbg_rx_callback)(UART_HandleTypeDef*) = NULL;
static void (*uart_dbg_tx_callback)(UART_HandleTypeDef*) = NULL;

void uart_install_callbacks(void (*rx)(UART_HandleTypeDef*), void (*tx)(UART_HandleTypeDef*))
{
	uart_dbg_rx_callback = rx;
	uart_dbg_tx_callback = tx;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	cli_uart_txcplt_callback(huart);
	if (uart_dbg_tx_callback != NULL) uart_dbg_tx_callback(huart);
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	cli_uart_rxcplt_callback(huart);
	cmd_queue_uart_rxcplt_callback(huart);
	if (uart_dbg_rx_callback != NULL) uart_dbg_rx_callback(huart);
}

//Fires after a DMA transfer of a result in DMA mode (DMA interrupt calls this callback handler)
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance != ADC1) return;

	assert_param(STATIC_TASK_HANDLE(MY_ADC));

	BaseType_t woken;
    vTaskNotifyGiveFromISR(STATIC_TASK_HANDLE(MY_ADC), &woken);
	portYIELD_FROM_ISR(woken);
}

//Used for DMA-accelerated i2c work
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
	if (hi2c != &hi2c1) return;

	i2c_event_handler();
}
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
	if (hi2c != &hi2c1) return;

	i2c_event_handler();
}