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



/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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
I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

/* USER CODE BEGIN PV */
/* Global variables for VS Code Live Watch */

// SHT45 Data
float live_sht45_temp = 0.0f;
float live_sht45_humidity = 0.0f;

// LIS3DHTR Data
int16_t live_lis3dh_x = 0;
int16_t live_lis3dh_y = 0;
int16_t live_lis3dh_z = 0;

// OPT3001 Data
float live_opt3001_lux = 0.0f;

// MAX31865 Data
float live_max31865_res = 0.0f;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
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
  MX_I2C1_Init();
  MX_SPI1_Init();
  
/* USER CODE BEGIN 2 */
  
  // 1. Initialize MAX31865 over SPI (Bias ON, Auto conversion, 3-wire/2-wire setup)
  // Ensure GPIOA and PIN_4 match your hardware setup for SPI Chip Select (CS)
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET); // Pull CS Low
  uint8_t spi_init_buf[2] = {0x00 | 0x80, 0xC2};        // Write 0xC2 to Config Register (0x00)
  HAL_SPI_Transmit(&hspi1, spi_init_buf, 2, HAL_MAX_DELAY);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);   // Pull CS High

  // 2. Initialize LIS3DHTR (Enable X,Y,Z axes, 100Hz Normal power mode)
  // 0x19 << 1 = 0x32
  uint8_t lis_init[2] = {0x20, 0x57}; // Register 0x20 (CTRL_REG1) -> Value 0x57
  HAL_I2C_Master_Transmit(&hi2c1, 0x32, lis_init, 2, HAL_MAX_DELAY);

  /* USER CODE END 2 */

/* INFINITE LOOP */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      // ==========================================
      // 1. READ SHT45 (Temperature & Humidity)
      // ==========================================
      uint8_t sht_cmd = 0xFD; // High precision command
      uint8_t sht_rx[6];
      // 0x44 << 1 = 0x88
      if(HAL_I2C_Master_Transmit(&hi2c1, 0x88, &sht_cmd, 1, HAL_MAX_DELAY) == HAL_OK) {
          HAL_Delay(10); // Wait for conversion to complete
          if(HAL_I2C_Master_Receive(&hi2c1, 0x88, sht_rx, 6, HAL_MAX_DELAY) == HAL_OK) {
              uint16_t t_ticks = (sht_rx[0] << 8) | sht_rx[1];
              uint16_t rh_ticks = (sht_rx[3] << 8) | sht_rx[4];
              
              live_sht45_temp = -45.0f + 175.0f * (float)t_ticks / 65535.0f;
              live_sht45_humidity = -6.0f + 125.0f * (float)rh_ticks / 65535.0f;
          }
      }

      // ==========================================
      // 2. READ LIS3DHTR (Accelerometer Axes)
      // ==========================================
      uint8_t lis_reg = 0x28 | 0x80; // OUT_X_L register address with auto-increment bit
      uint8_t lis_rx[6];
      // 0x19 << 1 = 0x32
      if(HAL_I2C_Master_Transmit(&hi2c1, 0x32, &lis_reg, 1, HAL_MAX_DELAY) == HAL_OK) {
          if(HAL_I2C_Master_Receive(&hi2c1, 0x32, lis_rx, 6, HAL_MAX_DELAY) == HAL_OK) {
              live_lis3dh_x = ((int16_t)(lis_rx[1] << 8 | lis_rx[0])) >> 4;
              live_lis3dh_y = ((int16_t)(lis_rx[3] << 8 | lis_rx[2])) >> 4;
              live_lis3dh_z = ((int16_t)(lis_rx[5] << 8 | lis_rx[4])) >> 4;
          }
      }

      // ==========================================
      // 3. READ OPT3001 (Ambient Light Intensity)
      // ==========================================
      uint8_t opt_reg = 0x00; // Result register address
      uint8_t opt_rx[2];
      // 0x45 << 1 = 0x8A
      if(HAL_I2C_Master_Transmit(&hi2c1, 0x8A, &opt_reg, 1, HAL_MAX_DELAY) == HAL_OK) {
          if(HAL_I2C_Master_Receive(&hi2c1, 0x8A, opt_rx, 2, HAL_MAX_DELAY) == HAL_OK) {
              uint16_t result = (opt_rx[0] << 8) | opt_rx[1];
              uint16_t exponent = (result & 0xF000) >> 12;
              uint16_t mantissa = (result & 0x0FFF);
              live_opt3001_lux = 0.01f * (float)(1 << exponent) * (float)mantissa;
          }
      }

      // ==========================================
      // 4. READ MAX31865 (RTD Temperature Probe)
      // ==========================================
      uint8_t rtd_reg = 0x01; // RTD MSB address
      uint8_t rtd_rx[3] = {0}; 
      
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET); // CS Low
      HAL_SPI_Transmit(&hspi1, &rtd_reg, 1, HAL_MAX_DELAY);
      HAL_SPI_Receive(&hspi1, &rtd_rx[1], 2, HAL_MAX_DELAY);
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);   // CS High

      uint16_t rtd_raw = (rtd_rx[1] << 8) | rtd_rx[2];
      if ((rtd_raw & 0x01) == 0) { // If no fault bit is flagged
          rtd_raw >>= 1;           // Shift out fault bit
          live_max31865_res = ((float)rtd_raw * 430.0f) / 32768.0f; // Calculation for a 430 Ohm reference resistor
      }

      HAL_Delay(250);
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

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_11;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 8;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00B07CB4;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_4BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
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
