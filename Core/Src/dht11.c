#include "dht11.h"
#include "bsp_delay.h"
#include "tim.h"
#include "FreeRTOS.h"
#include "task.h"      
//配置PB15为开漏输出
static void DHT11_IO_OUT(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(DHT11_PORT,&GPIO_InitStruct);
}
//配置PB15为上拉输入
static void DHT11_IO_IN(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;

    HAL_GPIO_Init(DHT11_PORT,&GPIO_InitStruct);
}
// 3. 等待引脚出现指定电平，带微秒超时
// 返回 0：成功；1：超时
static uint8_t DHT11_Wait(GPIO_PinState bit,uint16_t timeout_us)
{
    uint16_t start;
    uint16_t now;

    start = (uint16_t)__HAL_TIM_GET_COUNTER(&htim2);
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) != bit)
    {
        now = (uint16_t)__HAL_TIM_GET_COUNTER(&htim2);

        if ((uint16_t)(now - start) >= timeout_us)
        {
            return 1;
        }
    }

    return 0;
}

//4.主机发送 DHT11 起始信号
void DHT11_Rst(void)	   
{                 
	DHT11_IO_OUT(); 	//SET OUTPUT
    HAL_GPIO_WritePin(DHT11_PORT,DHT11_PIN,GPIO_PIN_RESET); 	//拉低DQ
    vTaskDelay(pdMS_TO_TICKS(20));    	//拉低至少18ms
    HAL_GPIO_WritePin(DHT11_PORT,DHT11_PIN,GPIO_PIN_SET);; 	//DQ=1 
    DHT11_IO_IN();//SET INPUT
    BSP_Delay_us(30);     	//主机拉高20~40us
}
// 5. 检查 DHT11 的响应时序
//返回1:未检测到DHT11的存在
//返回0:存在
static uint8_t DHT11_Check(void) 	   
{   
    if(DHT11_Wait(GPIO_PIN_RESET,100)) return 1;
    if(DHT11_Wait(GPIO_PIN_SET,100)) return 1;
    if(DHT11_Wait(GPIO_PIN_RESET,100)) return 1;
    return 0;
}
//6.从DHT11读取一个位,使用指针返回数据
//返回值：1/0
static uint8_t DHT11_ReadBit(uint8_t *bit) 			 
{
 	if(DHT11_Wait(GPIO_PIN_SET,100)) return 1;
    BSP_Delay_us(40);
    if(HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET)*bit = 1;
    else *bit = 0;  
    if (DHT11_Wait(GPIO_PIN_RESET, 100)) return 1;
    return 0;
}
// 7. 读取一个字节
static uint8_t DHT11_ReadByte(uint8_t *data)
{
    uint8_t value = 0;
    uint8_t bit = 0;

    for (uint8_t i = 0; i < 8; i++)
    {
        if (DHT11_ReadBit(&bit))
            return 1;

        value = (uint8_t)((value << 1) | bit);
    }

    *data = value;
    return 0;
}

//从DHT11读取一次数据
//temp:温度值(范围:0~50°)
//humi:湿度值(范围:20%~90%)
//返回值：0,正常;1,读取失败
uint8_t DHT11_Read_Data(uint8_t *temp, uint8_t *humi)
{
    uint8_t buf[5] = {0};
    if (temp == NULL || humi == NULL)
        return 1;

    DHT11_Rst();

    if (DHT11_Check())
        return 1;

    for (uint8_t i = 0; i < 5; i++)
    {
        if (DHT11_ReadByte(&buf[i]))
            return 1;
    }

    uint8_t checksum = (uint8_t)(
        buf[0] + buf[1] + buf[2] + buf[3]
    );

    if (checksum != buf[4])
        return 1;

    *humi = buf[0];
    *temp = buf[2];

    return 0;
}
//初始化DHT11的IO口 DQ 同时检测DHT11的存在
//返回1:不存在
//返回0:存在    	 
uint8_t DHT11_Init(void)
{	 
__HAL_RCC_GPIOB_CLK_ENABLE();
DHT11_IO_IN();
return 0;
} 








