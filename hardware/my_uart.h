#ifndef __MY_UART_H__
#define __MY_UART_H__

#include "stm32f1xx_hal.h"

#define U0_RX_SIZE  2048
#define U0_RX_MAX  256
#define NUM 10

typedef struct{
 uint8_t *star;
 uint8_t *end;

}UCB_URBuffptr;

typedef struct{
 uint16_t URxcounter;   /* 原来是 uint8_t：U0_RX_SIZE=2048，计数到 256 就回绕，
                         * 会导致 DMA 落点和帧长度全部错乱 */
	UCB_URBuffptr URxDataPtr[NUM];
	UCB_URBuffptr *IN;
	UCB_URBuffptr *OUT;
		UCB_URBuffptr *END;
	
}UCB_CB;

void u0_printf(char* format,...);
void U0Rx_PtrInit(void);

extern uint8_t U0_RxBuff[U0_RX_SIZE];
extern UCB_CB U0CB;

#endif


