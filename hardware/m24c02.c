
#include "stm32f1xx_hal.h"
#include "m24c02.h"
#include "my_iic.h"
#include "main.h"
#include "string.h"
uint8_t M24C02_WriteByte(uint8_t addr,uint8_t wdata)
{
	SoftI2C_Start();
	my_iic_sendbyte( M24C02_WADDR);
	if(IIC_Wait_ACK(200)) return 1;
	my_iic_sendbyte(addr);
	if(IIC_Wait_ACK(200)) return 2;
	my_iic_sendbyte(wdata);
	if(IIC_Wait_ACK(200)) return 3;
	SoftI2C_Stop();
	HAL_Delay(5);
	return 0;
}

uint8_t M24C02_WritePage(uint8_t addr,uint8_t *wdata)
{
	SoftI2C_Start();
	my_iic_sendbyte( M24C02_WADDR);
	if(IIC_Wait_ACK(200)) return 1;
	my_iic_sendbyte(addr);
	if(IIC_Wait_ACK(200)) return 2;
	for(uint8_t i=0;i<8;i++)
	{
		my_iic_sendbyte(wdata[i]);
		if(IIC_Wait_ACK(200)) return 3+i;
	}
	SoftI2C_Stop();
	HAL_Delay(5);
	return 0;
}
	
uint8_t M24C02_ReadData(uint8_t addr,uint8_t *rdata,uint16_t length)
{
	SoftI2C_Start();
	my_iic_sendbyte( M24C02_WADDR);
	if(IIC_Wait_ACK(200)) return 1;  //!=0*****
	my_iic_sendbyte(addr);
	if(IIC_Wait_ACK(200)) return 2;
	SoftI2C_Start();
	my_iic_sendbyte( M24C02_RADDR);
	if(IIC_Wait_ACK(200)) return 3;
	for(uint8_t i=0;i<length-1;i++)
	{
		rdata[i]=my_iic_readbyte(1);
	}
	rdata[length-1]=my_iic_readbyte(0);
SoftI2C_Stop();
	HAL_Delay(5);
	return 0;
}

void M24C02_ReadOTAInfo(void)
{
	memset(&OTA_Info,0,OTA_InfoCB_Size);
	M24C02_ReadData(0,(uint8_t*)&OTA_Info,OTA_InfoCB_Size);//将从M24C02中存放OTA标志位的信息读取出来放入RAM中的OTA_Info中
}



void M24C02_WriteOTAInfo(void)
{
	uint8_t i;
	uint8_t *wptr;
	wptr=(uint8_t* )&OTA_Info;
	for(i=0;i<OTA_InfoCB_Size/8;i++)
	{
		M24C02_WritePage(i*8,wptr+i*8);
		HAL_Delay(5);
	}
	
}

























