#ifndef __I2C_OLED_H__
#define	__I2C_OLED_H__

#include "stm32f1xx_hal.h"

#define OLED_DEV_ADDR			   0x78


void OLED_Init(void);
void OLED_SetPos(unsigned char x, unsigned char y);
void OLED_Fill(unsigned char fill_Data);
void OLED_CLS(void);
void OLED_ON(void);
void OLED_OFF(void);
void OLED_ShowStr(unsigned char x, unsigned char y, unsigned char ch[], unsigned char TextSize);
void Oled_ShowCH(unsigned char x,unsigned y);
void Oled_ShowCH_1(unsigned char x,unsigned y);
void Oled_ShowCH_2(unsigned char x,unsigned y);
void Oled_ShowCH_3(unsigned char x,unsigned y);
#endif /* __I2C_OLED_H__ */
