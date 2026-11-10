#ifndef GP2Y1010_H
#define GP2Y1010_H

#include "stm32f1xx_hal.h"
/* 一次完整读取的结果 */
typedef struct
{
    uint16_t raw;          /* ADC平均原始值 */
    uint16_t voltage_mv;   /* 电压，单位mV */
    float dust_ug;        /* 旧公式估算值，单位ug/m3，范围0~500 */
} GP2Y1010_Data_t;

void GP2Y1010_Init(void);

/*
 * 仅在调度器启动后的任务中调用。
 * 一批采样约1.16秒，期间ADC1由调用任务独占。
 * 返回HAL_OK时，data中的结果才有效。
 */
HAL_StatusTypeDef GP2Y1010_Read(GP2Y1010_Data_t *data);

#endif
