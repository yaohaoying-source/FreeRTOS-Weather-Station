#ifndef BSP_DELAY_H
#define BSP_DELAY_H

#include "stm32f1xx_hal.h"

HAL_StatusTypeDef BSP_Delay_Init(void);
void BSP_Delay_us(uint16_t us);

#endif
