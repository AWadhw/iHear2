/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "fatfs.h"
#include "pdm2pcm.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include <string.h>
#include <stdarg.h> //for va_list var arg functions
#include <audio_sd.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define NUM_AUDIO_IN_CHANNELS 2 //TODO: Change when switching to SAI
#define CHANNEL_DEMUX_MASK                    	0x55U
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CRC_HandleTypeDef hcrc;

I2S_HandleTypeDef hi2s2;
DMA_HandleTypeDef hdma_spi2_rx;

SPI_HandleTypeDef hspi5;

TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
int32_t CONVERT_AUDIO_IN_PDMToPCM(uint16_t *PDMBuf, uint16_t *PCMBuf);
//uint16_t PDM_Buffer[((((AUDIO_IN_CHANNELS * AUDIO_IN_SAMPLING_FREQUENCY) / 1000) * MAX_DECIMATION_FACTOR) / 16)* N_MS ];
uint16_t dataIn_PDM[NUM_AUDIO_IN_CHANNELS * 48* 8]; //TODO: change 16 to 48 and maybe 8 to 4
uint16_t processedData[NUM_AUDIO_IN_CHANNELS * 48]; //out from pdm2pcm
static uint16_t I2S_InternalBuffer[768]; //holder before data is interleaved

volatile uint16_t sample_i2s;
volatile uint16_t myData; //debug variable for console


volatile int8_t half_i2s, full_i2s; //TODO: check
volatile uint8_t button_flag, start_stop_recording;

uint32_t pcmErr = 0;

//BlackMagic
static uint8_t Channel_Demux[128] = {
  0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03,
  0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03,
  0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07,
  0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07,
  0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03,
  0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03,
  0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07,
  0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07,
  0x08, 0x09, 0x08, 0x09, 0x0a, 0x0b, 0x0a, 0x0b,
  0x08, 0x09, 0x08, 0x09, 0x0a, 0x0b, 0x0a, 0x0b,
  0x0c, 0x0d, 0x0c, 0x0d, 0x0e, 0x0f, 0x0e, 0x0f,
  0x0c, 0x0d, 0x0c, 0x0d, 0x0e, 0x0f, 0x0e, 0x0f,
  0x08, 0x09, 0x08, 0x09, 0x0a, 0x0b, 0x0a, 0x0b,
  0x08, 0x09, 0x08, 0x09, 0x0a, 0x0b, 0x0a, 0x0b,
  0x0c, 0x0d, 0x0c, 0x0d, 0x0e, 0x0f, 0x0e, 0x0f,
  0x0c, 0x0d, 0x0c, 0x0d, 0x0e, 0x0f, 0x0e, 0x0f
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2S2_Init(void);
static void MX_CRC_Init(void);
static void MX_SPI5_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */
void myprintf(const char *fmt, ...);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void myprintf(const char *fmt, ...) {
  static char buffer[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  int len = strlen(buffer);
  HAL_UART_Transmit(&huart2, (uint8_t*)buffer, len, -1);
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
  MX_USART2_UART_Init();
  MX_I2S2_Init();
  MX_CRC_Init();
  MX_PDM2PCM_Init();
  MX_SPI5_Init();
  MX_FATFS_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  //HAL_I2S_DMAStop(&hi2s2); //size in bytes so we divide by 2 TODO:check
  //HAL_I2S_Receive_DMA(&hi2s2, (uint16_t *)dataIn_PDM, sizeof(dataIn_PDM)/2);
  HAL_Delay(1000);
  /*BELOW ADDED FOR SD_CARD*/
  sd_card_init();
  //PDM_Filter(&dataIn_PDM[0], &processedData[0], &PDM1_filter_handler);
  //dump_audio_content((uint8_t*)processedData, WAV_WRITE_SAMPLE_COUNT);
  //sd_demo();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
//	  printf("Hello World \n");
//	  HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
//	  HAL_Delay(1000);
	  if(button_flag)
	  {
		  HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
		  if(start_stop_recording) { //1
			  if (HAL_I2S_DMAStop(&hi2s2) != HAL_OK) {
				 myprintf("Big error in recording");
			  }
			  start_stop_recording = 0;
			  stop_recording();
			  half_i2s = 0;
			  full_i2s = 0;
			  printf("stop recording \n");
			  myprintf("Stop recording \n");
		  }
		  else {
			  start_stop_recording = 1;
			  start_recording(I2S_AUDIOFREQ_48K);
			  printf("start recording \n");
			  myprintf("Start recording \n");
			  myprintf("start_recording %d and %d\n", half_i2s, full_i2s);
			  uint32_t myArrSize = sizeof(dataIn_PDM);
			  myprintf("The size of my array is: %i", myArrSize);
			  //HAL_I2S_Receive_DMA(&hi2s2, (uint16_t *)dataIn_PDM, sizeof(dataIn_PDM)/2);
			  //CHECK: AudioInCtx[Instance].Size = (PDM_Clock_Freq/8U) * 2U * N_MS_PER_INTERRUPT;
			  if (HAL_I2S_Receive_DMA(&hi2s2, &I2S_InternalBuffer[0], (uint16_t)768/2U) != HAL_OK) {
				  myprintf("Big error in DMA");
			  }
		  }
		  button_flag = 0;
	  }
	  //myprintf("PDM Data is: %x", dataIn_PDM[0]);
	  if(start_stop_recording == 1 && half_i2s == 1) {
//		  PDM_Filter(&dataIn_PDM[0], &processedData[0], &PDM1_filter_handler);
//		  myData = processedData[0];
		  half_i2s = 0;
		  CONVERT_AUDIO_IN_PDMToPCM((uint16_t *)dataIn_PDM, processedData);
		  dump_audio_content((uint8_t*)processedData, WAV_WRITE_SAMPLE_COUNT);
		  //myprintf("audio dumped!");

	  }
	  if(start_stop_recording == 1 && full_i2s == 1) {
//		  PDM_Filter(&dataIn_PDM[0], &processedData[0], &PDM1_filter_handler);
//		  myData = processedData[0];
		  full_i2s = 0;
		  CONVERT_AUDIO_IN_PDMToPCM((uint16_t *)dataIn_PDM, processedData);
		  dump_audio_content((uint8_t*)processedData + WAV_WRITE_SAMPLE_COUNT, WAV_WRITE_SAMPLE_COUNT);

	  }
  }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 96;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CRC Initialization Function
  * @param None
  * @retval None
  */
static void MX_CRC_Init(void)
{

  /* USER CODE BEGIN CRC_Init 0 */

  /* USER CODE END CRC_Init 0 */

  /* USER CODE BEGIN CRC_Init 1 */

  /* USER CODE END CRC_Init 1 */
  hcrc.Instance = CRC;
  if (HAL_CRC_Init(&hcrc) != HAL_OK)
  {
    Error_Handler();
  }
  __HAL_CRC_DR_RESET(&hcrc);
  /* USER CODE BEGIN CRC_Init 2 */

  /* USER CODE END CRC_Init 2 */

}

/**
  * @brief I2S2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2S2_Init(void)
{

  /* USER CODE BEGIN I2S2_Init 0 */

  /* USER CODE END I2S2_Init 0 */

  /* USER CODE BEGIN I2S2_Init 1 */

  /* USER CODE END I2S2_Init 1 */
  hi2s2.Instance = SPI2;
  hi2s2.Init.Mode = I2S_MODE_MASTER_RX;
  hi2s2.Init.Standard = I2S_STANDARD_MSB;
  hi2s2.Init.DataFormat = I2S_DATAFORMAT_16B_EXTENDED;
  hi2s2.Init.MCLKOutput = I2S_MCLKOUTPUT_DISABLE;
  hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_48K;
  hi2s2.Init.CPOL = I2S_CPOL_LOW;
  hi2s2.Init.ClockSource = I2S_CLOCK_PLL;
  hi2s2.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_DISABLE;
  if (HAL_I2S_Init(&hi2s2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2S2_Init 2 */

  /* USER CODE END I2S2_Init 2 */

}

/**
  * @brief SPI5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI5_Init(void)
{

  /* USER CODE BEGIN SPI5_Init 0 */

  /* USER CODE END SPI5_Init 0 */

  /* USER CODE BEGIN SPI5_Init 1 */

  /* USER CODE END SPI5_Init 1 */
  /* SPI5 parameter configuration*/
  hspi5.Instance = SPI5;
  hspi5.Init.Mode = SPI_MODE_MASTER;
  hspi5.Init.Direction = SPI_DIRECTION_2LINES;
  hspi5.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi5.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi5.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi5.Init.NSS = SPI_NSS_SOFT;
  hspi5.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
  hspi5.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi5.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi5.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi5.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi5) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI5_Init 2 */

  /* USER CODE END SPI5_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_SlaveConfigTypeDef sSlaveConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 1;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sSlaveConfig.SlaveMode = TIM_SLAVEMODE_EXTERNAL1;
  sSlaveConfig.InputTrigger = TIM_TS_TI1FP1;
  sSlaveConfig.TriggerPolarity = TIM_TRIGGERPOLARITY_RISING;
  sSlaveConfig.TriggerFilter = 0;
  if (HAL_TIM_SlaveConfigSynchro(&htim3, &sSlaveConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 1;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SD_CS_Pin */
  GPIO_InitStruct.Pin = SD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(SD_CS_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
int _write(int file, char *ptr, int len) {

	for (int DataIdx = 0; DataIdx < len; DataIdx++) {
		ITM_SendChar(*ptr++);
	}
	return len;
}

void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s) {
//	//sample_i2s = dataIn_PDM[0];
//	pcmErr = PDM_Filter(&dataIn_PDM[0], &processedData[0], &PDM1_filter_handler);
//	if (pcmErr != 0) {
//		myprintf("Error is: %i", pcmErr);
//	}
//
//	//MX_PDM2PCM_Process(&data_i2s[0], &processedData[0]);
//	myData = processedData[0];
//	full_i2s = 1;
	//myprintf("Reached Callback");
	uint32_t index;
	uint16_t * DataTempI2S = &(I2S_InternalBuffer[768/2U]);
  uint8_t a,b;
  for(index=0; index<(768/2U); index++)
  {
	  a = ((uint8_t *)(DataTempI2S))[(index*2U)];
	 b = ((uint8_t *)(DataTempI2S))[(index*2U)+1U];
	 ((uint8_t *)(dataIn_PDM))[(index*2U)] = Channel_Demux[a & CHANNEL_DEMUX_MASK] | (Channel_Demux[b & CHANNEL_DEMUX_MASK] << 4);;
	 ((uint8_t *)(dataIn_PDM))[(index*2U)+1U] = Channel_Demux[(a>>1) & CHANNEL_DEMUX_MASK] | (Channel_Demux[(b>>1) & CHANNEL_DEMUX_MASK] << 4);
  }
  full_i2s = 1;
  myData = processedData[0];
}

void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s) {
	//sample_i2s = dataIn_PDM[0];
//	pcmErr = PDM_Filter(&dataIn_PDM[0], &processedData[0], &PDM1_filter_handler);
//	if (pcmErr != 0) {
//			myprintf("Error is: %i", pcmErr);
//	}
//	myData = processedData[0];
//	half_i2s = 1;
	uint32_t index;
	uint16_t * DataTempI2S = I2S_InternalBuffer;
  uint8_t a,b;
  for(index=0; index<384; index++)
  {
	a = ((uint8_t *)(DataTempI2S))[(index*2U)];
	b = ((uint8_t *)(DataTempI2S))[(index*2U)+1U];
	((uint8_t *)(dataIn_PDM))[(index*2U)] = Channel_Demux[a & CHANNEL_DEMUX_MASK] |
	  (Channel_Demux[b & CHANNEL_DEMUX_MASK] << 4);;
	  ((uint8_t *)(dataIn_PDM))[(index*2U)+1U] = Channel_Demux[(a>>1) & CHANNEL_DEMUX_MASK] |
		(Channel_Demux[(b>>1) & CHANNEL_DEMUX_MASK] << 4);
  }
  myData = processedData[0];
  half_i2s = 1;
}

//TODO: might need different filter handlers
int32_t CONVERT_AUDIO_IN_PDMToPCM(uint16_t *PDMBuf, uint16_t *PCMBuf) {
	uint32_t index;
	for(index = 0; index < NUM_AUDIO_IN_CHANNELS; index++) {
		(void)PDM_Filter(&((uint8_t*)(PDMBuf))[index], (uint16_t*)&(PCMBuf[index]), &PDM1_filter_handler);
	}
	return 1;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if(GPIO_Pin == B1_Pin) {
		button_flag = 1;
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
