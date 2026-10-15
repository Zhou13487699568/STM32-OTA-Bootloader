#include "delay.h"
#include "stm32f1xx_hal.h"
//SysTick微秒延时，72M，不改动HAL库原有的HAL_Delay
void delay_us(uint32_t nus)
{
	uint32_t ticks;
	uint32_t start,now;
	ticks = nus * 72UL;    //72M：1us=72个时钟
	start = SysTick->VAL;
	while(1)
	{
		now = SysTick->VAL;
		if(start >= now)
		{
			if((start - now) >= ticks) break;
		}
		else
		{
			if(( (SysTick->LOAD - now) + start ) >= ticks) break;
		}
	}
}


void delay_ms(uint32_t ms)
{
while(ms--)
	delay_us(1000);
}
