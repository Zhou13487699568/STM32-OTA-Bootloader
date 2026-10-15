#ifndef __W25Q64_H
#define __W25Q64_H

#include "stm32f1xx_hal.h"

#define CS_ENABLE    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET)
#define CS_DISENABLE HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET)


uint32_t W25Q_ReadID(void);
void W25Q64_Read(uint8_t *rbuff,uint32_t addr,uint32_t datalen);
void W25Q64_PageWrite(uint8_t *wbuff,uint16_t PageNB);
void W25Q64_Erase64K(uint8_t blockNB);
void W25Q64_WaitEnable(void);
void W25Q64_WaitBusy(void);
void W25Q64_Init(void);
uint8_t SPI_ReadWriteByte(uint8_t txdata);
#endif

