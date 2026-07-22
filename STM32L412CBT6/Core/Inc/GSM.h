#ifndef INC_GSM_H_
#define INC_GSM_H_

#include "stm32l4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

// MAXIMUM READINGS IN RAM BEFORE UPLOAD
// Holds exactly 30 minutes of data at a 60-second logging interval
#define MAX_READINGS 30 

typedef struct {
    char date[16]; 
    char time[16]; // Large enough to hold "YYYY-MM-DD HH:MM:SS"
    float temp_c;
    float hum_pct;
    int lis3dhx;
    int lis3dhy;
    int lis3dhz;
} SensorReading_t;

// Public Functions
void GSM_Init(UART_HandleTypeDef *huart);
void GSM_AddReading(const char* date, const char* time, float temp, float hum, int lis_x, int lis_y, int lis_z);
void GSM_UploadBuffer(void);
int GSM_GetBufferCount(void);

#endif /* INC_GSM_H_ */