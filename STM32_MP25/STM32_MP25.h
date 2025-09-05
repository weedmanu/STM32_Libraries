#ifndef STM32_MP25_H_TEMP
#define STM32_MP25_H_TEMP

// -----------------------------------------------------------------------------
//  INCLUDES
// -----------------------------------------------------------------------------
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "stm32l4xx_hal.h"

// -----------------------------------------------------------------------------
//  CONSTANTES
// -----------------------------------------------------------------------------
#define PM25_FRAME_LEN 32
#define PM25_POLLING_TIMEOUT 3000

// -----------------------------------------------------------------------------
//  STRUCTURES
// -----------------------------------------------------------------------------
typedef struct
{
    uint8_t raw_frame[PM25_FRAME_LEN];
    uint16_t pm1_0_standard;
    uint16_t pm2_5_standard;
    uint16_t pm10_standard;
    uint16_t pm1_0_atmospheric;
    uint16_t pm2_5_atmospheric;
    uint16_t pm10_atmospheric;
    uint16_t particles_0_3um;
    uint16_t particles_0_5um;
    uint16_t particles_1_0um;
    uint16_t particles_2_5um;
    uint16_t particles_5_0um;
    uint16_t particles_10um;
    uint8_t version;
    uint16_t checksum;
} PM25_FullData;

// -----------------------------------------------------------------------------
//  ENUMS ET FLAGS (ERREUR, DEBUG, ASYNC)
// -----------------------------------------------------------------------------
typedef enum
{
    PM25_STATUS_OK = 0,
    PM25_STATUS_TIMEOUT,
    PM25_STATUS_HEADER_ERR,
    PM25_STATUS_CHECKSUM_ERR,
    PM25_STATUS_UART_ERR
} PM25_Status;

void PM25_SetDebugMode(int enable);

typedef void (*PM25_AsyncCallback)(PM25_Status status, PM25_FullData *data);
void PM25_StartAsyncRead(UART_HandleTypeDef *huart, PM25_AsyncCallback cb);
// Start async read using DMA (if available). Uses internal buffer and calls callback on complete.
void PM25_StartAsyncRead_DMA(UART_HandleTypeDef *huart, PM25_AsyncCallback cb);

typedef struct
{
    int index;
    const char *emoji;
    const char *label;
    const char *description;
} PM_StatusInfo;

// -----------------------------------------------------------------------------
//  FONCTIONS DE LECTURE ET VALIDATION
// -----------------------------------------------------------------------------
//  FONCTION DE CONFIGURATION PIN SET
// -----------------------------------------------------------------------------
void PM25_SetControlPin(GPIO_TypeDef *port, uint16_t pin);
// -----------------------------------------------------------------------------
void PM25_Polling_Init(UART_HandleTypeDef *huart);
// timeout_ms: pointer to timeout in ms. If NULL -> continuous mode (no SET toggle).
int PM25_Polling_ReadFull(UART_HandleTypeDef *huart, PM25_FullData *data, const uint32_t *timeout_ms);
int PM25_Validate_Header(const uint8_t *frame);
int PM25_Validate_Checksum(const uint8_t *frame);


// -----------------------------------------------------------------------------
//  FONCTIONS DE CONFIGURATION
// -----------------------------------------------------------------------------
void PM25_SetControlPin(GPIO_TypeDef *port, uint16_t pin);
void PM25_SetDebugMode(int enable);

// -----------------------------------------------------------------------------
//  FONCTIONS D'INTERPRETATION QUALITE AIR
// -----------------------------------------------------------------------------
int PM25_Quality_GetCode(uint16_t pm, const char *type);
PM_StatusInfo PM25_Quality_InterpretCode(int code);

// -----------------------------------------------------------------------------
//  FONCTIONS D'INTERPRETATION RATIO
// -----------------------------------------------------------------------------
int PM25_Ratio_GetCode(uint16_t pm25, uint16_t pm10);
PM_StatusInfo PM25_Ratio_Interpret(int code);

#endif // STM32_MP25_H_TEMP
