#ifndef __FMC_H
#define __FMC_H


#include "stm32f1xx_hal.h"
void STM32_EraseFlash(uint16_t start, uint16_t num);
void STM32_WriteFlash(uint32_t saddr, uint32_t *wdata, uint32_t wnum);



#endif

