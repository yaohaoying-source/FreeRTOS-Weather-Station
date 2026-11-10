#include "bsp_buzzer.h"
#include "stm32f1xx_hal.h"

/* PA4 输出高电平，开启蜂鸣器 */
void BSP_Buzzer_On(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
}

/* PA4 输出低电平，关闭蜂鸣器 */
void BSP_Buzzer_Off(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
}
