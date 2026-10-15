#include "my_uart.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"
#include <stdarg.h>
#include <stdio.h>      /* vsprintf 的声明在这里；缺了它会有
                         * "function vsprintf declared implicitly" 警告 */
#include <string.h>
 uint8_t U0_RxBuff[U0_RX_SIZE];    //2048
UCB_CB U0CB;
char U0_TxBuff[256];	//用于print函数
void U0Rx_PtrInit(void)
	{
	  U0CB.IN=&U0CB.URxDataPtr[0];
		U0CB.OUT=&U0CB.URxDataPtr[0];
		U0CB.END=&U0CB.URxDataPtr[NUM-1];
		U0CB.IN->star=U0_RxBuff;
		U0CB.IN->end=U0_RxBuff;
		U0CB.URxcounter=0;
		
}


void u0_printf(char* format,...)
{
	uint8_t i=0;
	va_list listdata;
	va_start(listdata,format);
	
	 // 把格式化字符串拼到Tx缓存
	vsprintf((char*)U0_TxBuff,format,listdata);
	
	for(i=0;i<strlen((const char*)U0_TxBuff);i++)
	{
		while(__HAL_UART_GET_FLAG(&huart1,UART_FLAG_TXE)==RESET);  // 等待发送数据寄存器为空 TXE，RESET是不为空，里面还有数据
		huart1.Instance->DR=U0_TxBuff[i];//写入数据到DR寄存器，启动发送
		
	}
while(__HAL_UART_GET_FLAG(&huart1,UART_FLAG_TC)==RESET);
__HAL_UART_CLEAR_FLAG(&huart1,UART_FLAG_TC);  //手动清除TC！！

	
}









