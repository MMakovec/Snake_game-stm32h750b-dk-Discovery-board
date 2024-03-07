/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "mt48lc4m32b2.h"
#include "ft5336.h"
#include "snake.h"
#include "stm32h7xx_hal_uart.h"
#include "font_SegoeUI10pt.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

// Kdaj pride do HardFault - teorija: problem nastane pri visjih hitrostih LTDC->snake
// interrupta, ko pojemo sadje,
// saj rng() pri generaciji sadja pocasen -> dobimo nested interrupt (funkcija snake ni
// reentrant)



/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/*------- framebuffers -------- */
uint16_t* framebuffer1_l1 = (uint16_t*)0xD0000000;
uint16_t* framebuffer1_l2 = (uint16_t*)0xD01FE0FF;

uint16_t* framebuffer2_l1 = (uint16_t*)0xD03FC1FF;
uint16_t* framebuffer2_l2 = (uint16_t*)0xD05FA2FF;

// global variables
uint8_t uart_direction = 'd';
uint8_t direction = RIGHT;
uint8_t running = 0;
uint8_t start = 0;
uint16_t score = 0;
uint16_t highscore = 0;
uint8_t ltdc_rdy = 0;

// semaphores
uint8_t snake_running = 0;
uint8_t drawing = 0;


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

DMA2D_HandleTypeDef hdma2d;

LTDC_HandleTypeDef hltdc;

UART_HandleTypeDef huart3;

SDRAM_HandleTypeDef hsdram2;

/* USER CODE BEGIN PV */

// snake head
Node* head;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Initialize(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA2D_Init(void);
static void MX_LTDC_Init(void);
static void MX_FMC_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
char USART_ReadChar(void)
{
    // Wait until the data is available in the Data Register
	while(!(USART1->ISR & USART_ISR_RXNE_RXFNE))
	{

	}
    char receivedChar = USART1->RDR;
    return receivedChar;
}

void draw_square(uint16_t col, uint16_t dims, uint16_t* buffer, uint16_t posx, uint16_t posy){
	for(int j = 0; j<dims; j++){
		for(int i = 0; i<dims; i++){
			buffer[i+posx+posy*LTDC_WIDTH+j*LTDC_WIDTH] = col;
		}
	}
}

void DMA2D_FillRect(uint32_t color, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint16_t* framebuffer)
{

  hdma2d.Init.Mode = DMA2D_R2M;
  hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
  hdma2d.Init.OutputOffset = LTDC_WIDTH - width;
  HAL_DMA2D_Init(&hdma2d);
  HAL_DMA2D_Start(
    &hdma2d,
    color,
    (uint32_t) &framebuffer[x+y*480],
    width,
    height);
  HAL_DMA2D_PollForTransfer(&hdma2d, 10);
}

uint16_t * switch_buffers(void){
	static uint8_t buffer = 0;
	if(buffer){
		HAL_LTDC_SetAddress(&hltdc, 0xD0000000, LTDC_LAYER_1);
		HAL_LTDC_SetAddress(&hltdc, 0xD01FE0FF, LTDC_LAYER_2);
		buffer = !buffer;
		return framebuffer2_l2;
	}
	else{
		HAL_LTDC_SetAddress(&hltdc, 0xD03FC1FF, LTDC_LAYER_1);
		HAL_LTDC_SetAddress(&hltdc, 0xD05FA2FF, LTDC_LAYER_2);
		buffer = !buffer;
		return framebuffer1_l2;
	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	// restart the interrupt reception mode
	HAL_UART_Receive_IT(&huart3,&uart_direction,1);
	if(ltdc_rdy){
		switch(direction){
			case LEFT:
				switch(uart_direction){
					case 'w':
						direction = UP;
						break;
					case 's':
						direction = DOWN;
						break;
				}
				break;
			case RIGHT:
				switch(uart_direction){
					case 'w':
						direction = UP;
						break;
					case 's':
						direction = DOWN;
						break;
				}
				break;
			case UP:
				switch(uart_direction){
					case 'a':
						direction = LEFT;
						break;
					case 'd':
						direction = RIGHT;
						break;
				}
				break;
			case DOWN:
				switch(uart_direction){
					case 'a':
						direction = LEFT;
						break;
					case 'd':
						direction = RIGHT;
						break;
				}
				break;
		}
		ltdc_rdy = 0;
	}
}

void HAL_LTDC_LineEventCallback(LTDC_HandleTypeDef* hltdc)
{
  if(!drawing){
	static uint8_t difficulty;
	static int count = 0;
	count++;

	if(score<MAX_SPEED) difficulty = score;

	// init snake
	if(count > 10){
	  if(running<2){
		  uint16_t *framebuffer = switch_buffers();
		  // clear framebuffer
		  for(int i = 0; i < LTDC_WIDTH*LTDC_HEIGHT; i++){
			  framebuffer[i] = 0xFFFF;
		  }
		  if(!running){
			  // make snake
			  Node root;
			  head = &root;
			  make_snake(head);
		  }

		  running++;
	  }
	}
	if(running == 2){
	  // draw snake
	  uint16_t *framebuffer = switch_buffers();
	  drawLinkedlist(head, framebuffer);
	  running++;
	}

	if (((count+difficulty) >= 20) && !snake_running)
	{
	uint16_t *framebuffer = switch_buffers();
	snake(framebuffer, &head);
	ltdc_rdy = 1;
	count = 0;
	}
  }
}


void draw_bitmap(const uint8_t* bitmap, uint16_t pos, uint16_t* framebuffer1, uint16_t* framebuffer2){
   for(int j = 0; j<13; j++){
	   for(int i = pos; i < (pos+8); i++)
	   {
		 if(bitmap[j]<<(i-pos) & 0x80){
			 framebuffer1[i+j*480] = 0x0000;
			 //framebuffer2[i+j*480] = 0x0000;
		 }
	   }
   }
}

void calc_score(const uint8_t** B_sc, uint8_t* size){
	const uint8_t* nums[] = {B_0,B_1,B_2,B_3,B_4,B_5,B_6,B_7,B_8,B_9};
	if(score/10 >= 10){
		*size = 3;
		B_sc[0] = nums[score/100];
		B_sc[1] = nums[(score%100)/10];
		B_sc[2] = nums[score%10];
	}
	else if(score/10 != 0){
		*size = 2;
		B_sc[0] = nums[score/10];
		B_sc[1] = nums[score%10];
	}
	else{
		*size = 1;
		B_sc[0] = nums[score];
	}
}

void draw_score(uint16_t* framebuffer1, uint16_t* framebuffer2, int* pos){
	drawing = 1;
    const uint8_t* score_msg[6] = {B_S,B_c,B_o,B_r,B_e,B_colon};
	for(int i = 0; i<6; i++){
		draw_bitmap(score_msg[i], *pos, framebuffer1, framebuffer2);
		*pos+=score_msg[i][13]+char_gap;
	}
	drawing = 0;
}

void increase_score(uint16_t* framebuffer1, uint16_t* framebuffer2, uint16_t col, int pos){
	drawing = 1;
	uint8_t B_sc_size = 0;
	const uint8_t* B_score[4];
	calc_score(B_score, &B_sc_size);

	// clear old score
	for(int j = 0; j<13; j++){
	   for(int i = pos; i < pos+(8*3); i++){
		   framebuffer1[i+j*480] = col;
		   //framebuffer2[i+j*480] = col;
	   }
	}
	for(int i = 0; i<B_sc_size; i++){
		draw_bitmap(B_score[i], pos, framebuffer1, framebuffer2);
		pos+=B_score[i][13]+char_gap;
	}
	drawing = 0;
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

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  SystemCoreClockUpdate(); //64Mhz
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA2D_Init();
  MX_LTDC_Init();
  MX_FMC_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_LTDC_SetAddress(&hltdc, 0xD03FC1FF, LTDC_LAYER_1);
  HAL_LTDC_SetAddress(&hltdc, 0xD05FA2FF, LTDC_LAYER_2);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  FMC_SDRAM_CommandTypeDef Command;

  /* Step 1: Configure a clock configuration enable command */
  Command.CommandMode            = FMC_SDRAM_CMD_CLK_ENABLE;
  Command.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK2;
  Command.AutoRefreshNumber      = 1;
  Command.ModeRegisterDefinition = 0;

  /* Send the command */
  uint32_t SDRAM_TIMEOUT = 1000;
  HAL_SDRAM_SendCommand(&hsdram2, &Command, SDRAM_TIMEOUT);


  HAL_UART_Receive_IT(&huart3,&uart_direction,1);
  //------------------------- main program ----------------------


  uint8_t r1 = 0x00, g1 = 0xff, b1 = 0x00;             	   // Color select
  uint16_t col1 = ((r1>>3)<<11) | ((g1>>2)<<5) | (b1>>3);  // Convert colors to RGB565
  // Put colors into the framebuffer
  for(int i = 0; i < LTDC_WIDTH*LTDC_HEIGHT; i++)
  {
    framebuffer1_l1[i] = col1;
    framebuffer2_l1[i] = col1;
  }

  //DMA2D_FillRect(col1, 0, 0, LTDC_WIDTH, LTDC_HEIGHT, framebuffer1_l1);
  //DMA2D_FillRect(col1, 0, 0, LTDC_WIDTH, LTDC_HEIGHT, framebuffer2_l1);

  // scoreboard
  int pos = 0;
  draw_score(framebuffer1_l1, framebuffer2_l1, &pos);
  uint8_t old_score = -1;

  while (1)
  {
    /* USER CODE END WHILE */
	  /*
	  if(old_score!=score && start == 2){
		  old_score = score;
		  increase_score(framebuffer1_l1, framebuffer2_l1, col1, pos);
	  }
	  */
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

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 160;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief DMA2D Initialization Function
  * @param None
  * @retval None
  */
static void MX_DMA2D_Init(void)
{

  /* USER CODE BEGIN DMA2D_Init 0 */

  /* USER CODE END DMA2D_Init 0 */

  /* USER CODE BEGIN DMA2D_Init 1 */

  /* USER CODE END DMA2D_Init 1 */
  hdma2d.Instance = DMA2D;
  hdma2d.Init.Mode = DMA2D_M2M_BLEND;
  hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
  hdma2d.Init.OutputOffset = 0;
  hdma2d.LayerCfg[1].InputOffset = 0;
  hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_RGB565;
  hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
  hdma2d.LayerCfg[1].InputAlpha = 0;
  hdma2d.LayerCfg[1].AlphaInverted = DMA2D_REGULAR_ALPHA;
  hdma2d.LayerCfg[1].RedBlueSwap = DMA2D_RB_REGULAR;
  hdma2d.LayerCfg[1].ChromaSubSampling = DMA2D_NO_CSS;
  hdma2d.LayerCfg[0].InputOffset = 0;
  hdma2d.LayerCfg[0].InputColorMode = DMA2D_INPUT_RGB565;
  hdma2d.LayerCfg[0].AlphaMode = DMA2D_NO_MODIF_ALPHA;
  hdma2d.LayerCfg[0].InputAlpha = 0;
  hdma2d.LayerCfg[0].AlphaInverted = DMA2D_REGULAR_ALPHA;
  hdma2d.LayerCfg[0].RedBlueSwap = DMA2D_RB_REGULAR;
  if (HAL_DMA2D_Init(&hdma2d) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_DMA2D_ConfigLayer(&hdma2d, 0) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_DMA2D_ConfigLayer(&hdma2d, 1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DMA2D_Init 2 */

  /* USER CODE END DMA2D_Init 2 */

}

/**
  * @brief LTDC Initialization Function
  * @param None
  * @retval None
  */
static void MX_LTDC_Init(void)
{

  /* USER CODE BEGIN LTDC_Init 0 */

  /* USER CODE END LTDC_Init 0 */

  LTDC_LayerCfgTypeDef pLayerCfg = {0};
  LTDC_LayerCfgTypeDef pLayerCfg1 = {0};

  /* USER CODE BEGIN LTDC_Init 1 */


  /* USER CODE END LTDC_Init 1 */
  hltdc.Instance = LTDC;
  hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
  hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
  hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AH;
  hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;
  hltdc.Init.HorizontalSync = 2;
  hltdc.Init.VerticalSync = 9;
  hltdc.Init.AccumulatedHBP = 45;
  hltdc.Init.AccumulatedVBP = 21;
  hltdc.Init.AccumulatedActiveW = 525;
  hltdc.Init.AccumulatedActiveH = 293;
  hltdc.Init.TotalWidth = 533;
  hltdc.Init.TotalHeigh = 297;
  hltdc.Init.Backcolor.Blue = 0;
  hltdc.Init.Backcolor.Green = 0;
  hltdc.Init.Backcolor.Red = 0;
  if (HAL_LTDC_Init(&hltdc) != HAL_OK)
  {
    Error_Handler();
  }
  pLayerCfg.WindowX0 = 0;
  pLayerCfg.WindowX1 = 480;
  pLayerCfg.WindowY0 = 0;
  pLayerCfg.WindowY1 = 272;
  pLayerCfg.PixelFormat = LTDC_PIXEL_FORMAT_RGB565;
  pLayerCfg.Alpha = 200;
  pLayerCfg.Alpha0 = 0;
  pLayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
  pLayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
  pLayerCfg.FBStartAdress = 0xD0000000;
  pLayerCfg.ImageWidth = 480;
  pLayerCfg.ImageHeight = 272;
  pLayerCfg.Backcolor.Blue = 0;
  pLayerCfg.Backcolor.Green = 0;
  pLayerCfg.Backcolor.Red = 0;
  if (HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg, 0) != HAL_OK)
  {
    Error_Handler();
  }
  pLayerCfg1.WindowX0 = 0;
  pLayerCfg1.WindowX1 = 480;
  pLayerCfg1.WindowY0 = 0;
  pLayerCfg1.WindowY1 = 272;
  pLayerCfg1.PixelFormat = LTDC_PIXEL_FORMAT_RGB565;
  pLayerCfg1.Alpha = 100;
  pLayerCfg1.Alpha0 = 0;
  pLayerCfg1.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
  pLayerCfg1.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
  pLayerCfg1.FBStartAdress = 0xD01FEFFF;
  pLayerCfg1.ImageWidth = 480;
  pLayerCfg1.ImageHeight = 272;
  pLayerCfg1.Backcolor.Blue = 0;
  pLayerCfg1.Backcolor.Green = 0;
  pLayerCfg1.Backcolor.Red = 0;
  if (HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg1, 1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LTDC_Init 2 */
  // enable interrupt
  LTDC->IER |= LTDC_IER_LIE;
  /* USER CODE END LTDC_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/* FMC initialization function */
static void MX_FMC_Init(void)
{

  /* USER CODE BEGIN FMC_Init 0 */

  /* USER CODE END FMC_Init 0 */

  FMC_SDRAM_TimingTypeDef SdramTiming = {0};

  /* USER CODE BEGIN FMC_Init 1 */

  /* USER CODE END FMC_Init 1 */

  /** Perform the SDRAM2 memory initialization sequence
  */
  hsdram2.Instance = FMC_SDRAM_DEVICE;
  /* hsdram2.Init */
  hsdram2.Init.SDBank = FMC_SDRAM_BANK2;
  hsdram2.Init.ColumnBitsNumber = FMC_SDRAM_COLUMN_BITS_NUM_8;
  hsdram2.Init.RowBitsNumber = FMC_SDRAM_ROW_BITS_NUM_12;
  hsdram2.Init.MemoryDataWidth = FMC_SDRAM_MEM_BUS_WIDTH_16;
  hsdram2.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
  hsdram2.Init.CASLatency = FMC_SDRAM_CAS_LATENCY_3;
  hsdram2.Init.WriteProtection = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
  hsdram2.Init.SDClockPeriod = FMC_SDRAM_CLOCK_PERIOD_2;
  hsdram2.Init.ReadBurst = FMC_SDRAM_RBURST_ENABLE;
  hsdram2.Init.ReadPipeDelay = FMC_SDRAM_RPIPE_DELAY_0;
  /* SdramTiming */
  SdramTiming.LoadToActiveDelay = 2;
  SdramTiming.ExitSelfRefreshDelay = 7;
  SdramTiming.SelfRefreshTime = 4;
  SdramTiming.RowCycleDelay = 7;
  SdramTiming.WriteRecoveryTime = 5;
  SdramTiming.RPDelay = 2;
  SdramTiming.RCDDelay = 2;

  if (HAL_SDRAM_Init(&hsdram2, &SdramTiming) != HAL_OK)
  {
    Error_Handler( );
  }

  /* USER CODE BEGIN FMC_Init 2 */
  MT48LC4M32B2_Context_t MT48LC4M32B2;
  MT48LC4M32B2.TargetBank = FMC_SDRAM_CMD_TARGET_BANK2;
  MT48LC4M32B2.RefreshMode = MT48LC4M32B2_AUTOREFRESH_MODE_CMD;
  MT48LC4M32B2.RefreshRate = REFRESH_COUNT;
  MT48LC4M32B2.BurstLength = MT48LC4M32B2_BURST_LENGTH_1;
  MT48LC4M32B2.BurstType = MT48LC4M32B2_BURST_TYPE_SEQUENTIAL;
  MT48LC4M32B2.CASLatency = MT48LC4M32B2_CAS_LATENCY_3;
  MT48LC4M32B2.OperationMode = MT48LC4M32B2_OPERATING_MODE_STANDARD;
  MT48LC4M32B2.WriteBurstMode = MT48LC4M32B2_WRITEBURST_MODE_SINGLE;

  MT48LC4M32B2_Init(&hsdram2, &MT48LC4M32B2);
  /* USER CODE END FMC_Init 2 */
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
  __HAL_RCC_GPIOK_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOI_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOJ_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_7, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOK, GPIO_PIN_0, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, MII_TX_ER_nINT_Pin|LCD_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD1_GPIO_Port, LD1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : PB4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF7_SPI2;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PD7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : USB_OTG_FS2_ID_Pin OTG_FS2_PSO_Pin */
  GPIO_InitStruct.Pin = USB_OTG_FS2_ID_Pin|OTG_FS2_PSO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PA8 */
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF1_TIM1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_INT_Pin */
  GPIO_InitStruct.Pin = LCD_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(LCD_INT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PK0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOK, &GPIO_InitStruct);

  /*Configure GPIO pin : OTG_FS2_OverCurrent_Pin */
  GPIO_InitStruct.Pin = OTG_FS2_OverCurrent_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(OTG_FS2_OverCurrent_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : MII_TX_ER_nINT_Pin LCD_RST_Pin */
  GPIO_InitStruct.Pin = MII_TX_ER_nINT_Pin|LCD_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : LD1_Pin */
  GPIO_InitStruct.Pin = LD1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : FDCAN2_TX_Pin */
  GPIO_InitStruct.Pin = FDCAN2_TX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF9_FDCAN2;
  HAL_GPIO_Init(FDCAN2_TX_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0xD0000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_16MB;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

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
