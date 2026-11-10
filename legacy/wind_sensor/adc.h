#include "main.h"
#include "sys.h"
#include "delay.h"

extern ADC_HandleTypeDef hadc1;

void MX_ADC1_Init(void);
uint16_t get_adc(u8 adc_channel);
uint16_t get_Adc_Average(u8 ch,u8 times);
