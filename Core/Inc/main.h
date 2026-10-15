/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#define STM_PAGE_NUM    64
#define STM_PAGE_SIZE  1024
#define STM_B_PAGE_NUM  20
#define STM_A_PAGE_NUM  STM_PAGE_NUM -STM_B_PAGE_NUM
#define STM_A_PAGE      STM_B_PAGE_NUM
#define STM_A_ADDR      (FLASH_BASE+STM_A_PAGE*STM_PAGE_SIZE)
#define STM_A_END_ADDR  (FLASH_BASE+STM_PAGE_NUM*STM_PAGE_SIZE-1)   /* A 区最后一个字节 0x0800FFFF */

/* SRAM 范围（STM32F103C8T6 只有 20KB）：
 * 有效地址 0x20000000~0x20004FFF，初始 MSP 允许等于 0x20005000（RAM 末尾+1） */
#define STM_SRAM_BASE      0x20000000U
#define STM_SRAM_END       0x20004FFFU
#define STM_INIT_MSP_MAX   0x20005000U

/* ===== 功能 5/6 用到的常量：外部 Flash(W25Q64) 暂存固件 =====================
 * W25Q64 = 64Mbit = 8MB，共 128 个 64KB 块。
 * 固件先整体下载到 W25Q64 的第 0 块，确认内容无误后再搬进内部 Flash 的 A 区。
 * 这样"下载中途失败/掉电"都不会破坏 A 区里原本能跑的旧程序。          */
#define STM_A_SIZE         ((STM_PAGE_NUM - STM_B_PAGE_NUM) * STM_PAGE_SIZE) /* A 区容量 = 44KB */
#define W25Q64_BLOCK_SIZE  (64U * 1024U)   /* W25Q64 一个块 64KB */
#define FW_STORE_BLOCK     0U              /* 固件暂存在第 0 块 */
#define FW_MAX_SIZE        STM_A_SIZE      /* 固件上限：必须放得进 A 区(44KB) */

#define OTA_SET_FLAG		0x03020100

#define UPDATA_A_FLAG    0x00000001
#define IAP_XMODEMC_FLAG 0x00000002
#define IAP_XMODEMD_FLAG 0x00000004
#define SET_VERSION_FLAG 0x00000008
#define       CMD_5_FLAG 0x00000010     /* 保留未用 */
#define CMD5_XMODEM_FLAG 0x00000020     /* 本次 XMODEM 的数据写外部 Flash(W25Q64)，
                                         * 而不是内部 Flash 的 A 区            */
typedef struct
{
	uint32_t OTA_FLAG;
	uint32_t Firelen[11];//w25q64中不同块的程序长度 0号成员对应第0块的大小
	uint8_t OTA_ver[32];
}OTA_InfoCB;

typedef struct
{
uint8_t updatabuff[STM_PAGE_SIZE];//一次下载1024个字节,c8t6一页是1K=1024字节，分页存储
uint32_t w25q64_BlockNB;  
	uint32_t xmodem_timer;
	uint32_t xmodem_NB;
	uint32_t xmode_CRC;
	uint32_t xmodem_lastblk;   /* 最近一个已正确存入缓冲的 XMODEM 包号，
	                            * 用来识别"ACK 丢失导致的重传包"：重传包只回 ACK
	                            * 不再写缓冲，避免整段数据错位 */

	
	}UP_DataA;//用于更新A区的结构体
#define OTA_InfoCB_Size sizeof(OTA_InfoCB)
extern OTA_InfoCB OTA_Info;
extern  UP_DataA updataA;
extern uint32_t BootStaFlag;

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define UART_TX_Pin GPIO_PIN_9
#define UART_TX_GPIO_Port GPIOA
#define UART_RX_Pin GPIO_PIN_10
#define UART_RX_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
