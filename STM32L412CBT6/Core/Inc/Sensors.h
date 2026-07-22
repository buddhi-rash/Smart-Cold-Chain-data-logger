#ifndef Sensors_H
#define Sensors_H

#include "main.h"

extern SPI_HandleTypeDef hspi1;
extern I2C_HandleTypeDef hi2c1;

extern volatile float live_temperature;
extern volatile uint8_t fault_register;
extern float dummy; // Optional: Dummy variable to indicate valid temperature reading

extern float temperature; // Variable to hold the calculated temperature from SHT45
extern float humidity; // Variable to hold the calculated humidity from SHT45

extern int16_t LIS3DH_X;
extern int16_t LIS3DH_Y;
extern int16_t LIS3DH_Z;

void MAX31865_Init(void);
void MAX31865_WriteRegister(uint8_t reg, uint8_t value);
uint8_t MAX31865_ReadRegister(uint8_t reg);
void MAX31865_ReadRTD(void);

void SHT45_Read (void);

void LIS3DHTR_Init(void);
void LIS3DHTR_Read(int16_t* x, int16_t* y, int16_t* z);

#endif /* Sensors_H */