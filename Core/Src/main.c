/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "my_uart.h"
#include "my_iic.h"
#include "m24c02.h"
#include "delay.h"
#include "w25q64.h"
#include "fmc.h"
#include "boot.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
 OTA_InfoCB OTA_Info;
 UP_DataA updataA;
/* 下面这几个是没用到的全局量，已删除：
 *   extern uint8_t U0CB_U0_RxBuff[U0_RX_SIZE];  名字和 my_uart.c 里的定义不一致
 *   uint8_t rbuff[256];  uint8_t ret;  uint32_t wdata[1024];（白占 4KB RAM）*/
uint32_t BootStaFlag;
uint8_t aaa[5]={1,2,3,4,5};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
	SoftI2C_GPIO_Init();
	U0Rx_PtrInit();
HAL_UART_Receive_DMA(&huart1,U0CB.IN->star,U0_RX_MAX+1);//重开DMA接收数据
__HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
W25Q64_Init();
//uint16_t i=0;

OTA_Info.Firelen[0]=0x55;

OTA_Info.OTA_FLAG = 0x00000000;



  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
/* 注意：bootloader 必须整个待在 0x08000000~0x08004FFF 内（A 区从 0x08005000 起）。
 * 这条红线由链接器守住：分散文件把 LR_IROM1 限制成 20KB(0x5000)，代码放不下时
 * 会直接报 L6406E / L6407E 拒绝生成 axf。所以"能编译通过"就等于"没有越界"，
 * 千万不要去放大这个区域限制，正确做法是精简代码。                  */
u0_printf("A 区向量表[0]（应为 MSP）= %08X\r\n", *(uint32_t *)0x08005000);
u0_printf("A 区向量表[1]（应为 PC ）= %08X\r\n", *(uint32_t *)0x08005004);
 bootloder_Brance();


 while(1) {
	 HAL_Delay(10);
if (U0CB.OUT != U0CB.IN)
{
    //取出一帧数据，传给事件处理函数
   bootloder_Event(U0CB.OUT->star,  U0CB.OUT->end - U0CB.OUT->star + 1);

    U0CB.OUT++;
    if(U0CB.OUT == U0CB.END)
    {
        U0CB.OUT = &U0CB.URxDataPtr[0];
    }
}

if((BootStaFlag & IAP_XMODEMC_FLAG) && (updataA.xmodem_NB == 0U))
{
	/* 1) 用真实的 100ms 计时（原来是数 100 次主循环，而循环里有 HAL_Delay(10)，
	 *    实际约 1 秒），用 HAL_GetTick() 更准；
	 * 2) 只在还没收到任何数据包时才发 C：标准 XMODEM 里 C 仅用于等待主机
	 *    开始发送，传输途中再插 C 会被部分发送端当成异常而触发重传，
	 *    重传又可能造成数据错位。                                  */
	if((HAL_GetTick() - updataA.xmodem_timer) >= 100U)
	{
		updataA.xmodem_timer = HAL_GetTick();
		u0_printf("C");
	}
}
	 
if(BootStaFlag & UPDATA_A_FLAG)
{
	/* ================= 上电自动升级（功能 5 的"无人工"版本）=================
	 * 触发条件：EEPROM 里的 OTA_Info.OTA_FLAG == OTA_SET_FLAG，
	 *           也就是"W25Q64 里已经存好一份固件，等着搬进 A 区"。
	 * 时序：bootloder_Brance() 先给 2 秒机会按 'w' 进命令行；
	 *       没按才置 UPDATA_A_FLAG，主循环看到标志就执行搬运。
	 * 搬运过程中如果向量表校验不过，会原样保留 A 区并返回 1，此时回到菜单，
	 * 从而避免"升级失败连旧程序也跑不起来"。                                  */
	BootStaFlag &= ~UPDATA_A_FLAG;          /* 先清标志，避免失败后被反复触发 */

	if(BootLoader_UpdateAFromW25Q64() != 0U)
	{
		BootLoader_Info();                  /* 失败原因上面已经打印，这里回菜单 */
	}
}

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
	
	
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
