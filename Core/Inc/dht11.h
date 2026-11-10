#ifndef __DHT11_H
#define __DHT11_H 
#include "main.h" 

#define DHT11_PORT GPIOB
#define DHT11_PIN GPIO_PIN_15
 
uint8_t DHT11_Init(void);//初始化DHT11
uint8_t DHT11_Read_Data(uint8_t *temp,uint8_t *humi);//读取温湿度
#endif
