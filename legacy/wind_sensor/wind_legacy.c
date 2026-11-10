/*
 * Historical excerpt from the bare-metal project's Core/Src/main.c.
 * Reference only: not included in the FreeRTOS Keil project.
 */
#include "adc.h"

volatile float wind=0;
volatile float wind_thr=5;

float Get_Wind_Data(void)
{
	float wind_adc=0;
	wind_adc=3.3f * get_Adc_Average(3,10)/4096.f *2;
	wind = 6*wind_adc;
	if(wind<2.28)
		wind=0;
	return wind;
}
