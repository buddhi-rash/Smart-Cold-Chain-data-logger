#include "Sensors.h"
#include <stdint.h>
#include <math.h>


#define RREF                430.0f  // 400 Ohm reference resistor for PT100[cite: 1]
#define RTD_A               3.90830e-3f
#define RTD_B               -5.77500e-7f
#define R_0                 100.0f   // Resistance of PT100 at 0°C

// MAX31865 Register Addresses
#define REG_CONFIG_READ     0x00
#define REG_CONFIG_WRITE    0x80
#define REG_RTD_MSB         0x01
#define REG_FAULT_STATUS    0x07

// Configuration register bitmasks
#define CONFIG_VBIAS        0x80  // D7: 1 = Bias ON
#define CONFIG_1SHOT        0x20  // D5: 1 = 1-shot conversion
#define CONFIG_3WIRE        0x10  // D4: 1 = 3-wire config
#define CONFIG_FAULT_CLR    0x02  // D1: 1 = Clear fault status
#define CONFIG_50HZ         0x01  // D0: 1 = 50Hz filter

#define MAX_CS_LOW()  HAL_GPIO_WritePin(MAX_CS_GPIO_Port, MAX_CS_Pin, GPIO_PIN_RESET)
#define MAX_CS_HIGH() HAL_GPIO_WritePin(MAX_CS_GPIO_Port, MAX_CS_Pin, GPIO_PIN_SET)


//SHT45
#define SHT45_ADDR (0x44 << 1) // 0x88
// LIS3DHTR
#define LIS3DH_ADDR (0x18 << 1) // 0x30


volatile float live_temperature = 0.0f;
volatile uint16_t raw_rtd_code = 0;
volatile float R_rtd = 0.0f;
volatile uint8_t fault_register = 0;
float temp = 0.0f; // Optional: Variable to hold calculated temperature
float dummy = 0.0f; // Optional: Dummy variable to indicate valid temperature reading

//SHT45 Variable to hold the calculated temperature and humidity
float temperature = 0.0f;
float humidity = 0.0f;

// LIS3DHTR Variables to hold the X, Y, Z axis readings
int16_t LIS3DH_X = 0;
int16_t LIS3DH_Y = 0;
int16_t LIS3DH_Z = 0;

// Shifts the SPI bus into Mode 3 for the PT100 Sensor
static void SPI_Set_Mode_MAX31865(void)
{
    hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
    hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
    HAL_SPI_Init(&hspi1);
}

void MAX31865_Init(void)
{
  SPI_Set_Mode_MAX31865(); // Ensure SPI is in Mode 3 for the MAX31865
  // Set the configuration register to enable VBIAS, 3-wire mode, and 50Hz filter
  // 1. Turn VBIAS ON, set 3-wire mode, and configure the 50Hz filter
  uint8_t config = CONFIG_VBIAS | CONFIG_3WIRE | CONFIG_50HZ;
  MAX31865_WriteRegister(REG_CONFIG_WRITE, config);
  
}

void MAX31865_WriteRegister(uint8_t reg, uint8_t value)
{
  SPI_Set_Mode_MAX31865(); // Ensure SPI is in Mode 3 for the MAX31865
  uint8_t data[2] = { reg, value };
  MAX_CS_LOW();
  HAL_SPI_Transmit(&hspi1, data, 2, HAL_MAX_DELAY);
  MAX_CS_HIGH();
}

uint8_t MAX31865_ReadRegister(uint8_t reg)
{
  SPI_Set_Mode_MAX31865(); // Ensure SPI is in Mode 3 for the MAX31865
  uint8_t tx_data[2] = { reg, 0x00 };
  uint8_t rx_data[2] = { 0 };
  
  MAX_CS_LOW();
  HAL_SPI_TransmitReceive(&hspi1, tx_data, rx_data, 2, HAL_MAX_DELAY);
  MAX_CS_HIGH();
  
  return rx_data[1];
}

void MAX31865_ReadRTD(void)
{
  SPI_Set_Mode_MAX31865(); // Ensure SPI is in Mode 3 for the MAX31865
  uint8_t tx_data[3] = { REG_RTD_MSB, 0x00, 0x00 };
  uint8_t rx_data[3] = { 0 };
  
  // 1. Recreate the base configuration (VBIAS ON, 3-Wire, 50Hz)
  uint8_t config = CONFIG_VBIAS | CONFIG_3WIRE | CONFIG_50HZ;
  // 2. Wait for the input filter capacitors to charge (~10.5 RC constants + 1ms)
  HAL_Delay(15); 
  
  // 3. Trigger 1-Shot conversion by adding the 1-SHOT bit
  config |= CONFIG_1SHOT;
  MAX31865_WriteRegister(REG_CONFIG_WRITE, config);
  
  // 4. Wait for conversion to complete (~62.5ms for 50Hz single conversion)
  HAL_Delay(65); 
  // 5. Read the 2 RTD data bytes sequentially
  MAX_CS_LOW();
  HAL_SPI_TransmitReceive(&hspi1, tx_data, rx_data, 3, HAL_MAX_DELAY);
  MAX_CS_HIGH();
  
  // 6. Instantly shut off VBIAS to save power
  MAX31865_WriteRegister(REG_CONFIG_WRITE, CONFIG_3WIRE | CONFIG_50HZ);
  
  // 7. Process Data
  uint16_t rtd_raw = (rx_data[1] << 8) | rx_data[2];
  
  // Check if the Fault flag (D0 of LSB) is set[cite: 1]
  if (rtd_raw & 0x01) 
  {
    dummy= 3.0f; // Optional: Set a dummy variable to indicate fault condition
    fault_register = MAX31865_ReadRegister(REG_FAULT_STATUS); // Read fault register[cite: 1]
    MAX31865_WriteRegister(REG_CONFIG_WRITE, CONFIG_FAULT_CLR | CONFIG_3WIRE | CONFIG_50HZ); // Clear fault[cite: 1]
    return; // Skip calculation on active fault
  }
  
  raw_rtd_code = rtd_raw >> 1; // Shift out the fault status bit to get 15-bit ADC code[cite: 1]
  
  // 8. Calculate Resistance[cite: 1]
  R_rtd = ((float)raw_rtd_code * RREF) / 32768.0f; // R_rtd = (ADC_code * Rref) / 2^15[cite: 1]
  
  if (R_rtd >= 100.0f) 
  {
    dummy = 2.0f; // Optional: Set a dummy variable to indicate invalid resistance reading
    float Z1 = -RTD_A;
    float Z2 = RTD_A * RTD_A - 4.0f * RTD_B * (1.0f - (R_rtd / R_0));
    temp = (Z1 + sqrtf(Z2)) / (2.0f * RTD_B);

  } else if (R_rtd < 100.0f) 
  {
    temp = -242.02f 
            + (2.2228f * R_rtd) 
            + (2.5859e-3f * R_rtd * R_rtd) 
            + (4.8260e-6f * R_rtd * R_rtd * R_rtd) 
            - (2.8183e-8f * R_rtd * R_rtd * R_rtd * R_rtd);
  }

  
  // 10. Range validation check before mapping to the watch variable[cite: 1]
  if (temp >= -200.0f && temp <= 850.0f) 
  {
    live_temperature = temp;
    dummy = 1.0f; // Optional: Set a dummy variable to indicate valid temperature reading
  }
}

void SHT45_Read (void)
{
    uint8_t cmd = 0xFD; // High precision measurement command
    uint8_t data[6] = {0};

    if (HAL_I2C_Master_Transmit(&hi2c1, SHT45_ADDR, &cmd, 1, HAL_MAX_DELAY) == HAL_OK)
    {
        HAL_Delay(10); // Wait for measurement to complete (~max 8.3ms)
        
        // Read 6 bytes (2B Temp + 1B CRC + 2B Humidity + 1B CRC)
        if (HAL_I2C_Master_Receive(&hi2c1, SHT45_ADDR, data, 6, HAL_MAX_DELAY) == HAL_OK)
        {
            uint16_t t_ticks = (data[0] << 8) | data[1];
            uint16_t rh_ticks = (data[3] << 8) | data[4];

            // Formulas provided by Sensirion datasheet
            temperature = -45.0f + 175.0f * ((float)t_ticks / 65535.0f);
            humidity = -6.0f + 125.0f * ((float)rh_ticks / 65535.0f);
            if (humidity > 100.0f) humidity = 100.0f;
            if (humidity < 0.0f) humidity = 0.0f;
        }
    }
}

void LIS3DHTR_Init(void)
{
    uint8_t config[2]= {0x20, 0x5F}; // CTRL_REG1 (0x20) -> Value 0x5F (100Hz data rate, Normal power mode, X/Y/Z enabled)
    // CTRL_REG1 (0x20) -> Value 0x57 (100Hz data rate, Normal power mode, X/Y/Z enabled)
    HAL_I2C_Master_Transmit(&hi2c1, LIS3DH_ADDR, config, 2, HAL_MAX_DELAY);
}

void LIS3DHTR_Read(int16_t* x, int16_t* y, int16_t* z)
{
    
    uint8_t reg = 0x28 | 0x80; // OUT_X_L register address. Bit 7 set to 1 for auto-increment.
    uint8_t data[6] = {0};

    if (HAL_I2C_Master_Transmit (&hi2c1, LIS3DH_ADDR, &reg, 1, HAL_MAX_DELAY) == HAL_OK)
    {
        if (HAL_I2C_Master_Receive(&hi2c1, LIS3DH_ADDR, data, 6, HAL_MAX_DELAY) == HAL_OK)
        {
            // Combine Low and High bytes for each axis
            *x = ((int16_t)((data[1] << 8) | data[0]))>>8;
            *y = ((int16_t)((data[3] << 8) | data[2]))>>8;
            *z = ((int16_t)((data[5] << 8) | data[4]))>>8;
        }
    }
}