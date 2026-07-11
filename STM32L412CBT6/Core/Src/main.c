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
#include "fatfs.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "Sensors.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// SPI Command to read the chip's ID
#define W25_CMD_JEDEC_ID 0x9F
// SPI Command to read standard data
#define W25_CMD_READ 0x03
#define W25_CMD_WRITE_ENABLE 0x06
#define W25_CMD_READ_STATUS  0x05
#define W25_CMD_SECTOR_ERASE 0x20
#define W25_CMD_PAGE_PROGRAM 0x02

#define W25_CS_LOW()  HAL_GPIO_WritePin(Storage_CS_GPIO_Port, Storage_CS_Pin, GPIO_PIN_RESET)
#define W25_CS_HIGH() HAL_GPIO_WritePin(Storage_CS_GPIO_Port, Storage_CS_Pin, GPIO_PIN_SET)
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;

/* USER CODE BEGIN PV */
// Array to store the 3 bytes the chip sends back
uint8_t flash_id[3];
uint8_t test_read_buffer[10]; // Buffer to hold our read test
float check = 0.0f; // Variable to hold the floating-point value to be written to the CSV file

FATFS fs;           // The File System Object
FIL fil;            // The File Object
UINT bytesWritten;  // Counter for bytes written

char log_buffer[128];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C2_Init(void);
static void MX_RTC_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */
void SPI_Set_Mode_Flash(void);
void SPI_Set_Mode_MAX31865(void);
void W25_Read_ID(void);
void W25_Read_Data(uint32_t address, uint8_t* buffer, uint32_t length);
void W25_Wait_For_Ready(void);
void W25_Write_Enable(void);
void W25_Erase_Sector(uint32_t address);
void W25_Write_Page(uint32_t address, uint8_t* buffer, uint16_t length);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Shifts the SPI bus into Mode 0 for the Memory Chip
void SPI_Set_Mode_Flash(void)
{
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    HAL_SPI_Init(&hspi1);
}

// Shifts the SPI bus into Mode 3 for the PT100 Sensor
void SPI_Set_Mode_MAX31865(void)
{
    hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
    hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
    HAL_SPI_Init(&hspi1);
}
void W25_Read_ID(void)
{
  SPI_Set_Mode_Flash(); // Ensure SPI is in Mode 0 for the flash chip

  uint8_t cmd = W25_CMD_JEDEC_ID;
    
  // 1. Pull CS Low to wake up the memory chip
  W25_CS_LOW();
    
  // 2. Transmit the 0x9F command over MOSI
  HAL_SPI_Transmit(&hspi1, &cmd, 1, 100);
    
  // 3. Receive the 3-byte response over MISO
  HAL_SPI_Receive(&hspi1, flash_id, 3, 100);
    
  // 4. Pull CS High to put the chip back to sleep
  W25_CS_HIGH();
}

void W25_Read_Data(uint32_t address, uint8_t* buffer, uint32_t length)
{
    SPI_Set_Mode_Flash(); // Ensure SPI is in Mode 0 for the flash chip
    uint8_t spi_data[4];

    // 1. Setup the command and 24-bit address (MSB first)
    spi_data[0] = W25_CMD_READ;                 // Instruction
    spi_data[1] = (address >> 16) & 0xFF;       // Address High Byte
    spi_data[2] = (address >> 8)  & 0xFF;       // Address Middle Byte
    spi_data[3] = (address & 0xFF);             // Address Low Byte

    // 2. Pull CS Low to select the chip
    W25_CS_LOW();

    // 3. Send the Command and Address (4 bytes total)
    HAL_SPI_Transmit(&hspi1, spi_data, 4, 100);

    // 4. Read the requested data directly into the buffer
    HAL_SPI_Receive(&hspi1, buffer, length, 1000);

    // 5. Pull CS High to put the chip back to sleep
    W25_CS_HIGH();
}

// Checks the BUSY bit in the status register and waits until it clears
void W25_Wait_For_Ready(void)
{
    SPI_Set_Mode_Flash(); // Ensure SPI is in Mode 0 for the flash chip
    uint8_t cmd = W25_CMD_READ_STATUS;
    uint8_t status = 0;
    
    W25_CS_LOW();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 100);
    do {
        // Keep reading the status register until bit 0 (BUSY) becomes 0
        HAL_SPI_Receive(&hspi1, &status, 1, 100);
    } while ((status & 0x01) == 0x01); 
    W25_CS_HIGH();
}

// Unlocks the memory to allow a write or erase
void W25_Write_Enable(void)
{
    SPI_Set_Mode_Flash(); // Ensure SPI is in Mode 0 for the flash chip
    uint8_t cmd = W25_CMD_WRITE_ENABLE;
    W25_CS_LOW();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 100);
    W25_CS_HIGH();
}

// Wipes a 4096-byte chunk of memory clean to 0xFF
void W25_Erase_Sector(uint32_t address)
{
    SPI_Set_Mode_Flash(); // Ensure SPI is in Mode 0 for the flash chip
    uint8_t spi_data[4];
    
    W25_Write_Enable(); // MUST unlock before erasing
    
    spi_data[0] = W25_CMD_SECTOR_ERASE;
    spi_data[1] = (address >> 16) & 0xFF;
    spi_data[2] = (address >> 8)  & 0xFF;
    spi_data[3] = (address & 0xFF);
    
    W25_CS_LOW();
    HAL_SPI_Transmit(&hspi1, spi_data, 4, 100);
    W25_CS_HIGH();
    
    W25_Wait_For_Ready(); // Wait for the physical erase to finish
}

// Writes up to 256 bytes of data to an ERASED sector
void W25_Write_Page(uint32_t address, uint8_t* buffer, uint16_t length)
{
    SPI_Set_Mode_Flash(); // Ensure SPI is in Mode 0 for the flash chip
    uint8_t spi_data[4];
    
    W25_Write_Enable(); // MUST unlock before writing
    
    spi_data[0] = W25_CMD_PAGE_PROGRAM;
    spi_data[1] = (address >> 16) & 0xFF;
    spi_data[2] = (address >> 8)  & 0xFF;
    spi_data[3] = (address & 0xFF);
    
    W25_CS_LOW();
    HAL_SPI_Transmit(&hspi1, spi_data, 4, 100);       // Send Address
    HAL_SPI_Transmit(&hspi1, buffer, length, 1000);   // Send Data Array
    W25_CS_HIGH();
    
    W25_Wait_For_Ready(); // Wait for the physical write to finish
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
  MX_I2C1_Init();
  MX_I2C2_Init();
  MX_RTC_Init();
  MX_SPI1_Init();
  MX_FATFS_Init();
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */
  HAL_Delay(100); 
  
  // Ask the chip who it is!
  W25_Read_ID();

  MAX31865_Init(); // Initialize the MAX31865 for PT100 readings
  LIS3DHTR_Init(); // Initialize the LIS3DHTR for accelerometer readings
  // 1. Try to Mount the File System
  /*FRESULT res = f_mount(&fs, USERPath, 1);
  
  // --- NEW AUTO-FORMAT CATCH BLOCK ---
  if (res == FR_NO_FILESYSTEM) 
  {
      // 1. Give FatFs a massive 4KB buffer to ensure it has plenty of working room!
      BYTE workBuffer[4096]; 
      
      // 2. Use '1' (FM_FAT) to explicitly command a standard FAT format
      res = f_mkfs(USERPath, 1, 0, workBuffer, sizeof(workBuffer));
      
      // If the format was successful, try mounting it one more time!
      if (res == FR_OK) 
      {
          res = f_mount(&fs, USERPath, 1);
      }
  }
  // -----------------------------------

  // Update our check variable to see the final result
  check = (float)res;

  // 2. If it mounted successfully (either immediately or after formatting)
  if(res == FR_OK) 
  {
      HAL_Delay(500); 
      
      if(f_open(&fil, "data.csv", FA_OPEN_ALWAYS | FA_WRITE) == FR_OK)
      {
          f_lseek(&fil, f_size(&fil));
          char header[] = "Date,Time,Temperature(C),Humidity(%),Accel_X,Accel_Y,Accel_Z\n";
          f_write(&fil, header, strlen(header), &bytesWritten);
          f_close(&fil);
          
          check = 99.0f; // 99 means ultimate success!
      }
  }*/
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    MAX31865_ReadRTD(); // Read the RTD and update the live_temperature variable
    SHT45_Read(); // Read the SHT45 and update the temperature and humidity variables
    LIS3DHTR_Read(&LIS3DH_X, &LIS3DH_Y, &LIS3DH_Z);
    HAL_Delay(500); // Delay for 0.5 second before the next reading


    /*// 2. Read the RTC Time and Date (MUST read Time first!)
      RTC_TimeTypeDef sTime = {0};
      RTC_DateTypeDef sDate = {0};
      HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
      HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

      // 3. Format all data into a single CSV string
      // snprintf safely converts our floats and integers into text
      snprintf(log_buffer, sizeof(log_buffer), "20%02d-%02d-%02d,%02d:%02d:%02d,%.2f,%.2f,%d,%d,%d\n",
               sDate.Year, sDate.Month, sDate.Date,
               sTime.Hours, sTime.Minutes, sTime.Seconds,
               live_temperature, humidity, LIS3DH_X, LIS3DH_Y, LIS3DH_Z);

      // 4. Open the file, move to the bottom, write the row, and close
      if(f_open(&fil, "data.csv", FA_OPEN_ALWAYS | FA_WRITE) == FR_OK)
      {
          f_lseek(&fil, f_size(&fil)); // Go to the very end of the file
          f_write(&fil, log_buffer, strlen(log_buffer), &bytesWritten);
          f_close(&fil);
      }*/
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

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSI
                              |RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
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
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x10B17DB5;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  hrtc.Init.OutPutPullUp = RTC_OUTPUT_PULLUP_NONE;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) != 0x32F2)
  {
      /** Initialize RTC and set the Time and Date
      */
      sTime.Hours = 0;      // 12 AM (Midnight) in 24-hour time
      sTime.Minutes = 8;    // Current minute
      sTime.Seconds = 0;
      sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
      sTime.StoreOperation = RTC_STOREOPERATION_RESET;
      
      // Note: We changed FORMAT_BCD to FORMAT_BIN so normal decimal numbers work
      if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
      {
        Error_Handler();
      }
      
      sDate.WeekDay = RTC_WEEKDAY_SATURDAY;
      sDate.Month = RTC_MONTH_JULY;
      sDate.Date = 11;      // 11th
      sDate.Year = 26;      // 2026

      if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
      {
        Error_Handler();
      }

      // Save our secret flag to Backup Register 1 so it NEVER resets on reboot!
      HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, 0x32F2);
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

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
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, Storage_CS_Pin|MAX_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : Storage_CS_Pin MAX_CS_Pin */
  GPIO_InitStruct.Pin = Storage_CS_Pin|MAX_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : Temp_int_Pin IMU_int_Pin Light_int_Pin */
  GPIO_InitStruct.Pin = Temp_int_Pin|IMU_int_Pin|Light_int_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

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
