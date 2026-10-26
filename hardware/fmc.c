#include "fmc.h"
#include "stm32f1xx_hal.h"
#include "main.h"
#include "usart.h"
#include "my_uart.h"

void STM32_EraseFlash(uint16_t start, uint16_t num)
{
    uint32_t page_err = 0;
    FLASH_EraseInitTypeDef erase_cfg = {0};
    HAL_StatusTypeDef ret;

    /* 安全检查：只允许擦 A 区，绝不允许擦到 bootloader 自己所在的区域。
     * bootloader 自身镜像一旦超过 (0x08005000-0x08000000)=20KB，
     * 这条擦除就会把正在运行的 bootloder_Event()/main()/u0_printf() 一起擦掉。 */
    if ((num == 0U) ||
        (start < STM_A_PAGE) ||
        (((uint32_t)start + (uint32_t)num) > STM_PAGE_NUM))
    {
        u0_printf("擦除范围非法：起始页=%d 页数=%d\r\n", start, num);
        return;
    }

    u0_printf("开始擦除：地址 0x%08X，共 %d 页\r\n",
              FLASH_BASE + (uint32_t)start * STM_PAGE_SIZE,
              num);

    /* 1. 停止 UART DMA */
    HAL_UART_DMAStop(&huart1);

    /* 2. 关闭 UART 中断 */
    __HAL_UART_DISABLE_IT(&huart1, UART_IT_IDLE);

    /* 3. 关闭 USART1 NVIC */
    HAL_NVIC_DisableIRQ(USART1_IRQn);

    /* 4. 清除可能已经产生的 IDLE 标志 */
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);

    HAL_FLASH_Unlock();

    erase_cfg.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_cfg.PageAddress =
        FLASH_BASE + (uint32_t)start * STM_PAGE_SIZE;
    erase_cfg.NbPages = num;

    ret = HAL_FLASHEx_Erase(&erase_cfg, &page_err);

    u0_printf("擦除返回=%d，出错页=0x%08X\r\n", ret, page_err);

    HAL_FLASH_Lock();

    /* 恢复 UART */
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);

    HAL_UART_Receive_DMA(
        &huart1,
        U0CB.IN->star,
        U0_RX_MAX + 1
    );

    u0_printf("串口接收已恢复\r\n");

    if(ret == HAL_OK)
        u0_printf("擦除成功\r\n");
    else
        u0_printf("擦除失败！\r\n");
}

/* saddr: 目标地址（必须 4 字节对齐且在 A 区内）
 * wdata: 数据指针
 * wnum : 要写入的【字节数】，必须是 4 的整数倍
 * 说明：原来是 while(wnum > 0){... wnum -= 4 ...}，只要传入的 wnum 不是
 *       4 的倍数，wnum 就会下溢成 0xFFFFFFFx，死循环把整个 Flash 写穿。 */
void STM32_WriteFlash(uint32_t saddr, uint32_t *wdata, uint32_t wnum)
{
    HAL_StatusTypeDef ret = HAL_OK;

    if ((wnum == 0U) || ((wnum & 0x03U) != 0U))
    {
        u0_printf("写入长度非法（必须是 4 的倍数）\r\n");
        return;
    }

    /* 地址保护：只允许写 A 区，禁止写到 bootloader 自己身上 */
    if ((saddr < STM_A_ADDR) || ((saddr + wnum - 1U) > STM_A_END_ADDR))
    {
        u0_printf("写入地址非法（超出 A 区）：0x%08X\r\n", saddr);
        return;
    }

    HAL_FLASH_Unlock();

    while(wnum > 0U)
    {
        //写一个32bit字（4字节）
        ret = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, saddr, *wdata);
        if (ret != HAL_OK)
        {
            break;                      /* 出错立即停，避免继续瞎写 */
        }
        wnum  -= 4U;
        saddr += 4U;
        wdata++;
    }

    HAL_FLASH_Lock();

    if(ret != HAL_OK)
    {
        u0_printf("写入 Flash 失败！\r\n");
    }
}


