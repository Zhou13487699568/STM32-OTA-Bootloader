
//#include "stm32f1xx_hal.h"
//#include <stdio.h>
//#include "my_iic.h"
//#include "delay.h"


//void SoftI2C_GPIO_Init(void)
//{
//    GPIO_InitTypeDef GPIO_InitStruct = {0};
//    __HAL_RCC_GPIOB_CLK_ENABLE();  // 打开PB口时钟，假设SCL=PB6，SDA=PB7

//    //SCL PB6 开漏输出
//    GPIO_InitStruct.Pin = GPIO_PIN_6;
//    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
//    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
//    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

//    //SDA PB7 开漏输出
//    GPIO_InitStruct.Pin = GPIO_PIN_7;
//    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
//    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
//    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

//    // 总线空闲时，SCL和SDA拉高
//    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_6,GPIO_PIN_SET);
//    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_7,GPIO_PIN_SET);
//}


////开始（起始信号）
////起始信号：SCL高，SDA从高拉低
//void SoftI2C_Start(void)
//{
//    I2C_SDA_H();
//    I2C_SCL_H();
//    delay_us(2);
//    I2C_SDA_L();
//     delay_us(2);
//    I2C_SCL_L();
//}

////停止信号：SCL高，SDA从低拉高
//void SoftI2C_Stop(void)
//{
//   
//    I2C_SDA_L();
//    I2C_SCL_H();
//    delay_us(2);
//    I2C_SDA_H();
//    delay_us(2);
//}

////字节发送函数
//void my_iic_sendbyte(uint8_t txd)
//{
//	uint8_t i;
//	for(i=7;i>0;i++)
//	{
//		I2C_SCL_L();
//		if(txd & BIT(i))
//		{
//			I2C_SDA_H(); 
//		}
//		else {
//						I2C_SDA_L(); 
//		}
//		delay_us(2);
//		I2C_SCL_H();
//		delay_us(2);
//	}
//	I2C_SCL_L();
//	I2C_SDA_H(); 
//}

////等待从机应答函数
//uint8_t IIC_Wait_ACK(int16_t timeout)
//{
//	
//	do{
//		timeout--;
//		delay_us(2);
//	}while((timeout>=0) && (I2C_SDA_READ()));//SDA=0就是有应答
//	if(timeout<0) return 1;
//	I2C_SCL_H();
//	delay_us(2);
//	if(I2C_SDA_READ()==SET) return 2;
//	I2C_SCL_L();//将时钟线拉低避免，误操作数据线导致停止
//	delay_us(2);
//	return 0;
//		

//}

////读取字节函数

//uint8_t my_iic_readbyte(uint8_t ack)
//{
//	uint8_t i=0;
//	uint8_t rxd=0;
//	
//	for(i=7;i>0;i++)
//	{
//		I2C_SCL_L();
//		delay_us(2);
//	  I2C_SCL_H();
//		delay_us(2);
//		if(I2C_SDA_READ())
//		{
//			rxd |=BIT(i);
//			delay_us(2);
//		}
//		I2C_SCL_L();
//		delay_us(2);
//	}
//		if(ack)//数据线为0，应答
//		{
//		I2C_SDA_L(); 
//		I2C_SCL_H();
//			delay_us(2);
//			I2C_SCL_L();
//			I2C_SDA_H();
//			delay_us(2);
//		}
//		else{
//			I2C_SDA_H(); 	//数据线为1，非应答
//		I2C_SCL_H();
//			delay_us(2);
//			I2C_SCL_L();
//			delay_us(2);	
//	}
//return rxd;
//}



#include "stm32f1xx_hal.h"
#include "my_iic.h"
#include "delay.h"

void SoftI2C_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = I2C_SCL_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(I2C_SCL_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = I2C_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(I2C_SDA_PORT, &GPIO_InitStruct);

    I2C_SCL_H();
    I2C_SDA_H();
}

void SDA_To_Output(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = I2C_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(I2C_SDA_PORT, &GPIO_InitStruct);
}

void SDA_To_Input(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = I2C_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(I2C_SDA_PORT, &GPIO_InitStruct);
}

// 起始信号：SCL高时，SDA从高→低
void SoftI2C_Start(void)
{
    SDA_To_Output();
    I2C_SDA_H();
    I2C_SCL_H();
    delay_us(5);
    I2C_SDA_L();
    delay_us(5);
    I2C_SCL_L();
    delay_us(2);
}

// 停止信号：SCL高时，SDA从低→高
void SoftI2C_Stop(void)
{
    SDA_To_Output();
    I2C_SDA_L();
    I2C_SCL_H();
    delay_us(5);
    I2C_SDA_H();
    delay_us(5);
}

// 发送1字节：MSB优先（先发bit7）
void my_iic_sendbyte(uint8_t txd)
{
    uint8_t i;
    SDA_To_Output();
    for(i=0;i<8;i++)
    {
        I2C_SCL_L();
        delay_us(2);
        if(txd & 0x80) { I2C_SDA_H(); }
        else           { I2C_SDA_L(); }
        txd <<= 1;
        delay_us(2);
        I2C_SCL_H();
        delay_us(5);
    }
    I2C_SCL_L();
    delay_us(2);
    I2C_SDA_H();   // 释放SDA，准备接收ACK
    delay_us(2);
}

// 等待从机ACK：返回0=ACK，1=超时无ACK，2=NACK
uint8_t IIC_Wait_ACK(int16_t timeout)
{
    SDA_To_Input();          // 切输入，读从机拉低的ACK
    I2C_SCL_L();
    delay_us(3);

    while((timeout >= 0) && (I2C_SDA_READ() != GPIO_PIN_RESET))
    {
        timeout--;
        delay_us(2);
    }
    if(timeout < 0) {
        SDA_To_Output();
        return 1;
    }

    I2C_SCL_H();
    delay_us(3);
    if(I2C_SDA_READ() != GPIO_PIN_RESET) {
        I2C_SCL_L();
        SDA_To_Output();
        return 2;
    }
    I2C_SCL_L();
    delay_us(3);
    SDA_To_Output();
    return 0;
}

// 读取1字节：MSB优先接收，读完8bit后才发ACK/NACK
uint8_t my_iic_readbyte(uint8_t ack)
{
    uint8_t i;
    uint8_t rxd = 0;
    SDA_To_Input();

    for(i=0;i<8;i++)
    {
        rxd <<= 1;
        I2C_SCL_L();
        delay_us(2);
        I2C_SCL_H();
        delay_us(3);
        if(I2C_SDA_READ() != GPIO_PIN_RESET) {
            rxd |= 0x01;
        }
        I2C_SCL_L();
        delay_us(2);
    }

    // 读完8bit后，切换输出，发送ACK/NACK
    SDA_To_Output();
    if(ack) { I2C_SDA_L(); }   // ACK=0
    else    { I2C_SDA_H(); }   // NACK=1
    delay_us(2);
    I2C_SCL_H();
    delay_us(5);
    I2C_SCL_L();
    delay_us(2);
    I2C_SDA_H();
    delay_us(2);

    return rxd;
}










