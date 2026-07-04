#include "i2c_display.h"

static HAL_StatusTypeDef I2CDisplay_SendCommand(I2CDisplay_Typedef *display, uint8_t command)
{
    return HAL_I2C_Master_Transmit(display->hi2c, display->slave_address, &command, 1, HAL_MAX_DELAY);
}

void I2CDisplay_Init(I2CDisplay_Typedef *display, I2C_HandleTypeDef *hi2c, uint8_t slave_address)
{
    display->hi2c = hi2c;
    display->slave_address = slave_address;

    (void)I2CDisplay_SendCommand(display, 0x00);
}

HAL_StatusTypeDef I2CDisplay_WriteString(I2CDisplay_Typedef *display, const char *text)
{
    if (text == NULL)
    {
        return HAL_ERROR;
    }

    return HAL_I2C_Master_Transmit(display->hi2c, display->slave_address, (uint8_t *)text, (uint16_t)strlen(text), HAL_MAX_DELAY);
}

HAL_StatusTypeDef I2CDisplay_WriteLine(I2CDisplay_Typedef *display, uint8_t line, const char *text)
{
    (void)line;
    return I2CDisplay_WriteString(display, text);
}

void I2CDisplay_Clear(I2CDisplay_Typedef *display)
{
    (void)display;
}
