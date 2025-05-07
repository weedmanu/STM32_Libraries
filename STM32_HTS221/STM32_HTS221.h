#ifndef STM32_HTS221_H_
#define STM32_HTS221_H_

#include "stm32l4xx_hal.h" // Pour I2C_HandleTypeDef
#include <stdio.h>         // Pour printf dans les messages d'erreur (peut être retiré pour une bibliothèque pure)

/**
  * @brief  Initialise le capteur HTS221.
  * @param  hi2c: Pointeur vers la structure I2C.
  * @retval Aucun.
  */
void HTS221_Init(I2C_HandleTypeDef *hi2c);

float HTS221_ReadTemperature(I2C_HandleTypeDef *hi2c);

float HTS221_ReadHumidity(I2C_HandleTypeDef *hi2c);

// Note: HTS221_ReadCalibration est maintenant appelée en interne par HTS221_Init.

#endif /* STM32_HTS221_H_ */
