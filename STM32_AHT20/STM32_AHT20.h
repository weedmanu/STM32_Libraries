/* aht20.h */
#ifndef STM32_AHT20_H
#define STM32_AHT20_H

#include "stm32l0xx_hal.h" // Remplacez stm32l0xx_hal.h si vous utilisez une autre série de carte ex : stm32f4xx_hal.h.
#include <stdio.h>

/* Adresse I2C du capteur AHT20 */
#define AHT20_I2C_ADDR 0x38 << 1 // Adresse 0x38 (7-bit) décalée pour format 8-bit

/* Commandes AHT20 */
#define AHT20_CMD_INIT 0xBE       // Initialisation
#define AHT20_CMD_TRIGGER 0xAC    // Déclenchement de la mesure
#define AHT20_CMD_SOFT_RESET 0xBA // Reset logiciel

/* Paramètres des commandes */
#define AHT20_INIT_PARAM1 0x08
#define AHT20_INIT_PARAM2 0x00
#define AHT20_MEASURE_PARAM1 0x33
#define AHT20_MEASURE_PARAM2 0x00

/* Délais typiques (en ms) et timeout I2C */
#define AHT20_I2C_TIMEOUT_MS 100           // Timeout pour les opérations I2C
#define AHT20_DELAY_SOFT_RESET_MS 20       // Délai après la commande de soft reset
#define AHT20_DELAY_INIT_CMD_WAIT_MS 20    // Délai après la commande d'initialisation (datasheet: 10ms min)
#define AHT20_DELAY_MEASUREMENT_WAIT_MS 80 // Délai d'attente pour la mesure (datasheet: >=75ms)

/* Structure pour contenir les données du capteur */
typedef struct
{
    float temperature; // Température en °C
    float humidity;    // Humidité relative en %
} AHT20_Data;

/* Codes d'erreur */
typedef enum
{
    AHT20_OK = 0,
    AHT20_ERROR_I2C,
    AHT20_ERROR_CHECKSUM,
    AHT20_ERROR_TIMEOUT,
    AHT20_ERROR_CALIBRATION,
    AHT20_ERROR_BUSY,
    AHT20_ERROR_INVALID_PARAM,
} AHT20_Status;

/* Fonction d'initialisation */
AHT20_Status AHT20_Init(I2C_HandleTypeDef *hi2c);

/* Fonction de reset logiciel */
AHT20_Status AHT20_SoftReset(I2C_HandleTypeDef *hi2c);

/* Fonction pour lire les mesures */
AHT20_Status AHT20_ReadMeasurements(I2C_HandleTypeDef *hi2c, AHT20_Data *data);

/* Fonction pour vérifier le statut du capteur */
AHT20_Status AHT20_GetStatus(I2C_HandleTypeDef *hi2c, uint8_t *status);

#endif /* STM32_AHT20_H */
