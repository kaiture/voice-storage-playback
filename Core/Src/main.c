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
#include "adc.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "key.h"
#include "led.h"
#include "mcp4821.h"
#include "play.h"
#include "record.h"
#include "w25q64.h"
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
static SystemState system_state = ST_IDLE;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void System_HandleKey(KeyTypeDef key);
static void System_StopRecording(uint8_t failed);
static HAL_StatusTypeDef System_StopPlayback(uint8_t failed);
static void System_StartPlayback(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* 错误码指示: LED 连闪 code 次 -> 停顿 -> 重复 (区别于 Error_Handler 的连续快闪) */
static void FatalBlink(uint32_t code)
{
  volatile uint32_t i;
  uint32_t n;

  __disable_irq();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  /* PC13 推挽输出 */
  GPIOC->CRH &= ~((uint32_t)0xFU << 24);
  GPIOC->CRH |= ((uint32_t)0x1U << 24);
  while (1)
  {
    for (n = 0U; n < code; n++)
    {
      GPIOC->BSRR = (uint32_t)GPIO_PIN_13 << 16;  /* 亮(低电平点亮) */
      for (i = 0U; i < 600000U; ++i) { }
      GPIOC->BSRR = GPIO_PIN_13;                  /* 灭 */
      for (i = 0U; i < 600000U; ++i) { }
    }
    for (i = 0U; i < 4000000U; ++i) { }           /* 组间停顿(约0.5s) */
  }
}
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
  MX_ADC1_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  LED_Init();
  KEY_Init();
  if (MCP4821_Init() != HAL_OK)
  {
    FatalBlink(1U);   /* MCP4821 初始化失败 */
  }
  if (W25Q64_Init() != HAL_OK)
  {
    if (W25Q64_DebugID == 0xFFFFFFUL)          /* MISO 无响应(悬空/芯片没工作) */
    {
      FatalBlink(2U);
    }
    else if (W25Q64_DebugID == 0x00000000UL)   /* MISO 恒低 */
    {
      FatalBlink(3U);
    }
    else                                       /* 芯片有应答, 但 ID 非 W25Q 系列 */
    {
      FatalBlink(4U);
    }
  }
  if (Record_Init() != HAL_OK)
  {
    FatalBlink(5U);   /* 录音子系统初始化失败 */
  }
  if (Play_Init() != HAL_OK)
  {
    FatalBlink(6U);   /* 播放子系统初始化失败 */
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    System_HandleKey(KEY_Scan());

    if ((system_state == ST_RECORD) || (system_state == ST_REC_PAUSE))
    {
      if ((Record_Process() != HAL_OK) || (Record_HasError() != 0U))
      {
        System_StopRecording(1U);
      }
      else if (Record_IsFull() != 0U)
      {
        System_StopRecording(0U);
      }
    }

    if ((system_state == ST_PLAY) || (system_state == ST_PLAY_PAUSE))
    {
      if (Play_Process() != HAL_OK)
      {
        System_StopPlayback(1U);
      }
      else if (Play_IsFinished() != 0U)
      {
        System_StopPlayback(0U);
      }
    }

    LED_Control(system_state);
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
static void System_StopRecording(uint8_t failed)
{
  HAL_StatusTypeDef status = Record_Stop();

  system_state = ((status != HAL_OK) || (failed != 0U)) ? ST_ERROR : ST_IDLE;
}

static HAL_StatusTypeDef System_StopPlayback(uint8_t failed)
{
  HAL_StatusTypeDef status = Play_Stop();

  system_state = ((status != HAL_OK) || (failed != 0U)) ? ST_ERROR : ST_IDLE;
  return status;
}

static void System_StartPlayback(void)
{
  if (Record_GetLength() != 0U)
  {
    system_state = (Play_Start() == HAL_OK) ? ST_PLAY : ST_ERROR;
  }
}

static void System_HandleKey(KeyTypeDef key)
{
  if (key == KEY_NONE)
  {
    return;
  }

  switch (key)
  {
    case KEY1_PRESSED:
      if ((system_state == ST_IDLE) || (system_state == ST_ERROR))
      {
        system_state = (Record_Start() == HAL_OK) ? ST_RECORD : ST_ERROR;
      }
      else if ((system_state == ST_RECORD) || (system_state == ST_REC_PAUSE))
      {
        System_StopRecording(0U);
      }
      break;

    case KEY2_PRESSED:
      if ((system_state == ST_RECORD) && (Record_Pause() == HAL_OK))
      {
        system_state = ST_REC_PAUSE;
      }
      else if ((system_state == ST_REC_PAUSE) && (Record_Resume() == HAL_OK))
      {
        system_state = ST_RECORD;
      }
      else if ((system_state == ST_PLAY) && (Play_Pause() == HAL_OK))
      {
        system_state = ST_PLAY_PAUSE;
      }
      else if ((system_state == ST_PLAY_PAUSE) && (Play_Resume() == HAL_OK))
      {
        system_state = ST_PLAY;
      }
      break;

    case KEY3_PRESSED:
      if ((system_state == ST_PLAY) || (system_state == ST_PLAY_PAUSE))
      {
        System_StopPlayback(0U);
      }
      else if ((system_state == ST_IDLE) || (system_state == ST_ERROR))
      {
        System_StartPlayback();
      }
      break;

    case KEY4_PRESSED:
      if ((system_state == ST_PLAY) || (system_state == ST_PLAY_PAUSE))
      {
        if (System_StopPlayback(0U) == HAL_OK)
        {
          System_StartPlayback();
        }
      }
      else if ((system_state == ST_IDLE) || (system_state == ST_ERROR))
      {
        System_StartPlayback();
      }
      break;

    default:
      break;
  }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* 致命错误指示: 直接驱动 PC13 LED 快闪(不依赖中断/时钟初始化是否完成) */
  __disable_irq();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  /* PC13 配置为推挽输出: 清 CNF13/MODE13 后置 MODE13=01 */
  GPIOC->CRH &= ~((uint32_t)0xFU << 24);
  GPIOC->CRH |= ((uint32_t)0x1U << 24);
  while (1)
  {
    volatile uint32_t i;
    GPIOC->BSRR = GPIO_PIN_13;                  /* 灭 */
    for (i = 0U; i < 300000U; ++i) { }
    GPIOC->BSRR = (uint32_t)GPIO_PIN_13 << 16;  /* 亮(低电平点亮) */
    for (i = 0U; i < 300000U; ++i) { }
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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
