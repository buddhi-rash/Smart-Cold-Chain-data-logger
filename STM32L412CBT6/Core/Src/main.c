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
#include <math.h>
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

/* USER CODE BEGIN PV */
float SHT45_Temperature = 0.0f;
float SHT45_Humidity = 0.0f;

int16_t LIS3DH_X = 0;
int16_t LIS3DH_Y = 0;
int16_t LIS3DH_Z = 0;

float OPT3001_Lux = 0.0f;

#define SHT45_ADDR     (0x44 << 1)
#define LIS3DH_ADDR    (0x18 << 1)
#define OPT3001_ADDR   (0x45 << 1)
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */
void LIS3DHTR_Init(void);
void Read_SHT45(void);
void Read_LIS3DHTR(void);
void Read_OPT3001(void);
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
  /* USER CODE BEGIN 2 */
  LIS3DHTR_Init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    Read_SHT45();
    Read_LIS3DHTR();
    Read_OPT3001();
    HAL_Delay(500); // Delay 1 second between readings
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
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
  hi2c1.Init.Timing = 0x10B17DB5;
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
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/**
  * @brief Initializes LIS3DHTR: Sets power mode to Normal (100 Hz), enables all axes.
  */
void LIS3DHTR_Init(void)
{
    uint8_t config[2]= {0x20, 0x5F};
    // CTRL_REG1 (0x20) -> Value 0x57 (100Hz data rate, Normal power mode, X/Y/Z enabled)
    HAL_I2C_Master_Transmit(&hi2c1, LIS3DH_ADDR, config, 2, HAL_MAX_DELAY);
}

/**
  * @brief Reads and parses Temperature and Humidity from SHT45
  */
void Read_SHT45(void)
{
    uint8_t cmd = 0xFD; // High precision measurement command
    uint8_t data[6] = {0};

    // Send measurement command
    if (HAL_I2C_Master_Transmit(&hi2c1, SHT45_ADDR, &cmd, 1, HAL_MAX_DELAY) == HAL_OK)
    {
        HAL_Delay(10); // Wait for measurement to complete (~max 8.3ms)
        
        // Read 6 bytes (2B Temp + 1B CRC + 2B Humidity + 1B CRC)
        if (HAL_I2C_Master_Receive(&hi2c1, SHT45_ADDR, data, 6, HAL_MAX_DELAY) == HAL_OK)
        {
            uint16_t t_ticks = (data[0] << 8) | data[1];
            uint16_t rh_ticks = (data[3] << 8) | data[4];

            // Formulas provided by Sensirion datasheet
            SHT45_Temperature = -45.0f + 175.0f * ((float)t_ticks / 65535.0f);
            SHT45_Humidity = -6.0f + 125.0f * ((float)rh_ticks / 65535.0f);
            

        }
    }
}

/**
  * @brief Reads X, Y, and Z raw data from LIS3DHTR
  */
void Read_LIS3DHTR(void)
{
    uint8_t reg = 0x28 | 0x80; // OUT_X_L register address. Bit 7 set to 1 for auto-increment.
    uint8_t data[6] = {0};

    if (HAL_I2C_Master_Transmit(&hi2c1, LIS3DH_ADDR, &reg, 1, HAL_MAX_DELAY) == HAL_OK)
    {
        if (HAL_I2C_Master_Receive(&hi2c1, LIS3DH_ADDR, data, 6, HAL_MAX_DELAY) == HAL_OK)
        {
            // Combine Low and High bytes for each axis
            LIS3DH_X = ((int16_t)((data[1] << 8) | data[0]))>> 8;
            LIS3DH_Y = ((int16_t)((data[3] << 8) | data[2]))>> 8;
            LIS3DH_Z = ((int16_t)((data[5] << 8) | data[4]))>> 8;
        }
    }
}

/**
  * @brief Reads and decodes Light Intensity (Lux) from OPT3001
  */
void Read_OPT3001(void)
{
    uint8_t reg = 0x00; // Result Register address
    uint8_t data[2] = {0};

    if (HAL_I2C_Master_Transmit(&hi2c1, OPT3001_ADDR, &reg, 1, HAL_MAX_DELAY) == HAL_OK)
    {
        if (HAL_I2C_Master_Receive(&hi2c1, OPT3001_ADDR, data, 2, HAL_MAX_DELAY) == HAL_OK)
        {
            uint16_t raw_reg = (data[0] << 8) | data[1];
            
            // OPT3001 Result layout: [4-bit Exponent][12-bit Mantissa]
            uint8_t exponent = (raw_reg >> 12) & 0x0F;
            uint16_t mantissa = raw_reg & 0x0FFF;

            // Lux calculation formula from TI datasheet
            OPT3001_Lux = 0.01f * (float)(1 << exponent) * (float)mantissa;
        }
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
