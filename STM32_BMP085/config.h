#ifndef CONFIG_H
#define CONFIG_H

// Driver de la carte
#include "stm32l1xx_hal.h"

// Configuration de l'I2C
extern I2C_HandleTypeDef hi2c1;
#define CONFIG_I2C hi2c1  // Remplacez par l'instance I2C que vous utilisez

// Adresse I2C du capteur
#define CONFIG_I2C_ADDRESS 0x77

// Mode de fonctionnement du capteur
#define CONFIG_MODE BMP085_STANDARD  // Options : BMP085_ULTRALOWPOWER, BMP085_STANDARD, BMP085_HIGHRES, BMP085_ULTRAHIGHRES

#endif // CONFIG_H
