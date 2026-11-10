#include "iic.h"
#include "oled.h"
#include "codetab.h"



void WriteCmd(unsigned char I2C_Command)
{
	write_i2c(OLED_DEV_ADDR,0x00, I2C_Command);
}


void WriteDat(unsigned char I2C_Data)
{
	write_i2c(OLED_DEV_ADDR,0x40, I2C_Data);
}

//OLED初始化
void OLED_Init(void)
{
  
  
	HAL_Delay(100); 
	I2C_GPIO_Init();
	WriteCmd(0xAE); //display off
	WriteCmd(0x20);	//Set Memory Addressing Mode	
	WriteCmd(0x02);	//00,Horizontal Addressing Mode;01,Vertical Addressing Mode;10,Page Addressing Mode (RESET);11,Invalid
	WriteCmd(0xb0);	//Set Page Start Address for Page Addressing Mode,0-7
	WriteCmd(0xc8);	//Set COM Output Scan Direction
	WriteCmd(0x00); //---set low column address
	WriteCmd(0x10); //---set high column address
	WriteCmd(0x40); //--set start line address
	WriteCmd(0x81); //--set contrast control register
	WriteCmd(0xff); //???? 0x00~0xff
	WriteCmd(0xa1); //--set segment re-map 0 to 127
	WriteCmd(0xa6); //--set normal display
	WriteCmd(0xa8); //--set multiplex ratio(1 to 64)
	WriteCmd(0x3F); //
	WriteCmd(0xa4); //0xa4,Output follows RAM content;0xa5,Output ignores RAM content
	WriteCmd(0xd3); //-set display offset
	WriteCmd(0x00); //-not offset
	WriteCmd(0xd5); //--set display clock divide ratio/oscillator frequency
	WriteCmd(0xf0); //--set divide ratio
	WriteCmd(0xd9); //--set pre-charge period
	WriteCmd(0x22); //
	WriteCmd(0xda); //--set com pins hardware configuration
	WriteCmd(0x12);
	WriteCmd(0xdb); //--set vcomh
	WriteCmd(0x20); //0x20,0.77xVcc
	WriteCmd(0x8d); //--set DC-DC enable
	WriteCmd(0x14); //
	WriteCmd(0xaf); //--turn on oled panel
}


void OLED_SetPos(unsigned char x, unsigned char y)
{ 
	WriteCmd(0xb0+y);
	WriteCmd(((x&0xf0)>>4)|0x10);
	WriteCmd((x&0x0f)|0x00);
}


void OLED_Fill(unsigned char fill_Data)
{
	unsigned char m,n;
	for(m=0;m<8;m++)
	{
		WriteCmd(0xb0+m);		//page0-page1
		WriteCmd(0x00);		//low column start address
		WriteCmd(0x10);		//high column start address
		for(n=0;n<128;n++)
			{
				WriteDat(fill_Data);
			}
	}
}

//清屏命令
void OLED_CLS(void)
{
	OLED_Fill(0x00);
}

//打开OLED显示
void OLED_ON(void)
{
	WriteCmd(0X8D);  
	WriteCmd(0X14);  
	WriteCmd(0XAF); 
}

//关闭OLED
void OLED_OFF(void)
{
	WriteCmd(0X8D);  
	WriteCmd(0X10);  
	WriteCmd(0XAE);  
}


//显示字符
//TextSize为字符大小，1代表8*8字符，2代表16*16字符
void OLED_ShowStr(unsigned char x, unsigned char y, unsigned char ch[], unsigned char TextSize)
{
	unsigned char c = 0,i = 0,j = 0;
	switch(TextSize)
	{
		case 1:
		{
			while(ch[j] != '\0')
			{
				c = ch[j] - 32;
				if(x > 122)
				{
					x = 0;
					y++;
				}
				OLED_SetPos(x,y);
				for(i=0;i<6;i++)
					WriteDat(F6x8[c][i]);
				x += 6;
				j++;
			}
		}break;
		case 2:
		{
			while(ch[j] != '\0')
			{
				c = ch[j] - 32;
				if(x > 120)
				{
					x = 0;
					y++;
				}
				OLED_SetPos(x,y);
				for(i=0;i<8;i++)
					WriteDat(F8X16[c*16+i]);
				OLED_SetPos(x,y+1);
				for(i=0;i<8;i++)
					WriteDat(F8X16[c*16+i+8]);
				x += 8;
				j++;
			}
		}break;
			
	}
}

//void oled_shownum(unsigned char x,unsigned char y,unsigned char num)
//{
//	unsigned char i=0;
//	if(x>122||y>6)
//	{
//		x=0,y=0;
//	}
//	OLED_SetPos(x,y);
//	for(i=0;i<8;i++)
//	{
//		WriteDat(F8X16_num[num][i]);
//	}
//	y++;
//	OLED_SetPos(x,y);
//	for(i=0;i<8;i++)
//	{
//		WriteDat(F8X16_num[num][i+8]);
//	}
//}

void Oled_ShowCH(unsigned char x,unsigned y)
{
	unsigned char i,j=0;
	
	OLED_SetPos(x,y);
	//Param:i:字符索引
	for(i = 0;i<3;i++)
	{
		for(j=0;j<16;j++)
		{
			WriteDat(F16X16[i][j]);
		}
		OLED_SetPos(x,y+1);
		for(j = 0;j<16;j++)
		{
			WriteDat(F16X16[i][j+16]);
		}
		x = x+16;
		OLED_SetPos(x,y);
	}
}
void Oled_ShowCH_1(unsigned char x,unsigned y)
{
	unsigned char i,j=0;
	
	OLED_SetPos(x,y);
	//Param:i:字符索引
	for(i = 3;i<4;i++)
	{
		for(j=0;j<16;j++)
		{
			WriteDat(F16X16[i][j]);
		}
		OLED_SetPos(x,y+1);
		for(j = 0;j<16;j++)
		{
			WriteDat(F16X16[i][j+16]);
		}
		x = x+16;
		OLED_SetPos(x,y);
	}
}
void Oled_ShowCH_2(unsigned char x,unsigned y)
{
	unsigned char i,j=0;
	
	OLED_SetPos(x,y);
	//Param:i:字符索引
	for(i = 4;i<7;i++)
	{
		for(j=0;j<16;j++)
		{
			WriteDat(F16X16[i][j]);
		}
		OLED_SetPos(x,y+1);
		for(j = 0;j<16;j++)
		{
			WriteDat(F16X16[i][j+16]);
		}
		x = x+16;
		OLED_SetPos(x,y);
	}
}
void Oled_ShowCH_3(unsigned char x,unsigned y)
{
	unsigned char i,j=0;
	
	OLED_SetPos(x,y);
	//Param:i:字符索引
	for(i = 0;i<1;i++)
	{
		for(j=0;j<16;j++)
		{
			WriteDat(F[i][j]);
		}
		OLED_SetPos(x,y+1);
		for(j = 0;j<16;j++)
		{
			WriteDat(F[i][j+16]);
		}
		x = x+16;
		OLED_SetPos(x,y);
	}
}
