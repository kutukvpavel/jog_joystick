/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

#include "stm32f1xx_ll_system.h"
#include "stm32f1xx_ll_gpio.h"
#include "stm32f1xx_ll_exti.h"
#include "stm32f1xx_ll_bus.h"
#include "stm32f1xx_ll_cortex.h"
#include "stm32f1xx_ll_rcc.h"
#include "stm32f1xx_ll_utils.h"
#include "stm32f1xx_ll_pwr.h"
#include "stm32f1xx_ll_dma.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define OUT_LED_Pin LL_GPIO_PIN_13
#define OUT_LED_GPIO_Port GPIOC
#define IN_A_P_Pin LL_GPIO_PIN_12
#define IN_A_P_GPIO_Port GPIOB
#define IN_A_N_Pin LL_GPIO_PIN_13
#define IN_A_N_GPIO_Port GPIOB
#define IN_Z_P_Pin LL_GPIO_PIN_14
#define IN_Z_P_GPIO_Port GPIOB
#define IN_RES_Pin LL_GPIO_PIN_15
#define IN_RES_GPIO_Port GPIOB
#define IN_SPIN_N_Pin LL_GPIO_PIN_8
#define IN_SPIN_N_GPIO_Port GPIOA
#define IN_SPIN_P_Pin LL_GPIO_PIN_9
#define IN_SPIN_P_GPIO_Port GPIOA
#define IN_Z_N_Pin LL_GPIO_PIN_10
#define IN_Z_N_GPIO_Port GPIOA
#define IN_Y_P_Pin LL_GPIO_PIN_15
#define IN_Y_P_GPIO_Port GPIOA
#define IN_Y_N_Pin LL_GPIO_PIN_3
#define IN_Y_N_GPIO_Port GPIOB
#define IN_X_P_Pin LL_GPIO_PIN_4
#define IN_X_P_GPIO_Port GPIOB
#define IN_X_N_Pin LL_GPIO_PIN_5
#define IN_X_N_GPIO_Port GPIOB
#define IN_FAST_Pin LL_GPIO_PIN_8
#define IN_FAST_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
