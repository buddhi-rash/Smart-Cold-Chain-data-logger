#ifndef __I2C_DISPLAY_H
#define __I2C_DISPLAY_H

#include "main.h"
#include <stdint.h>

typedef struct
{
    I2C_HandleTypeDef *hi2c;
    uint8_t slave_address;
} I2CDisplay_Typedef;

void I2CDisplay_Init(I2CDisplay_Typedef *display, I2C_HandleTypeDef *hi2c, uint8_t slave_address);
HAL_StatusTypeDef I2CDisplay_WriteString(I2CDisplay_Typedef *display, const char *text);
HAL_StatusTypeDef I2CDisplay_WriteLine(I2CDisplay_Typedef *display, uint8_t line, const char *text);
void I2CDisplay_Clear(I2CDisplay_Typedef *display);

#endif
 