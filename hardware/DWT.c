
#include "dwt.h"

// DWT初始化，main里面调用一次！
void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/**
 * @brief 微秒级延时
 * @param nus：延时多少微秒
 * @note 主频72MHz，1us =72个系统时钟
 */
void delay_us(uint32_t nus)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t tick_cnt = nus * 72UL;    //72M，每微秒72个时钟
    while((DWT->CYCCNT - start) < tick_cnt);
}

