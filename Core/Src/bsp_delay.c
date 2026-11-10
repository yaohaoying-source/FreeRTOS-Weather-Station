#include "bsp_delay.h"
#include "tim.h"

HAL_StatusTypeDef BSP_Delay_Init(void)
{
    return HAL_TIM_Base_Start(&htim2);
}

void BSP_Delay_us(uint16_t us)
{
    uint16_t previous;
    uint16_t current;
    uint32_t elapsed = 0;

    previous = (uint16_t)__HAL_TIM_GET_COUNTER(&htim2);

    while(elapsed < us)
    {
        current = (uint16_t)__HAL_TIM_GET_COUNTER(&htim2);
        /* 无符号差值处理计数器从 65535 回到 0 的情况。 */
        elapsed += (uint16_t)(current - previous);
        previous = current;
    }
}
