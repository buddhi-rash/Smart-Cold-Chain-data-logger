#include "GSM.h"
#include <stdio.h>
#include <string.h>

// Private Variables
static UART_HandleTypeDef *gsm_uart;
static SensorReading_t readingBuffer[MAX_READINGS];
static int currentReadingCount = 0;

// 4096 bytes safely holds 30 JSON objects (approx 3,300 bytes total)
static char jsonPayload[4096]; 
static char tempStrBuffer[200]; 
static char atCommand[100];
static char rxBuffer[256];

// ---------------------------------------------------------
// INITIALIZATION
// ---------------------------------------------------------
void GSM_Init(UART_HandleTypeDef *huart) {
    gsm_uart = huart; 
    currentReadingCount = 0;
}

// ---------------------------------------------------------
// UTILITY
// ---------------------------------------------------------
int GSM_GetBufferCount(void) {
    return currentReadingCount;
}

// ---------------------------------------------------------
// ADD READING TO RAM BUFFER
// ---------------------------------------------------------
void GSM_AddReading(const char* date, const char* time, float temp, float hum, int lis_x, int lis_y, int lis_z) {
    if (currentReadingCount < MAX_READINGS) {
        // Copy the string into the struct safely
        strncpy(readingBuffer[currentReadingCount].date, date, sizeof(readingBuffer[0].date) - 1);
        readingBuffer[currentReadingCount].date[sizeof(readingBuffer[0].date) - 1] = '\0';

        // Copy Time
        strncpy(readingBuffer[currentReadingCount].time, time, sizeof(readingBuffer[0].time) - 1);
        readingBuffer[currentReadingCount].time[sizeof(readingBuffer[0].time) - 1] = '\0';

        readingBuffer[currentReadingCount].temp_c = temp;
        readingBuffer[currentReadingCount].hum_pct = hum;
        readingBuffer[currentReadingCount].lis3dhx = lis_x;
        readingBuffer[currentReadingCount].lis3dhy = lis_y;
        readingBuffer[currentReadingCount].lis3dhz = lis_z;
        currentReadingCount++;
    }
}

// ---------------------------------------------------------
// INTERNAL AT COMMAND HELPER
// ---------------------------------------------------------
static bool sendATCommand(const char* cmd, const char* expected_response, uint32_t timeout_ms) {
    memset(rxBuffer, 0, sizeof(rxBuffer));
    HAL_UART_Transmit(gsm_uart, (uint8_t*)cmd, strlen(cmd), 1000);
    
    uint32_t startTick = HAL_GetTick();
    int idx = 0;
    
    while ((HAL_GetTick() - startTick) < timeout_ms) {
        uint8_t c;
        if (HAL_UART_Receive(gsm_uart, &c, 1, 10) == HAL_OK) {
            if (idx < (sizeof(rxBuffer) - 1)) {
                rxBuffer[idx++] = c;
            }
            if (strstr(rxBuffer, expected_response) != NULL) {
                return true; 
            }
        }
    }
    return false; 
}
// ---------------------------------------------------------
// INTERNAL NETWORK CONNECTION CHECK
// ---------------------------------------------------------

static bool GSM_EnsureNetworkConnection(void) {
    // 1. Wake up the module and verify communication
    sendATCommand("AT\r\n", "OK", 1000);
    
    // 2. Check if SIM card is ready
    if (!sendATCommand("AT+CPIN?\r\n", "READY", 2000)) {
        return false; // No SIM or SIM error
    }

    // 3. Wait for Network Registration (0,1 = Home Network, 0,5 = Roaming)
    // We poll this up to 10 times to give the modem time to attach to the tower
    bool isRegistered = false;
    for (int i = 0; i < 10; i++) {
        if (sendATCommand("AT+CREG?\r\n", "0,1", 1000) || 
            sendATCommand("AT+CREG?\r\n", "0,5", 1000)) {
            isRegistered = true;
            break;
        }
    }
    if (!isRegistered) return false;

    // 4. Set the specific APN
    sendATCommand("AT+CGDCONT=1,\"IP\",\"dialogbb\"\r\n", "OK", 2000);

    // 5. Activate the PDP Context (Connect to the internet)
    // This can take a few seconds, so we give it a 10-second timeout
    if (!sendATCommand("AT+CGACT=1,1\r\n", "OK", 10000)) {
        return false; // Failed to get an IP address
    }

    // 6. Fix SSL configuration to satisfy Google's security requirements
    sendATCommand("AT+CSSLCFG=\"authmode\",0,0\r\n", "OK", 2000);

    return true; // Network is fully primed and ready for HTTP
}

// ---------------------------------------------------------
// INTERNAL JSON BUILDER
// ---------------------------------------------------------
static int buildBatchPayload(void) {
    strcpy(jsonPayload, "{\"readings\":[");
    
    for (int i = 0; i < currentReadingCount; i++) {
        // Formats the timestamp securely as a text string
        snprintf(tempStrBuffer, sizeof(tempStrBuffer), 
            "{\"date\":\"%s\",\"time\":\"%s\",\"temperature_c\":%.2f,\"humidity_pct\":%.2f,\"lis3dhx\":%d,\"lis3dhy\":%d,\"lis3dhz\":%d}",
            readingBuffer[i].date, 
            readingBuffer[i].time, 
            readingBuffer[i].temp_c, 
            readingBuffer[i].hum_pct, 
            readingBuffer[i].lis3dhx, 
            readingBuffer[i].lis3dhy, 
            readingBuffer[i].lis3dhz
        );
        
        strcat(jsonPayload, tempStrBuffer);
        
        if (i < currentReadingCount - 1) {
            strcat(jsonPayload, ",");
        }
    }
    strcat(jsonPayload, "]}");
    return strlen(jsonPayload);
}

// ---------------------------------------------------------
// THE CLOUD UPLOAD SEQUENCE
// ---------------------------------------------------------
void GSM_UploadBuffer(void) {
    // 1. Safety check: do we have data?
    if (currentReadingCount == 0) return; 

    // 2. CRITICAL: Ensure we have a valid cellular data connection before proceeding
    if (!GSM_EnsureNetworkConnection()) {
        // If this fails, we exit without clearing currentReadingCount.
        // The data stays safely in RAM and we will try again next cycle.
        return; 
    }

    // 3. Build the JSON string and get its length
    int payloadLength = buildBatchPayload();

    // 4. Execute the HTTP POST sequence
    sendATCommand("AT+HTTPTERM\r\n", "OK", 2000); 
    if (!sendATCommand("AT+HTTPINIT\r\n", "OK", 2000)) return;
    
    sendATCommand("AT+HTTPPARA=\"URL\",\"https://script.google.com/macros/s/AKfycbzbstjnyDSZQ5v1PGncT8iKw7GQHgHx_ergVSFKYuD6g2uDOam32AyzHTi-oUorvv-0NA/exec\"\r\n", "OK", 2000);
    sendATCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"\r\n", "OK", 2000);

    snprintf(atCommand, sizeof(atCommand), "AT+HTTPDATA=%d,10000\r\n", payloadLength);
    
    if (sendATCommand(atCommand, "DOWNLOAD", 5000)) {
        HAL_UART_Transmit(gsm_uart, (uint8_t*)jsonPayload, payloadLength, 2000);
        HAL_Delay(500); 

        // Look for the 1,302 success code from Google Apps Script
        if (sendATCommand("AT+HTTPACTION=1\r\n", "1,302", 15000)) {
            currentReadingCount = 0; // Success: Clear RAM buffer for the next 30 minutes
        }
    }
    
    // Close the HTTP session
    sendATCommand("AT+HTTPTERM\r\n", "OK", 2000);
}
