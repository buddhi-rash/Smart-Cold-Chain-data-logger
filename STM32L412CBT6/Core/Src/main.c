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
#include <stdio.h>
#include "ssd1306.h"
#include "ssd1306_fonts.h"
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
I2C_HandleTypeDef hi2c2;

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


// ==========================================
// 1. YOUR GLOBAL VARIABLES (The System Data)
// ==========================================
int target_temperature = 25;
int motor_speed = 100;
int alarm_enable = 1;
// Home screen data
int battery_percent = 100;           // Battery percentage (0-100)
char time_str[9] = "00:00:00";     // HH:MM:SS string to display


// ==========================================
// 2. UI TYPES & DATA STRUCTURES
// ==========================================
typedef enum {
    UI_STATE_HOME,    // Main dashboard display
    UI_STATE_MENU,    // Scrolling through settings list
    UI_STATE_EDIT     // Tweaking a specific setting
} UI_State_t;

typedef struct {
    const char* label;   // Text to display on OLED
    int* target_var;     // Pointer to the actual global variable
    int min_val;         // Lower limit
    int max_val;         // Upper limit
    int step;            // Amount to change per click
} MenuItem_t;

// Global UI engine variables
UI_State_t current_state = UI_STATE_HOME;
int current_menu_index = 0;
uint8_t ui_needs_update = 1; // 1 = Redraw screen immediately

// Display timeout state
#define DISPLAY_TIMEOUT_MS 60000U
uint8_t display_enabled = 1;
uint32_t last_user_activity_ms = 0;

// Sensor reading timing (read every 500ms)
#define SENSOR_READ_INTERVAL_MS 500U
static uint32_t last_sensor_read_ms = 0;

// Button debouncing
#define DEBOUNCE_TIME_MS 20U  // 20ms debounce time

// ==========================================
// 3. MENU REGISTRATION
// ==========================================
MenuItem_t menu_items[] = {
    {"Target Temp",  &target_temperature, 10,  40,  1}, // Min 10, Max 40, Step 1
    {"Motor Speed",  &motor_speed,        0, 255,  5}, // Min 0, Max 255, Step 5
    {"Alarm Switch", &alarm_enable,       0,   1,  1}  // 0 = Off, 1 = On
};

const int TOTAL_MENU_ITEMS = sizeof(menu_items) / sizeof(MenuItem_t);
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C2_Init(void);
/* USER CODE BEGIN PFP */
void LIS3DHTR_Init(void);
void Read_SHT45(void);
void Read_LIS3DHTR(void);
void Read_OPT3001(void);
uint8_t Process_UI(void);
void Display_HomeScreen(void);
void Display_MenuScreen(void);
void Display_EditScreen(void);
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
  MX_I2C2_Init();
  /* USER CODE BEGIN 2 */
  LIS3DHTR_Init();
  
  // --- I2C2 device scan for OLED ---
  for (uint8_t addr = 0x30; addr <= 0x3F; ++addr) {
    if (HAL_I2C_IsDeviceReady(&hi2c2, (uint16_t)(addr << 1), 3, 100) == HAL_OK) {
      break;
    }
  }
  
  // Initialize OLED display
  ssd1306_Init();
  ssd1306_SetDisplayOn(1);
  ssd1306_Fill(Black);
  ssd1306_SetCursor(0, 20);
  ssd1306_WriteString((char*)"Display Ready!", Font_11x18, White);
  ssd1306_UpdateScreen();
  HAL_Delay(1000);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint32_t current_time_ms = HAL_GetTick();
    
    // Read sensors at regular intervals (every 500ms)
    if ((current_time_ms - last_sensor_read_ms) >= SENSOR_READ_INTERVAL_MS) {
      Read_SHT45();
      Read_LIS3DHTR();
      Read_OPT3001();
      last_sensor_read_ms = current_time_ms;
    }

    // Check buttons frequently for responsive input
    uint8_t user_activity = Process_UI();
    if (user_activity) {
      if (!display_enabled) {
        ssd1306_SetDisplayOn(1);
        display_enabled = 1;
      }
      last_user_activity_ms = HAL_GetTick();
      ui_needs_update = 1;  // Request screen redraw on user activity
    }

    if (display_enabled) {
      if (ui_needs_update) {
        // Render the appropriate screen based on current state
        if (current_state == UI_STATE_HOME) {
          Display_HomeScreen();
        } else if (current_state == UI_STATE_MENU) {
          Display_MenuScreen();
        } else if (current_state == UI_STATE_EDIT) {
          Display_EditScreen();
        }
        ui_needs_update = 0;
      }
      
      if ((HAL_GetTick() - last_user_activity_ms) >= DISPLAY_TIMEOUT_MS) {
        ssd1306_SetDisplayOn(0);
        display_enabled = 0;
      }
    }

    // Short delay for CPU efficiency, but still allows responsive button checking
    HAL_Delay(10);  // Check buttons every 10ms for responsive input
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
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pins : User_up_Pin User_down_Pin Menu_Pin */
  GPIO_InitStruct.Pin = User_up_Pin|User_down_Pin|Menu_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

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


uint8_t Process_UI(void) {
    // Static edge flags to remember previous button states
    static uint8_t menu_was_pressed = 0;
    static uint8_t up_was_pressed = 0;
    static uint8_t down_was_pressed = 0;

    uint8_t changed = 0;

    // Read physical hardware pins from correct ports
    uint8_t menu_pressed = (HAL_GPIO_ReadPin(Menu_GPIO_Port, Menu_Pin) == GPIO_PIN_RESET);
    uint8_t up_pressed   = (HAL_GPIO_ReadPin(User_up_GPIO_Port, User_up_Pin) == GPIO_PIN_RESET);
    uint8_t down_pressed = (HAL_GPIO_ReadPin(User_down_GPIO_Port, User_down_Pin) == GPIO_PIN_RESET);

    // ------------------------------------------
    // MENU BUTTON LOGIC (State Transitions)
    // ------------------------------------------
    if (menu_pressed && !menu_was_pressed) { // Rising edge (Just pressed)
        menu_was_pressed = 1;
        changed = 1;

        if (current_state == UI_STATE_HOME) {
            // Enter menu from home
            current_state = UI_STATE_MENU;
            current_menu_index = 0; // Reset to first item
        } else if (current_state == UI_STATE_MENU) {
            // Enter edit mode for selected item
            current_state = UI_STATE_EDIT;
        } else if (current_state == UI_STATE_EDIT) {
            // Exit edit mode back to menu
            current_state = UI_STATE_MENU;
        }
    } else if (!menu_pressed) {
        menu_was_pressed = 0; // Reset flag when released
    }

    // ------------------------------------------
    // UP BUTTON LOGIC (Also acts as Back/Home in MENU state)
    // ------------------------------------------
    if (up_pressed && !up_was_pressed) {
        up_was_pressed = 1;
        changed = 1;

        if (current_state == UI_STATE_MENU) {
            // Scroll selection up
            current_menu_index--;
            if (current_menu_index < 0) {
                current_menu_index = TOTAL_MENU_ITEMS - 1; // Wrap around to bottom
            }
        } 
        else if (current_state == UI_STATE_EDIT) {
            // Modify the global variable via its pointer
            MenuItem_t* active_item = &menu_items[current_menu_index];
            *(active_item->target_var) += active_item->step;
            
            // Boundary enforcement
            if (*(active_item->target_var) > active_item->max_val) {
                *(active_item->target_var) = active_item->max_val;
            }
        }
        else if (current_state == UI_STATE_HOME) {
            // From home, Up button can also open menu
            current_state = UI_STATE_MENU;
            current_menu_index = 0;
        }
    } else if (!up_pressed) {
        up_was_pressed = 0;
    }

    // ------------------------------------------
    // DOWN BUTTON LOGIC (Navigate menu items or go back to Home)
    // ------------------------------------------
    if (down_pressed && !down_was_pressed) {
        down_was_pressed = 1;
        changed = 1;

        if (current_state == UI_STATE_MENU) {
            // Scroll selection down
            current_menu_index++;
            if (current_menu_index >= TOTAL_MENU_ITEMS) {
                // When reaching end, go back to HOME
                current_state = UI_STATE_HOME;
                current_menu_index = 0; // Reset for next time
            }
        } 
        else if (current_state == UI_STATE_EDIT) {
            // Modify the global variable via its pointer
            MenuItem_t* active_item = &menu_items[current_menu_index];
            *(active_item->target_var) -= active_item->step;
            
            // Boundary enforcement
            if (*(active_item->target_var) < active_item->min_val) {
                *(active_item->target_var) = active_item->min_val;
            }
        }
        else if (current_state == UI_STATE_HOME) {
            // From home, Down button can also open menu
            current_state = UI_STATE_MENU;
            current_menu_index = 0;
        }
    } else if (!down_pressed) {
        down_was_pressed = 0;
    }

    return changed;
}

/* USER CODE BEGIN 4 - Home screen renderer */
void Display_HomeScreen(void)
{
  char buf[32];
  ssd1306_Fill(Black);

  // Top-left: label
  ssd1306_SetCursor(2, 0);
  ssd1306_WriteString((char*)"Temp", Font_6x8, White);

  // Large temperature value
  snprintf(buf, sizeof(buf), "%.1fC", SHT45_Temperature);
  ssd1306_SetCursor(2, 10);
  ssd1306_WriteString(buf, Font_16x24, White);

  // Humidity at top-right
  snprintf(buf, sizeof(buf), "Hum: %.0f%%", SHT45_Humidity);
  ssd1306_SetCursor(64, 10);
  ssd1306_WriteString(buf, Font_11x18, White);

  // Battery percentage
  snprintf(buf, sizeof(buf), "Bat: %d%%", battery_percent);
  ssd1306_SetCursor(64, 36);
  ssd1306_WriteString(buf, Font_7x10, White);

  // Time at bottom-left
  ssd1306_SetCursor(2, 50);
  ssd1306_WriteString(time_str, Font_7x10, White);

  ssd1306_UpdateScreen();
}

/**
  * @brief Display the Menu screen showing all available settings
  */
void Display_MenuScreen(void)
{
  char buf[32];
  ssd1306_Fill(Black);

  // Title
  ssd1306_SetCursor(2, 0);
  ssd1306_WriteString((char*)"SETTINGS", Font_7x10, White);

  // Draw a line separator
  ssd1306_Line(0, 12, 127, 12, White);

  // Display all menu items, highlighting the selected one
  int y_pos = 18;
  for (int i = 0; i < TOTAL_MENU_ITEMS; i++) {
    // Highlight selected item with inverse colors
    if (i == current_menu_index) {
      // Draw selection box (inverted)
      ssd1306_DrawRectangle(0, y_pos - 2, 127, y_pos + 10, White);
      ssd1306_SetCursor(4, y_pos);
      snprintf(buf, sizeof(buf), "%s: %d", menu_items[i].label, *(menu_items[i].target_var));
      ssd1306_WriteString(buf, Font_6x8, Black);  // Black text on white background
    } else {
      // Normal item
      ssd1306_SetCursor(4, y_pos);
      snprintf(buf, sizeof(buf), "%s: %d", menu_items[i].label, *(menu_items[i].target_var));
      ssd1306_WriteString(buf, Font_6x8, White);
    }
    y_pos += 14;
  }

  // Bottom navigation hints
  ssd1306_SetCursor(2, 54);
  ssd1306_WriteString((char*)"Menu:Select  Dn:Back to Home", Font_6x8, White);

  ssd1306_UpdateScreen();
}

/**
  * @brief Display the Edit screen for modifying a single setting
  */
void Display_EditScreen(void)
{
  char buf[32];
  ssd1306_Fill(Black);

  MenuItem_t* active_item = &menu_items[current_menu_index];

  // Title: Show which item we're editing
  ssd1306_SetCursor(2, 0);
  ssd1306_WriteString((char*)"EDIT MODE", Font_7x10, White);

  // Separator line
  ssd1306_Line(0, 12, 127, 12, White);

  // Item name
  ssd1306_SetCursor(2, 18);
  ssd1306_WriteString((char*)active_item->label, Font_11x18, White);

  // Current value - large display
  snprintf(buf, sizeof(buf), "%d", *(active_item->target_var));
  ssd1306_SetCursor(2, 40);
  ssd1306_WriteString(buf, Font_16x24, White);

  // Range info and hints at bottom
  snprintf(buf, sizeof(buf), "Range: %d-%d  Menu:Back", active_item->min_val, active_item->max_val);
  ssd1306_SetCursor(2, 54);
  ssd1306_WriteString(buf, Font_6x8, White);

  ssd1306_UpdateScreen();
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
