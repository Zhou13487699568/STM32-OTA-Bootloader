#include "w25q64.h"
//spi收发数据是同时进行的，想收数据必须也要发数据
//C8T6的Flash是64k，bootloder会分掉一点所以A区程序空间会小于64K
// W25Q64,64是64M个二进制位，8M字节，8192K，128个64k块
#include "stm32f1xx_hal.h"
#include "spi.h"

void W25Q64_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* 开启GPIOA时钟 */
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* PA4 CS引脚：推挽输出，50M速率 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  CS_DISENABLE;   // CS拉高，取消选中W25Q
  // SPI初始化已经在CubeMX生成的spi.c里自动完成，不需要SPI0_Init();
}


// 全双工：发txdata，同时返回收到的字节，阻塞模式
uint8_t SPI_ReadWriteByte(uint8_t txdata)
{
  uint8_t rxdata = 0;
  HAL_SPI_TransmitReceive(&hspi1, &txdata, &rxdata, 1, HAL_MAX_DELAY);
  return rxdata;
}


void W25Q64_WaitBusy(void)
{
	uint8_t res;
	do{
	CS_ENABLE;
	SPI_ReadWriteByte(0x05);
	res=SPI_ReadWriteByte(0xff);
		 CS_DISENABLE; 
	}while((res & 0x01)==0x01);
	
}

//void W25Q64_WaitBusy(void)
//{
//    uint8_t res;

//    CS_ENABLE;

//    SPI_ReadWriteByte(0x05);

//    res=SPI_ReadWriteByte(0xff);

//    CS_DISENABLE;

//    u0_printf("status=%02X\r\n",res);

//}


void W25Q64_WaitEnable(void)
{
	W25Q64_WaitBusy();
	CS_ENABLE;
	SPI_ReadWriteByte(0x06);
	 CS_DISENABLE; 
}

void W25Q64_Erase64K(uint8_t blockNB)
{
		uint8_t wdata[4];
	wdata[0]=0xD8;
	wdata[1]=(blockNB*64*1024)>>16;
	wdata[2]=(blockNB*64*1024)>>8;
	wdata[3]=(blockNB*64*1024)>>0;
	
	W25Q64_WaitEnable();
	
		CS_ENABLE;
	HAL_SPI_Transmit(&hspi1, wdata, 4, HAL_MAX_DELAY);
	CS_DISENABLE;
	W25Q64_WaitBusy();
	
	}

//void W25Q64_Erase64K(uint8_t blockNB)
//{
//    uint8_t wdata[4];

//    uint32_t addr = blockNB * 0x10000;

//    wdata[0]=0xD8;
//    wdata[1]=(addr>>16)&0xff;
//    wdata[2]=(addr>>8)&0xff;
//    wdata[3]=addr&0xff;


//   W25Q64_WaitEnable();

//    CS_ENABLE;

//    HAL_SPI_Transmit(&hspi1,wdata,4,HAL_MAX_DELAY);

//    CS_DISENABLE;


//    u0_printf("erase cmd ok\r\n");


//    W25Q64_WaitBusy();


//    u0_printf("erase finish\r\n");
//}

void W25Q64_PageWrite(uint8_t *wbuff,uint16_t PageNB)
{
	uint8_t wdata[4];
	wdata[0]=0x02;
	wdata[1]=( PageNB*256)>>16;
	wdata[2]=( PageNB*256)>>8;
	wdata[3]=( PageNB*256)>>0;
	
	W25Q64_WaitBusy();
	W25Q64_WaitEnable();
		CS_ENABLE;
	
	HAL_SPI_Transmit(&hspi1, wdata, 4, HAL_MAX_DELAY);
	HAL_SPI_Transmit(&hspi1, wbuff, 256, HAL_MAX_DELAY);
	
		CS_DISENABLE;
	W25Q64_WaitBusy();
	
}

void W25Q64_Read(uint8_t *rbuff,uint32_t addr,uint32_t datalen)
{
	uint8_t wdata[4];
	wdata[0] = 0x03;
	wdata[1] = (addr >> 16) & 0xFF;
	wdata[2] = (addr >> 8) & 0xFF;
	wdata[3] = addr & 0xFF;

	W25Q64_WaitBusy();
	CS_ENABLE;
	//先发读命令+3字节地址
	HAL_SPI_Transmit(&hspi1, wdata,4,HAL_MAX_DELAY);
	//然后循环读取datalen字节
	for(uint32_t i=0; i<datalen; i++)
	{
		rbuff[i] = SPI_ReadWriteByte(0xFF);
	}
	CS_DISENABLE;
}






















