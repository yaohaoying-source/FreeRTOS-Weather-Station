#include "adc.h"


ADC_HandleTypeDef hadc1;

void MX_ADC1_Init(void)
{
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
}

uint16_t get_adc(u8 adc_channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    uint16_t value;

    sConfig.Channel = adc_channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
        return 0;
    }
    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        return 0;
    }
    if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK)
    {
        (void)HAL_ADC_Stop(&hadc1);
        return 0;
    }

    value = (uint16_t)HAL_ADC_GetValue(&hadc1);
    if (HAL_ADC_Stop(&hadc1) != HAL_OK)
    {
        return 0;
    }
    return value;
}

uint16_t get_Adc_Average(u8 ch, u8 times)
{
    u32 temp_val = 0;
    u8 t;

    if (times == 0)
    {
        return 0;
    }
    for (t = 0; t < times; t++)
    {
        temp_val += get_adc(ch);
        delay_ms(5);
    }
    return (uint16_t)(temp_val / times);
}

void HAL_ADC_MspInit(ADC_HandleTypeDef* hadc)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(hadc->Instance==ADC1)
  {
    __HAL_RCC_ADC1_CLK_ENABLE();
		__HAL_RCC_GPIOA_CLK_ENABLE();
		GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  }
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef* hadc)
{
  if(hadc->Instance==ADC1)
  {
    __HAL_RCC_ADC1_CLK_DISABLE();

		HAL_GPIO_DeInit(GPIOA, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_3);
 
  }
}

