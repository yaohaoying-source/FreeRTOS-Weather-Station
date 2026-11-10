
#include "iic.h"
#include "bsp_delay.h"

void I2C_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_IIC_ENABLE;	/* ��GPIOʱ�� */

    GPIO_InitStructure.Pin = I2C_SCL_PIN | I2C_SDA_PIN;
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_OD;  	/* ��©��� */
	GPIO_InitStructure.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(I2C_GPIO_PORT, &GPIO_InitStructure);

    /* ��һ��ֹͣ�ź�, ��λIIC�����ϵ������豸������ģʽ */
//    I2C_Stop();
		I2C_SCL_HIGH();
		I2C_SDA_HIGH();
}


//IIC��ʱ����
static void I2C_Delay(void)
{
	BSP_Delay_us(3);
}


void I2C_Start(void)
{

	I2C_SDA_HIGH();
	I2C_SCL_HIGH();
	I2C_Delay();
	I2C_SDA_LOW();
	I2C_Delay();
	I2C_SCL_LOW();
	I2C_Delay();
}


void I2C_Stop(void)
{

	I2C_SDA_LOW();
	I2C_SCL_HIGH();
	I2C_Delay();
	I2C_SDA_HIGH();
}


void I2C_SendByte(uint8_t Byte)
{
	uint8_t i;

	for (i = 0; i < 8; i++)
	{		
		if (Byte & 0x80)
		{
			I2C_SDA_HIGH();
		}
		else
		{
			I2C_SDA_LOW();
		}
		I2C_Delay();
		I2C_SCL_HIGH();
		I2C_Delay();	
		I2C_SCL_LOW();
		if (i == 7)
		{
			I2C_SDA_HIGH(); 
		}
		Byte <<= 1;	
		I2C_Delay();
	}
}



uint8_t I2C_ReadByte(void)
{
	uint8_t i;
	uint8_t value;
	value = 0;
	for (i = 0; i < 8; i++)
	{
		value <<= 1;
		I2C_SCL_HIGH();
		I2C_Delay();
		if (I2C_SDA_READ())
		{
			value++;
		}
		I2C_SCL_LOW();
		I2C_Delay();
	}
	return value;
}


uint8_t I2C_WaitAck(void)
{
	uint8_t re;

	I2C_SDA_HIGH();	
	I2C_Delay();
	I2C_SCL_HIGH();	
	I2C_Delay();
	if (I2C_SDA_READ())	
	{
		re = 1;
	}
	else
	{
		re = 0;
	}
	I2C_SCL_LOW();
	I2C_Delay();
	return re;
}


void I2C_Ack(void)
{
	I2C_SDA_LOW();	
	I2C_Delay();
	I2C_SCL_HIGH();	
	I2C_Delay();
	I2C_SCL_LOW();
	I2C_Delay();
	I2C_SDA_HIGH();	
}


void I2C_NAck(void)
{
	I2C_SDA_HIGH();	/* CPU??SDA = 1 */
	I2C_Delay();
	I2C_SCL_HIGH();	
	I2C_Delay();
	I2C_SCL_LOW();
	I2C_Delay();	
}



void write_i2c(uint8_t device,uint8_t addr,uint8_t dat)
{
	I2C_Start();
	I2C_SendByte(device);
	I2C_WaitAck();
	I2C_SendByte(addr);
	I2C_WaitAck();
	I2C_SendByte(dat);
	I2C_WaitAck();
	I2C_Stop();
}


uint8_t read_i2c(uint8_t device,uint8_t addr)
{
	uint8_t dat;
	I2C_Start();
	I2C_SendByte(device);
	I2C_WaitAck();
	I2C_SendByte(addr);
	I2C_WaitAck();
	I2C_Start();
	I2C_SendByte(device+0x01);
	I2C_WaitAck();
	dat=I2C_ReadByte();
	I2C_NAck();
	I2C_Stop();
	return dat;
}

//?1???,ack=1?,??ACK,ack=0,??nACK   
uint8_t IIC_Read_Byte(unsigned char ack)
{
	unsigned char i,receive=0;
	// MYSDA_IN;//SDA?????
   for(i=0;i<8;i++ )
	{
        I2C_SCL_LOW();    //SCL?????,?SCL?????? SDA???
        BSP_Delay_us(10);
		  I2C_SCL_HIGH();
        receive<<=1;  //???????0,?????????????????,???????
        if(I2C_SDA_READ())receive++;   //?????1,++????1,???0,?????0; 
		BSP_Delay_us(10); 
   }					 
   if (!ack)
        I2C_NAck();//??nACK
   else
        I2C_Ack(); //??ACK   
   return receive;
}

uint8_t I2C_Read_Array(uint8_t device,uint16_t addr,uint8_t *Data,uint16_t Num)
{
	uint16_t i;
	I2C_Start();
	I2C_SendByte(device);
	I2C_WaitAck();
	I2C_SendByte(addr);
	I2C_WaitAck();
	I2C_Start();
	I2C_SendByte(device+0x01);
	I2C_WaitAck();
	for(i=0;i<Num;i++)
	{
		*Data++ = I2C_ReadByte();
		if(i==Num-1)
			I2C_NAck(); //��Ӧ��
		else
			I2C_Ack(); //Ӧ��
	}
	I2C_Stop();
	return 1;
}

