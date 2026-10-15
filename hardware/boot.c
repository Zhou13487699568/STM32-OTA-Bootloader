#include "my_uart.h"
#include "my_iic.h"
#include "m24c02.h"
#include "delay.h"
#include "w25q64.h"
#include "fmc.h"
#include "main.h"
#include "boot.h"
#include "usart.h"
#include <string.h>

void bootloder_Brance(void)
{
	//M24C02_ReadOTAInfo();
	if(BootLoader_Enter(20)==0)
	{
	if(OTA_Info.OTA_FLAG == OTA_SET_FLAG)
	{
		u0_printf("OTA adpat\r\n");
		BootStaFlag |= UPDATA_A_FLAG;
		updataA.w25q64_BlockNB=0;
	}
	else 
	{u0_printf("no need adapt,go A\r\n");	//跳转A分区
			HAL_Delay(10);
					LOAD_A(STM_A_ADDR);		}				
	
		}
	

	u0_printf("Enter bootloader command shell\r\n");
	BootLoader_Info();


	}

/* 在 DMA+IDLE 收到的帧队列里查找字符 cmd：找到返回 1（并且把它之前
 * 那些已经检查过的帧出队），否则返回 0。
 * 不能用 U0_RxBuff[0] 判断：那个字节只在"复位后收到的第一个字节恰好
 * 是 w"时才有意义，之后 DMA 的落点会一直往后走，它就永远不变了。 */
static uint8_t BootLoader_CmdPending(uint8_t cmd)
{
    while (U0CB.OUT != U0CB.IN)
    {
        uint8_t i;
        uint8_t len = (uint8_t)(U0CB.OUT->end - U0CB.OUT->star + 1U);

        for (i = 0; i < len; i++)
        {
            if (U0CB.OUT->star[i] == cmd)
            {
                return 1;
            }
        }

        U0CB.OUT++;
        if (U0CB.OUT == U0CB.END)
        {
            U0CB.OUT = &U0CB.URxDataPtr[0];
        }
    }

    return 0;
}

uint8_t BootLoader_Enter(uint8_t timeout) {
    u0_printf("Press 'w' within %d ms to enter the BootLoader command line.\r\n", timeout * 100);
    while (timeout--) {
        HAL_Delay(100);
        if(BootLoader_CmdPending('w') != 0U) {
            return 1;                    //进入命令行
        }
    }
    return 0;                           //不进入命令行
}


void BootLoader_Info(void)
{
    u0_printf("\r\n");

    u0_printf("[1] Erase A Partition\r\n");
    // 擦除 A 分区

    u0_printf("[2] UART IAP: Download to A Partition\r\n");
    // 通过串口 IAP 将程序下载到 A 分区

    u0_printf("[3] Set OTA Version\r\n");
    // 设置 OTA 版本号

    u0_printf("[4] Get OTA Version\r\n");
    // 查询 OTA 版本号

    u0_printf("[5] Download Firmware to External Flash\r\n");
    // 将固件下载到外部 Flash

    u0_printf("[6] Run Firmware from External Flash\r\n");
    // 从外部 Flash 运行固件

    u0_printf("[7] Restart\r\n");
    // 重启系统
}







//设置sp
 void MSR_sp(uint32_t ADDR)
{
// HAL库内置，CMSIS提供，直接调用，不用写汇编
__set_MSP( *(__IO uint32_t *)ADDR );

}


void LOAD_A(uint32_t addr)
{
    uint32_t app_msp;
    uint32_t app_pc;
    load_a   app_entry;

    app_msp = *(volatile uint32_t *)addr;
    app_pc  = *(volatile uint32_t *)(addr + 4);

    u0_printf("APP ADDR = 0x%08X\r\n", addr);
    u0_printf("APP MSP  = 0x%08X\r\n", app_msp);
    u0_printf("APP PC   = 0x%08X\r\n", app_pc);

    /* MSP 合法性：C8T6 只有 20KB SRAM，范围 0x20000000~0x20004FFF。
     * 初始栈顶 = RAM 末尾 + 1，所以 0x20005000 是完全合法的值，
     * 原来的 "app_msp > 0x20004FFF" 会把正确的 0x20005000 判成错误。
     * 另外初值必须 8 字节对齐（AAPCS 要求）。 */
    if ((app_msp < 0x20000100U) ||
        (app_msp > STM_INIT_MSP_MAX) ||
        ((app_msp & 0x07U) != 0U))
    {
        u0_printf("APP MSP ERROR\r\n");
        return;
    }

    /* 复位入口：必须落在 A 区范围内，并且 bit0 = 1（Thumb 状态） */
    if ((app_pc < STM_A_ADDR) ||
        (app_pc > STM_A_END_ADDR) ||
        ((app_pc & 0x01U) == 0U))
    {
        u0_printf("APP PC ERROR\r\n");
        return;
    }

    app_entry = (load_a)app_pc;

    /* 1) 先关掉 bootloader 的全部中断源并清挂起标志。
     *    原来的顺序是 SCB->VTOR=addr; __enable_irq(); __set_MSP(app_msp);
     *    在"开了中断、栈还没换"的窗口里只要来一个 SysTick/串口中断，
     *    硬件就会用 bootloader 的栈去压栈、却按 A 区的向量表取入口，
     *    这是跳转后最典型的 HardFault 来源。                       */
    __disable_irq();
    HAL_SuspendTick();                  /* 关掉 HAL 的 1ms SysTick 中断 */
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;
    NVIC->ICER[0] = 0xFFFFFFFFU;        /* F103C8 的 IRQ 都在 ICER[0] 内 */
    NVIC->ICPR[0] = 0xFFFFFFFFU;        /* 清掉所有已挂起的中断标志 */

    /* 2) 释放 bootloader 占用的外设（串口/DMA/GPIO） */
    BootLoader_Clear();

    /* 3) 向量表重定位到 A 区，并保证对后面的取指立即生效 */
    SCB->VTOR = addr;
    __DSB();
    __ISB();

    /* 4) 最后一步才切栈，切完立刻跳转，中间不允许任何中断进来 */
    __set_MSP(app_msp);
    __DSB();
    __ISB();

    app_entry();                        /* 进入 A 区 Reset_Handler，不返回 */

    while (1)
    {
    }
}




/* 校验版本号格式 VER-x.y.z-y/m/d-h:m（共 26 字节，分隔符位置固定）。
 * 用手写判断替代 sscanf：MicroLIB 的 scanf 链（scanf.o/scanf_fp.o/
 * scanf_char_file.o/_chval.o 等）会多占 1KB 以上 Flash，而 bootloader
 * 必须整个压在 0x08005000 以下（20KB），这里必须省。          */
static uint8_t OTA_Ver_IsValid(const uint8_t *s, uint8_t len)
{
    uint8_t i = 4U;
    uint8_t f, digits;
    const char sep[7] = {'.', '.', '-', '/', '/', '-', ':'};

    if (len != 26U) return 0U;
    if ((s[0] != 'V') || (s[1] != 'E') || (s[2] != 'R') || (s[3] != '-')) return 0U;

    for (f = 0U; f < 7U; f++)
    {
        digits = 0U;
        while ((i < len) && (s[i] >= '0') && (s[i] <= '9')) { i++; digits++; }
        if (digits == 0U) return 0U;                       /* 每段至少一位数字 */
        if ((i >= len) || (s[i] != (uint8_t)sep[f])) return 0U;
        i++;                                               /* 跳过分隔符 */
    }

    digits = 0U;
    while ((i < len) && (s[i] >= '0') && (s[i] <= '9')) { i++; digits++; }
    if ((digits == 0U) || (i != len)) return 0U;

    return 1U;
}

/* ============================================================================
 *             功能 5 / 6 ：外部 Flash(W25Q64) 暂存与回搬
 * ==========================================================================*/

/* ----------------------------------------------------------------------------
 * W25Q64_Write1K()：把 RAM 里的 1KB 缓冲(updataA.updatabuff)写进 W25Q64
 *   waddr : W25Q64 中的目标地址，必须是 1KB 对齐
 *   返回  : 0 = 成功，1 = 会跨出暂存块（说明固件太大）
 *
 * 为什么要自己拆成 4 页：
 *   W25Q64 的页编程指令(0x02)一次只能写 1 页=256 字节，而且起始地址必须页对齐；
 *   如果一次写超过一页或地址没对齐，多出来的数据会在"页内回卷"，把页开头覆盖掉。
 *   所以 1KB = 4 次 256 字节页写。
 * 为什么用地址算页号而不是直接用驱动的 PageNB 参数：
 *   驱动 W25Q64_PageWrite() 收的是"页号"，页号 = 地址/256；这里统一从地址换算，
 *   避免"地址"和"页号"两套编号混用算错。
 * -------------------------------------------------------------------------- */
static uint8_t W25Q64_Write1K(uint32_t waddr)
{
    uint8_t  k;
    uint16_t page;

    if ((waddr + (uint32_t)STM_PAGE_SIZE) > W25Q64_BLOCK_SIZE)
    {
        return 1U;                                 /* 会跨块：本设计只用一个块 */
    }

    page = (uint16_t)(waddr / 256U);               /* 起始页号 */

    for (k = 0U; k < (STM_PAGE_SIZE / 256U); k++)  /* 1024/256 = 4 页 */
    {
        W25Q64_PageWrite(&updataA.updatabuff[(uint32_t)k * 256U],
                         (uint16_t)(page + k));
    }

    return 0U;
}

/* ----------------------------------------------------------------------------
 * BootLoader_UpdateAFromW25Q64()：功能 6 的核心 —— 把 W25Q64 里的固件搬进 A 区
 *
 * 流程与理由：
 *   (1) 从 EEPROM 里的 OTA_Info.Firelen[] 取固件长度。它是"含 XMODEM 填充的
 *       实际写入长度"，一定是 128 的整数倍，因此天然满足内部 Flash 要求的
 *       4 字节对齐，不需要额外补齐。
 *   (2) ***先*** 把 W25Q64 的头 8 字节读出来，按向量表规则(合法的 MSP + 合法的
 *       Thumb 复位入口)校验一遍。这一步必须放在擦除 A 区之前：万一外部 Flash
 *       里是空的或坏的，直接擦 A 区就等于把原本能跑的旧固件也毁掉了。
 *   (3) 校验通过才擦除整个 A 区。
 *   (4) 1KB 一块搬运：W25Q64 读 -> RAM 缓冲 -> 内部 Flash 写 -> 立刻回读校验。
 *       回读校验能抓出"Flash 写入失败/写了一半"这类静默错误。
 *   (5) 全部通过后清掉 EEPROM 里的"待更新"标记（避免下次复位反复搬运），
 *       然后跳进 A 区。
 *
 * 返回：0 = 已成功搬完并跳转（正常情况下不会返回）
 *       1 = 失败，调用者应回到菜单（此时 A 区可能已被擦，但绝不会跳进坏程序）
 * -------------------------------------------------------------------------- */
uint8_t BootLoader_UpdateAFromW25Q64(void)
{
    uint32_t fwlen;          /* 固件长度(字节) */
    uint32_t fwbase;         /* 固件在 W25Q64 中的起始地址 */
    uint32_t i, j, n;
    uint32_t msp, pc;
    uint32_t *pram;          /* 指向 RAM 缓冲 */
    uint32_t *pflash;        /* 指向内部 Flash 里的对应位置 */

    fwbase = (uint32_t)FW_STORE_BLOCK * W25Q64_BLOCK_SIZE;
    fwlen  = OTA_Info.Firelen[FW_STORE_BLOCK];

    u0_printf("Ext len=%d\r\n", fwlen);

    /* (1) 长度合法性：非 0、不超过 A 区容量、4 字节对齐 */
    if ((fwlen == 0U) || (fwlen > FW_MAX_SIZE) || ((fwlen & 0x03U) != 0U))
    {
        u0_printf("Ext len err\r\n");
        return 1U;
    }

    /* (2) 先验向量表，再决定动不动 A 区 */
    W25Q64_Read(updataA.updatabuff, fwbase, 8U);
    msp = *(volatile uint32_t *)&updataA.updatabuff[0];
    pc  = *(volatile uint32_t *)&updataA.updatabuff[4];
    u0_printf("MSP=%08X PC=%08X\r\n", msp, pc);

    if ((msp < 0x20000100U) || (msp > STM_INIT_MSP_MAX) || ((msp & 0x07U) != 0U) ||
        (pc  < STM_A_ADDR)   || (pc  > STM_A_END_ADDR)   || ((pc  & 0x01U) == 0U))
    {
        u0_printf("Bad vector, A kept\r\n");
        return 1U;                       /* A 区原样保留，旧程序还能跑 */
    }

    /* (3) 擦除 A 区 */
    STM32_EraseFlash(STM_A_PAGE, STM_A_PAGE_NUM);

    /* (4) 1KB 一块搬 + 回读校验 */
    for (i = 0U; i < fwlen; i += (uint32_t)STM_PAGE_SIZE)
    {
        n = fwlen - i;
        if (n > (uint32_t)STM_PAGE_SIZE)
        {
            n = (uint32_t)STM_PAGE_SIZE;         /* 每次最多搬 1KB */
        }

        W25Q64_Read(updataA.updatabuff, fwbase + i, n);                      /* 外部读 */
        STM32_WriteFlash(STM_A_ADDR + i, (uint32_t *)updataA.updatabuff, n); /* 内部写 */

        pram   = (uint32_t *)updataA.updatabuff;
        pflash = (uint32_t *)(STM_A_ADDR + i);
        for (j = 0U; j < (n / 4U); j++)
        {
            if (pflash[j] != pram[j])
            {
                u0_printf("verify fail\r\n");    /* 写完立刻读回来比对 */
                return 1U;
            }
        }
    }

    /* (5) 搬运成功：清"待更新"标记并存回 EEPROM，避免下次复位又搬一遍 */
    OTA_Info.OTA_FLAG = 0U;
    M24C02_WriteOTAInfo();

    HAL_Delay(50);
    u0_printf("Run A\r\n");

    LOAD_A(STM_A_ADDR);          /* 只有 A 区向量表非法时才会返回（LOAD_A 会自己报错） */
    return 1U;
}

void bootloder_Event(uint8_t* data, uint8_t datalen)
{
    uint8_t ack;

    /*
     * Bootloader菜单
     */
    if(BootStaFlag == 0)
    {

        if(datalen >= 1 && data[0]=='1')
        {
            u0_printf("Erase A\r\n");

            STM32_EraseFlash(STM_A_PAGE, STM_A_PAGE_NUM);
        }


        else if(datalen >= 1 && data[0]=='2')
        {
            u0_printf("Start XMODEM download\r\n");

            //擦除APP区域
            STM32_EraseFlash(STM_A_PAGE, STM_A_PAGE_NUM);


            //打开XMODEM接收
            BootStaFlag  |= IAP_XMODEMC_FLAG;
            BootStaFlag |= IAP_XMODEMD_FLAG;


            updataA.xmodem_timer = 0;
            updataA.xmodem_NB = 0;
            updataA.xmodem_lastblk = 0xFFFFFFFFU;   /* 无效值：保证第 1 个包（包号
                                                     * 可能是 0 或 1）不会被误判成重传包 */
           return ;
        }
    else if(datalen >=1 && data[0]=='3')
   {
	    u0_printf("Set OTA Version\r\n");
	 BootStaFlag |= SET_VERSION_FLAG;

     }
 else if(datalen >=1 && data[0]=='4')
   {
	    u0_printf("Check OTA Version\r\n");
	    /* 原来这里多了一句 BootStaFlag |= SET_VERSION_FLAG; ，
	     * 会让"查询版本"之后收到的任意 26 字节帧被当成版本号写进 EEPROM，
	     * 已删除：只有按 3 才进入版本设置状态。 */
		  M24C02_ReadOTAInfo();
		  u0_printf(" OTA Version:%s\r\n",OTA_Info.OTA_ver);
		  BootLoader_Info();

     }
        else if(datalen >= 1 && data[0]=='5')
        {
            /* [5] 用 XMODEM 把固件下载到外部 Flash(W25Q64) 暂存。
             *     关键点：整个下载过程完全不碰内部 Flash，所以下载失败、
             *     断线、掉电都不会影响 A 区里正在跑的旧程序。            */
            u0_printf("Download to W25Q64\r\n");

            /* 1) 复位 XMODEM 相关状态 */
            updataA.xmodem_timer   = 0;
            updataA.xmodem_NB      = 0;
            updataA.xmodem_lastblk = 0xFFFFFFFFU;

            /* 2) 擦掉暂存块：W25Q64 必须"先擦后写"，块擦除(0xD8)约 150ms */
            W25Q64_Erase64K(FW_STORE_BLOCK);

            /* 3) 打开 XMODEM-CRC 接收。
             *    CMD5_XMODEM_FLAG 就是"本次数据写外部 Flash"的开关；
             *    收包、满 1KB 落盘、EOT 收尾这些流程全部与功能 2 复用。 */
            BootStaFlag |= IAP_XMODEMC_FLAG | IAP_XMODEMD_FLAG | CMD5_XMODEM_FLAG;
            return;
        }
        else if(datalen >= 1 && data[0]=='6')
        {
            /* [6] 把 W25Q64 里暂存的固件搬进 A 区并运行 */
            u0_printf("Run FW in W25Q64\r\n");

            if (BootLoader_UpdateAFromW25Q64() != 0U)
            {
                /* 失败：回到菜单；A 区里的旧程序仍然在（向量表校验会先拦住） */
                u0_printf("Update A fail\r\n");
                BootLoader_Info();
            }
            return;
        }
        else if(datalen >=1 && data[0]=='7')
        {
            u0_printf("reset\r\n");

            HAL_Delay(100);

            NVIC_SystemReset();
        }

    }



    /*
     * XMODEM接收
     */
    if(BootStaFlag & IAP_XMODEMD_FLAG)
    {
        /*
         * 接收数据包
         *
         * SOH  0x01
         * NO
         * ~NO
         * DATA 128
         * CRC16 2
         *
         * 共133字节
         */
        if(datalen == 133 && data[0]==0x01)
        {
            uint8_t  blk = data[1];      /* 包号 */
            uint16_t crc;

            if(data[2] != (uint8_t)(~blk))
            {
                /*
                 * 包号取反字节不合法：协议错乱，回 NAK 让主机重发本包
                 */
                ack=0x15;
                HAL_UART_Transmit( &huart1, &ack,1,100);
            }
            else if((uint32_t)blk == updataA.xmodem_lastblk)
            {
                /*
                 * 同一个包的重传（上一次 ACK 在链路上丢了）。
                 * 这里只回 ACK、绝不能再写一次缓冲：否则 xmodem_NB 会多加一，
                 * 后面所有数据都会整体错位一个包，下载出来的向量表就是错的。
                 */
                ack=0x06;
                HAL_UART_Transmit(&huart1, &ack,1,100);
            }
            else
            {
                crc = Xmodem_CRC16(&data[3],128);

                if(crc == ((data[131]<<8)|data[132]))
                {
                    /*
                     * 保存数据
                     */
                    memcpy(
                    &updataA.updatabuff[(updataA.xmodem_NB%(STM_PAGE_SIZE/128))*128], &data[3],128);

                    updataA.xmodem_NB++;
                    updataA.xmodem_lastblk = (uint32_t)blk;   /* 记下已接收的包号 */
                    /*
                     * 满1KB落盘：写到哪里由 CMD5_XMODEM_FLAG 决定
                     *   未置位 = 功能2：写内部 Flash 的 A 区
                     *   置位   = 功能5：写外部 Flash(W25Q64) 暂存块
                     */
                    if(updataA.xmodem_NB%(STM_PAGE_SIZE/128)==0)
                    {
                        if(BootStaFlag & CMD5_XMODEM_FLAG)
                        {
                            /* 刚收满的这 1KB 在 W25Q64 里的起始地址。
                             * xmodem_NB 在这之前已经 +1，所以减掉 8 个包就是这 1KB 的起点。 */
                            uint32_t waddr = (updataA.xmodem_NB - (STM_PAGE_SIZE/128)) * 128U;

                            /* 两种异常共用同一段收尾：
                             *   · 固件超过 A 区容量 —— 搬不进去，没有继续收的意义；
                             *   · W25Q64 写失败/会跨块。
                             * 短或运算保证越界时不会再调用写函数。 */
                            if(((waddr + STM_PAGE_SIZE) > FW_MAX_SIZE) ||
                               (W25Q64_Write1K(waddr) != 0U))
                            {
                                u0_printf("\r\nW25Q64 err\r\n");
                                BootStaFlag &= ~(IAP_XMODEMD_FLAG | IAP_XMODEMC_FLAG | CMD5_XMODEM_FLAG);
                                ack = 0x18;                 /* CAN：通知主机停止发送 */
                                HAL_UART_Transmit(&huart1, &ack, 1, 100);
                                return;
                            }
                        }
                        else
                        {
                            STM32_WriteFlash(
                            FLASH_BASE +(STM_A_PAGE+
                            ((updataA.xmodem_NB/(STM_PAGE_SIZE/128))-1))
                            *STM_PAGE_SIZE,

                            (uint32_t*)updataA.updatabuff,

                            STM_PAGE_SIZE);
                        }
                    }

                    /*
                     * ACK
                     */
                    ack=0x06;

                    HAL_UART_Transmit(&huart1, &ack,1,100);
                }
                else
                {
                    /*
                     * CRC错误
                     */
                    ack=0x15;
                    HAL_UART_Transmit( &huart1, &ack,1,100);

                }
            }
        }

        /*
         * EOT结束
         */
        else if(datalen==1 && data[0]==0x04)
        {
            /*
             * EOT：本次传输收尾
             *   remain = 最后不足 1KB 的部分（8 个包凑 1KB，所以一定是 128 的整数倍）
             *   total  = 本次一共要写入的字节数
             */
            uint16_t remain;
            uint32_t total;

            remain = (updataA.xmodem_NB%(STM_PAGE_SIZE/128))*128;
            total  = updataA.xmodem_NB*128U;

            if(BootStaFlag & CMD5_XMODEM_FLAG)
            {
                /* ============ 功能 5 收尾：数据落在外部 Flash(W25Q64) ============ */
                if(remain)
                {
                    /* W25Q64 只能整页(256B)写且要求页对齐，所以这里把"包含这
                     * remain 字节的整 1KB"写下去：
                     *   · 起点 total-remain 一定是 1KB 对齐的，拆页安全；
                     *   · 缓冲里多出来的残留字节写进去没有副作用，因为固件真正
                     *     的长度由 OTA_Info.Firelen[] 记录，回搬时不会读到它们。 */
                    if(W25Q64_Write1K(total - remain) != 0U)
                    {
                        u0_printf("\r\nW25Q64 err\r\n");
                        BootStaFlag &= ~(IAP_XMODEMD_FLAG | IAP_XMODEMC_FLAG | CMD5_XMODEM_FLAG);
                        return;
                    }
                }

                /* 记录长度 + 打上"待更新"标记，并写进 EEPROM（掉电不丢）。
                 * A 区只有 44KB，一个 64KB 块足够放，所以只用 Firelen[0]。
                 * 下一次上电时 bootloder_Brance() 会读到这个标记，自动完成搬运。 */
                OTA_Info.Firelen[FW_STORE_BLOCK] = total;
                OTA_Info.OTA_FLAG = OTA_SET_FLAG;
                M24C02_WriteOTAInfo();

                ack=0x06;
                HAL_UART_Transmit( &huart1,&ack, 1,100);

                u0_printf("\r\nOK %d B\r\n", total);
                BootLoader_Info();

                /* 回到菜单：整个下载过程没有写内部 Flash，A 区旧程序依然可用 */
                BootStaFlag &= ~(IAP_XMODEMD_FLAG | IAP_XMODEMC_FLAG | CMD5_XMODEM_FLAG);
                return;
            }

            /* ================= 功能 2 收尾：数据落在内部 Flash A 区 ================= */
            if(remain)
            {
                STM32_WriteFlash(FLASH_BASE +(STM_A_PAGE+(updataA.xmodem_NB/(STM_PAGE_SIZE/128)))*STM_PAGE_SIZE,(uint32_t*)updataA.updatabuff, remain);
            }

            /*
             * 回复ACK
             */
            ack=0x06;
            HAL_UART_Transmit( &huart1,&ack, 1,100);

            BootStaFlag &= ~IAP_XMODEMD_FLAG;
            BootStaFlag &= ~IAP_XMODEMC_FLAG;   /* 同时停掉发送 C 的轮询标志 */

            HAL_Delay(100);
            NVIC_SystemReset();

        }

    }
		
		
	else	if(BootStaFlag & SET_VERSION_FLAG)
		{
			if(datalen == 26)
			{
				if(OTA_Ver_IsValid(data, 26U)){
					memset(OTA_Info.OTA_ver,0,32);
					memcpy(OTA_Info.OTA_ver,data,26);
					 M24C02_WriteOTAInfo();
					u0_printf("Version number format right\r\n");
					BootLoader_Info();
					BootStaFlag &= ~SET_VERSION_FLAG;
				}else{u0_printf("Version number format error.\r\n");
				}
			}else{
			u0_printf("VERSION LENGTH ERROR\r\n");
			}
		}

}

void BootLoader_Clear(void)
{
    __disable_irq();

    HAL_UART_DeInit(&huart1);

    __HAL_RCC_USART1_CLK_DISABLE();

    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);

    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6 | GPIO_PIN_7);

    HAL_DeInit();
}


uint16_t Xmodem_CRC16(uint8_t *data, uint16_t datalen)
{
    uint8_t i;
    uint16_t Crcinit = 0x0000;
    uint16_t Crcipoly = 0x1021;

    while (datalen--)
    {
        Crcinit = (*data << 8) ^ Crcinit;
        for(i = 0; i < 8; i++)
        {
            if(Crcinit & 0x8000)
                Crcinit = (Crcinit << 1) ^ Crcipoly;
            else
                Crcinit = (Crcinit << 1);
        }
        data++;
    }
    return Crcinit;
}




