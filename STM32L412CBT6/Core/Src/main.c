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
#include "stm32l4xx_hal.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "Sensors.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <stdio.h>
#include <string.h>
#include "usbd_core.h"
#include "GSM.h"
extern USBD_HandleTypeDef hUsbDeviceFS;
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

#define MAX_BUFFERED_READINGS 100 // Adjust based on your available RAM
typedef struct {
    uint8_t year;
    uint8_t month;
    uint8_t date;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    float temperature;
    float humidity;
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    uint8_t shock_event; // NEW FIELD
} SensorRecord_t;
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

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
// Array to store the 3 bytes the chip sends back
uint8_t flash_id[3];
uint8_t test_read_buffer[10]; // Buffer to hold our read test
float check = 0.0f; // Variable to hold the floating-point value to be written to the CSV file
float debug=0.0f; 
float f_c= 0.0f;
uint32_t dif_time=0; 

FATFS fs;           // The File System Object
FIL fil;            // The File Object
UINT bytesWritten;  // Counter for bytes written

char log_buffer[128];

SensorRecord_t local_record_buffer[MAX_BUFFERED_READINGS];
uint32_t local_record_count = 0; // Tracks how many readings we have stored
volatile uint8_t shock_event_data = 0;

//Display variables
// ==========================================
// UI GLOBAL VARIABLES
// ==========================================
int Logging_interval = 30;// Logging interval in seconds (10-40)
int Uploading_Interval = 120;// Uploading interval in seconds (0-255)
int alarm_enable = 1;

int battery_percent = 73;           // Battery percentage (0-100)
char time_str[16] = "00:00:00";     // String to hold RTC time for OLED

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

UI_State_t current_state = UI_STATE_HOME;
int current_menu_index = 0;
uint8_t ui_needs_update = 1; 

#define DISPLAY_TIMEOUT_MS 60000U
uint8_t display_enabled = 1;
uint32_t last_user_activity_ms = 0;
uint32_t last_sensor_read_ms = 0; // Timer for our non-blocking 500ms loop
uint32_t last_upload_ms = 0;

MenuItem_t menu_items[] = {
    // Label                 Pointer               Min   Max    Step
    {"Logging Interval",    &Logging_interval,     10,   180,   10},  // Range: 10s to 120s (Step by 10s)
    {"Uploading Interval",  &Uploading_Interval,   60,   3600,  60},  // Range: 60s to 3600s (Step by 1 minute)
    {"Alarm Switch",        &alarm_enable,         0,    1,     1}  
};
const int TOTAL_MENU_ITEMS = sizeof(menu_items) / sizeof(MenuItem_t);
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C2_Init(void);
static void MX_RTC_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
void SPI_Set_Mode_Flash(void);
void SPI_Set_Mode_MAX31865(void);
void W25_Read_ID(void);
void W25_Read_Data(uint32_t address, uint8_t* buffer, uint32_t length);
void W25_Wait_For_Ready(void);
void W25_Write_Enable(void);
void W25_Erase_Sector(uint32_t address);
void W25_Write_Page(uint32_t address, uint8_t* buffer, uint16_t length);
void Update_Dashboard(void);
uint8_t Process_UI(void);
void Display_HomeScreen(void);
void Display_MenuScreen(void);
void Display_EditScreen(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Shifts the SPI bus into Mode 0 for the Memory Chip
void SPI_Set_Mode_Flash(void)
{
// 1. Force the PT100 CS HIGH so it ignores the bus change
  HAL_GPIO_WritePin(MAX_CS_GPIO_Port, MAX_CS_Pin, GPIO_PIN_SET);
    
  // 2. Safely disable the SPI hardware block without breaking GPIOs
  __HAL_SPI_DISABLE(&hspi1);
    
  // 3. Fully restore ALL SPI parameters for Mode 0
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;         // Mode 0
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;             // Mode 0
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16; // Overwrite FatFs changes
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;

  HAL_SPI_Init(&hspi1);
}

// Shifts the SPI bus into Mode 3 for the PT100 Sensor
void SPI_Set_Mode_MAX31865(void)
{
// 1. Force the Flash CS HIGH so it ignores the bus change
  HAL_GPIO_WritePin(Storage_CS_GPIO_Port, Storage_CS_Pin, GPIO_PIN_SET);
    
  // 2. Safely disable the SPI hardware block without breaking GPIOs
  __HAL_SPI_DISABLE(&hspi1);
    
  // 3. Fully restore ALL SPI parameters for Mode 3
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;        // Mode 3
  hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;             // Mode 3
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16; // Guarantee safe clock speed!
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;

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

uint8_t Process_UI(void) {
    static uint8_t menu_was_pressed = 0;
    static uint8_t up_was_pressed = 0;
    static uint8_t down_was_pressed = 0;
    uint8_t changed = 0;

    uint8_t menu_pressed = (HAL_GPIO_ReadPin(Menu_GPIO_Port, Menu_Pin) == GPIO_PIN_RESET);
    uint8_t up_pressed   = (HAL_GPIO_ReadPin(User_DOWN_GPIO_Port, User_DOWN_Pin) == GPIO_PIN_RESET);
    uint8_t down_pressed = (HAL_GPIO_ReadPin(User_UP_GPIO_Port, User_UP_Pin) == GPIO_PIN_RESET);

    // MENU BUTTON LOGIC
    if (menu_pressed && !menu_was_pressed) { 
        menu_was_pressed = 1;
        changed = 1;
        if (current_state == UI_STATE_HOME) {
            current_state = UI_STATE_MENU;
            current_menu_index = 0; 
        } else if (current_state == UI_STATE_MENU) {
            current_state = UI_STATE_EDIT;
        } else if (current_state == UI_STATE_EDIT) {
            current_state = UI_STATE_MENU;
        }
    } else if (!menu_pressed) menu_was_pressed = 0; 

    // UP BUTTON LOGIC
    if (up_pressed && !up_was_pressed) {
        up_was_pressed = 1;
        changed = 1;
        if (current_state == UI_STATE_MENU) {
            current_menu_index--;
            if (current_menu_index < 0) current_menu_index = TOTAL_MENU_ITEMS - 1; 
        } else if (current_state == UI_STATE_EDIT) {
            MenuItem_t* active_item = &menu_items[current_menu_index];
            *(active_item->target_var) += active_item->step;
            if (*(active_item->target_var) > active_item->max_val) *(active_item->target_var) = active_item->max_val;
        } else if (current_state == UI_STATE_HOME) {
            current_state = UI_STATE_MENU;
            current_menu_index = 0;
        }
    } else if (!up_pressed) up_was_pressed = 0;

    // DOWN BUTTON LOGIC
    if (down_pressed && !down_was_pressed) {
        down_was_pressed = 1;
        changed = 1;
        if (current_state == UI_STATE_MENU) {
            current_menu_index++;
            if (current_menu_index >= TOTAL_MENU_ITEMS) {
                current_state = UI_STATE_HOME;
                current_menu_index = 0; 
            }
        } else if (current_state == UI_STATE_EDIT) {
            MenuItem_t* active_item = &menu_items[current_menu_index];
            *(active_item->target_var) -= active_item->step;
            if (*(active_item->target_var) < active_item->min_val) *(active_item->target_var) = active_item->min_val;
        } else if (current_state == UI_STATE_HOME) {
            current_state = UI_STATE_MENU;
            current_menu_index = 0;
        }
    } else if (!down_pressed) down_was_pressed = 0;

    return changed;
}

void Display_HomeScreen(void)
{
  char buf[32];
  ssd1306_Fill(Black);

  ssd1306_SetCursor(2, 0);
  ssd1306_WriteString((char*)"Temp", Font_6x8, White);

  // UPDATED TO USE YOUR PT100 VARIABLE
  snprintf(buf, sizeof(buf), "%.1fC", live_temperature);
  ssd1306_SetCursor(2, 10);
  ssd1306_WriteString(buf, Font_16x24, White);

  // UPDATED TO USE YOUR SHT45 VARIABLE
  snprintf(buf, sizeof(buf), "Hum: %.0f%%", humidity);
  ssd1306_SetCursor(64, 10);
  ssd1306_WriteString(buf, Font_7x10, White);

  snprintf(buf, sizeof(buf), "Bat: %d%%", battery_percent);
  ssd1306_SetCursor(64, 36);
  ssd1306_WriteString(buf, Font_7x10, White);

  ssd1306_SetCursor(2, 50);
  ssd1306_WriteString(time_str, Font_7x10, White);

  ssd1306_UpdateScreen();
}

void Display_MenuScreen(void)
{
  char buf[32];
  ssd1306_Fill(Black);
  ssd1306_SetCursor(2, 0);
  ssd1306_WriteString((char*)"SETTINGS", Font_7x10, White);
  ssd1306_Line(0, 12, 127, 12, White);

  int y_pos = 18;
  for (int i = 0; i < TOTAL_MENU_ITEMS; i++) {
    if (i == current_menu_index) {
      ssd1306_DrawRectangle(0, y_pos - 2, 127, y_pos + 10, White);
      ssd1306_SetCursor(4, y_pos);
      snprintf(buf, sizeof(buf), "%s: %d", menu_items[i].label, *(menu_items[i].target_var));
      ssd1306_WriteString(buf, Font_6x8, Black); 
    } else {
      ssd1306_SetCursor(4, y_pos);
      snprintf(buf, sizeof(buf), "%s: %d", menu_items[i].label, *(menu_items[i].target_var));
      ssd1306_WriteString(buf, Font_6x8, White);
    }
    y_pos += 14;
  }
  ssd1306_SetCursor(2, 54);
  ssd1306_WriteString((char*)"Menu:Select  Dn:Back", Font_6x8, White);
  ssd1306_UpdateScreen();
}

void Display_EditScreen(void)
{
  char buf[32];
  ssd1306_Fill(Black);
  MenuItem_t* active_item = &menu_items[current_menu_index];

  ssd1306_SetCursor(2, 0);
  ssd1306_WriteString((char*)"EDIT MODE", Font_7x10, White);
  ssd1306_Line(0, 12, 127, 12, White);

  ssd1306_SetCursor(2, 18);
  ssd1306_WriteString((char*)active_item->label, Font_11x18, White);

  snprintf(buf, sizeof(buf), "%d", *(active_item->target_var));
  ssd1306_SetCursor(2, 40);
  ssd1306_WriteString(buf, Font_16x24, White);

  snprintf(buf, sizeof(buf), "Range: %d-%d  Menu:Back", active_item->min_val, active_item->max_val);
  ssd1306_SetCursor(2, 54);
  ssd1306_WriteString(buf, Font_6x8, White);
  ssd1306_UpdateScreen();
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
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_Delay(100); 

  // Replace &huart1 with your configured cellular UART handle
  GSM_Init(&huart1);
  
  // Ask the chip who it is!
  W25_Read_ID();

  ssd1306_Init();
  // Show a quick boot screen
  ssd1306_Fill(Black);
  ssd1306_SetCursor(10, 20);
  ssd1306_WriteString("Cold Chain", Font_11x18, White);
  ssd1306_SetCursor(25, 40);
  ssd1306_WriteString("Logger V1", Font_7x10, White);
  ssd1306_UpdateScreen();


  HAL_Delay(2000); // Show the boot screen for 2 seconds
  ssd1306_Fill(Black);

  //USBD_DeInit(&hUsbDeviceFS);

  MAX31865_Init(); // Initialize the MAX31865 for PT100 readings

  LIS3DHTR_Init(); // Initialize the LIS3DHTR for accelerometer readings

  
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint8_t was_usb_connected = 0;
  while (1)
  {
    uint32_t current_time_ms = HAL_GetTick();
      uint8_t is_usb_connected = (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED) ? 1 : 0;

      // =========================================================
      // TRANSITION LOGIC (Cable plugged in)
      // =========================================================
      if (is_usb_connected == 1 && was_usb_connected == 0) 
      {
          // 1. Alert the User
          ssd1306_Fill(Black);
          ssd1306_SetCursor(10, 20);
          ssd1306_WriteString("USB CONNECTED", Font_11x18, White);
          ssd1306_SetCursor(10, 40);
          ssd1306_WriteString("Syncing Data...", Font_7x10, White);
          ssd1306_UpdateScreen();
          
          // 2. Quickly mount FatFs and dump our RAM buffer into the CSV file
          /*if (f_mount(&fs, USERPath, 1) == FR_OK) 
          {
              if (f_open(&fil, "maindata.csv", FA_OPEN_ALWAYS | FA_WRITE) == FR_OK) 
              {
                  // ==========================================
                  // NEW: Check if the file is brand new (size 0)
                  // ==========================================
                  if (f_size(&fil) == 0) 
                  {
                      // File was just created, write the header!
                      f_c=0.2f;
                      char header[] = "Date,Time,Temperature(C),Humidity(%),Accel_X,Accel_Y,Accel_Z\n";
                      f_write(&fil, header, strlen(header), &bytesWritten);
                  }
                  else
                  {
                      // File already exists, just jump to the end
                      f_c=0.1f;
                      f_lseek(&fil, f_size(&fil)); 
                  }

                  // Loop through all saved readings and write them
                  for (uint32_t i = 0; i < local_record_count; i++) 
                  {
                      snprintf(log_buffer, sizeof(log_buffer), "20%02d-%02d-%02d,%02d:%02d:%02d,%.2f,%.2f,%d,%d,%d\n",
                               local_record_buffer[i].year, local_record_buffer[i].month, local_record_buffer[i].date,
                               local_record_buffer[i].hours, local_record_buffer[i].minutes, local_record_buffer[i].seconds,
                               local_record_buffer[i].temperature, local_record_buffer[i].humidity, 
                               local_record_buffer[i].accel_x, local_record_buffer[i].accel_y, local_record_buffer[i].accel_z);
                      
                      f_write(&fil, log_buffer, strlen(log_buffer), &bytesWritten);
                  }
                  f_close(&fil);
              }
          }*/

          // 3. Reset the buffer counter since data is now saved
          local_record_count = 0;
          
          // 4. Unmount FatFs safely so the PC can take control of the Flash memory
          f_mount(NULL, USERPath, 0); 
          was_usb_connected = 1;
      }
      // =========================================================
      // TRANSITION LOGIC (Cable pulled out)
      // =========================================================
      else if (is_usb_connected == 0 && was_usb_connected == 1) 
      {
          // Note: We DO NOT mount the file system here. We keep it unmounted 
          // during battery operation to save power and prevent SPI conflicts.
          ui_needs_update = 1;       
          was_usb_connected = 0;
      }

      // =========================================================
      // NORMAL OPERATION (Only runs when running on Battery)
      // =========================================================
      if (is_usb_connected == 0) 
      {
          debug = 2.0f;
          uint8_t user_activity = Process_UI();
          if (user_activity) {
              if (!display_enabled) {
                  ssd1306_SetDisplayOn(1);
                  display_enabled = 1;
              }
              last_user_activity_ms = current_time_ms;
              ui_needs_update = 1;  
          }

          dif_time = current_time_ms - last_sensor_read_ms;
          if ((current_time_ms - last_sensor_read_ms) >= (Logging_interval * 1000)) 
          {
              last_sensor_read_ms = current_time_ms;
              debug = 1.0f;
              
              MAX31865_ReadRTD(); 
              SHT45_Read(); 
              LIS3DHTR_Read(&LIS3DH_X, &LIS3DH_Y, &LIS3DH_Z);
              
              RTC_TimeTypeDef sTime = {0};
              RTC_DateTypeDef sDate = {0};
              HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
              HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
              
              snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", sTime.Hours, sTime.Minutes, sTime.Seconds);

              // =========================================================
              // NEW: STORE IN RAM BUFFER INSTEAD OF CSV
              // =========================================================
              if (local_record_count < MAX_BUFFERED_READINGS) 
              {
                  local_record_buffer[local_record_count].year = sDate.Year;
                  local_record_buffer[local_record_count].month = sDate.Month;
                  local_record_buffer[local_record_count].date = sDate.Date;
                  local_record_buffer[local_record_count].hours = sTime.Hours;
                  local_record_buffer[local_record_count].minutes = sTime.Minutes;
                  local_record_buffer[local_record_count].seconds = sTime.Seconds;
                  local_record_buffer[local_record_count].temperature = live_temperature;
                  local_record_buffer[local_record_count].humidity = humidity;
                  local_record_buffer[local_record_count].accel_x = LIS3DH_X;
                  local_record_buffer[local_record_count].accel_y = LIS3DH_Y;
                  local_record_buffer[local_record_count].accel_z = LIS3DH_Z;
                  
                  local_record_count++;
              }

              /*if (f_mount(&fs, USERPath, 1) == FR_OK) 
              {
                if (f_open(&fil, "maindata.csv", FA_OPEN_ALWAYS | FA_WRITE) == FR_OK) 
                {
                    // ==========================================
                    // NEW: Check if the file is brand new (size 0)
                    // ==========================================
                    if (f_size(&fil) == 0) 
                    {
                        // File was just created, write the header!
                        f_c=0.2f;
                        char header[] = "Date,Time,Temperature(C),Humidity(%),Accel_X,Accel_Y,Accel_Z\n";
                        f_write(&fil, header, strlen(header), &bytesWritten);
                    }
                    else
                    {
                        // File already exists, just jump to the end
                        f_c=0.1f;
                        f_lseek(&fil, f_size(&fil)); 
                    }

                    // Loop through all saved readings and write them
                    for (uint32_t i = 0; i < local_record_count; i++) 
                    {
                        snprintf(log_buffer, sizeof(log_buffer), "20%02d-%02d-%02d,%02d:%02d:%02d,%.2f,%.2f,%d,%d,%d\n",
                                local_record_buffer[i].year, local_record_buffer[i].month, local_record_buffer[i].date,
                                local_record_buffer[i].hours, local_record_buffer[i].minutes, local_record_buffer[i].seconds,
                                local_record_buffer[i].temperature, local_record_buffer[i].humidity, 
                                local_record_buffer[i].accel_x, local_record_buffer[i].accel_y, local_record_buffer[i].accel_z);
                      
                        f_write(&fil, log_buffer, strlen(log_buffer), &bytesWritten);
                    }
                    f_close(&fil);
                }
            }*/

              // 3. Reset the buffer counter since data is now saved
              //local_record_count = 0;
          
              // 4. Unmount FatFs safely so the PC can take control of the Flash memory
              //f_mount(NULL, USERPath, 0); 
              
              char date_buffer[16];
              char time_buffer[16];
              snprintf(date_buffer, sizeof(date_buffer), "%d/%d/20%02d", sDate.Month, sDate.Date, sDate.Year);
              snprintf(time_buffer, sizeof(time_buffer), "%02d:%02d:%02d", sTime.Hours, sTime.Minutes, sTime.Seconds);
              GSM_AddReading(date_buffer, time_buffer, live_temperature, humidity, LIS3DH_X, LIS3DH_Y, LIS3DH_Z, shock_event_data);
              shock_event_data=0;
              if (((current_time_ms - last_upload_ms) >= (Uploading_Interval * 1000)) || 
                  (GSM_GetBufferCount() >= MAX_READINGS)) 
              {
                  last_upload_ms = current_time_ms; 
                  if (GSM_GetBufferCount() > 0) {
                      ssd1306_SetCursor(2, 50);
                      ssd1306_WriteString("Uploading...   ", Font_7x10, White);
                      ssd1306_UpdateScreen();
                      GSM_UploadBuffer();
                  }
              }

              if (current_state == UI_STATE_HOME) ui_needs_update = 1; 
          }

          if (display_enabled && ui_needs_update) 
          {
              if (current_state == UI_STATE_HOME) Display_HomeScreen();
              else if (current_state == UI_STATE_MENU) Display_MenuScreen();
              else if (current_state == UI_STATE_EDIT) Display_EditScreen();
              ui_needs_update = 0;
          }

          if (display_enabled && ((current_time_ms - last_user_activity_ms) >= DISPLAY_TIMEOUT_MS)) {
              ssd1306_SetDisplayOn(0);
              display_enabled = 0;
          }
      }

      HAL_Delay(10); 
  
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
    /* USER CODE BEGIN 3 */
}
  /* USER CODE END 3 */


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

  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) != 0x32F2)
    {
        /** Initialize RTC and set the Time and Date
        */
        sTime.Hours = 21;      // 9 PM in 24-hour time
        sTime.Minutes = 07;    // Current minute
        sTime.Seconds = 0;
        sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
        sTime.StoreOperation = RTC_STOREOPERATION_RESET;
      
        // Note: We changed FORMAT_BCD to FORMAT_BIN so normal decimal numbers work
        if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
        {
          Error_Handler();
        }
      
        sDate.WeekDay = RTC_WEEKDAY_MONDAY;
        sDate.Month = RTC_MONTH_JULY;
        sDate.Date = 20;      // 20th
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
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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

  /*Configure GPIO pins : User_UP_Pin User_DOWN_Pin Menu_Pin */
  GPIO_InitStruct.Pin = User_UP_Pin|User_DOWN_Pin|Menu_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    // Check if the interrupt came from the IMU pin
    if (GPIO_Pin == IMU_int_Pin)
    {
        // Force the main loop to read sensors immediately by tricking the timer
        // We set last_sensor_read_ms far into the past so the standard interval triggers on the next loop
        last_sensor_read_ms = 0; 
        
        // Optional: Read INT1_SRC (0x31) to clear the interrupt flag on the sensor[cite: 1]
        uint8_t reg = 0x31 | 0x80;
        uint8_t dummy_read;
        HAL_I2C_Master_Transmit(&hi2c1, LIS3DH_ADDR, &reg, 1, 100);
        HAL_I2C_Master_Receive(&hi2c1, LIS3DH_ADDR, &dummy_read, 1, 100);

        shock_event_data = dummy_read;
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
