#ifndef __M24C02_H__
#define __M24C02_H__


#include "stm32f1xx_hal.h"

//读信息：0xA0，写信息：0xA1
#define M24C02_WADDR   0xA0
#define M24C02_RADDR   0xA1

void M24C02_ReadOTAInfo(void);
uint8_t M24C02_WriteByte(uint8_t addr,uint8_t wdata);
uint8_t M24C02_WritePage(uint8_t addr,uint8_t *wdata);
uint8_t M24C02_ReadData(uint8_t addr,uint8_t *rdata,uint16_t length);
void M24C02_WriteOTAInfo(void);

#endif
