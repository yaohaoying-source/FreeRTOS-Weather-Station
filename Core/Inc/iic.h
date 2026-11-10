#ifndef __BSP_I2C_H__
#define	__BSP_I2C_H__

#include "stm32f1xx_hal.h"
#include "main.h"

#define I2C_GPIO_PORT                       GPIOB
#define I2C_SCL_PIN                         GPIO_PIN_8
#define I2C_SDA_PIN                         GPIO_PIN_9
#define RCC_IIC_ENABLE    __HAL_RCC_GPIOB_CLK_ENABLE()       /* GPIO�˿�ʱ�� */

#define I2C_SCL_HIGH()                      HAL_GPIO_WritePin(I2C_GPIO_PORT,I2C_SCL_PIN,GPIO_PIN_SET)    // IICʱ�������ߵ�ƽ
#define I2C_SCL_LOW()                       HAL_GPIO_WritePin(I2C_GPIO_PORT,I2C_SCL_PIN,GPIO_PIN_RESET)  // IICʱ�������͵�ƽ
#define I2C_SDA_HIGH()                      HAL_GPIO_WritePin(I2C_GPIO_PORT,I2C_SDA_PIN,GPIO_PIN_SET)    // IIC���������ߵ�ƽ
#define I2C_SDA_LOW()                       HAL_GPIO_WritePin(I2C_GPIO_PORT,I2C_SDA_PIN,GPIO_PIN_RESET)  // IIC���������͵�ƽ
#define I2C_SDA_READ()                      HAL_GPIO_ReadPin(I2C_GPIO_PORT,I2C_SDA_PIN)										//��ȡ������״̬

void 	I2C_GPIO_Init(void);
void    I2C_Start(void);
void    I2C_Stop(void);
void    I2C_SendByte(uint8_t _ucByte);
uint8_t I2C_ReadByte(void);
uint8_t I2C_WaitAck(void);
void    I2C_Ack(void);
void    I2C_NAck(void);
uint8_t IIC_Read_Byte(unsigned char ack);
//max30102
uint8_t I2C_Read_Array(uint8_t device,uint16_t addr,uint8_t *Data,uint16_t Num);

uint8_t read_i2c(uint8_t device,uint8_t addr);
void write_i2c(uint8_t device,uint8_t addr,uint8_t dat);
#endif /* __BSP_I2C_H__ */

