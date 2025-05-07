#ifndef STM32_HTS221_H_
#define STM32_HTS221_H_

#include "stm32l4xx_hal.h" // Pour I2C_HandleTypeDef

typedef enum {
    HTS221_OK,
    HTS221_ERROR
} HTS221_Status;

/**
  * @brief  Initialise le capteur HTS221.
  * @param  hi2c: Pointeur vers la structure I2C.
  * @retval HTS221_Status: Statut de l'initialisation.
  */
HTS221_Status HTS221_Init(I2C_HandleTypeDef *hi2c);

/**
  * @brief  Lit la température depuis le capteur HTS221.
  * @param  hi2c: Pointeur vers la structure I2C.
  * @retval Température en degrés Celsius.
  */
float HTS221_ReadTemperature(I2C_HandleTypeDef *hi2c);

/**
  * @brief  Lit l'humidité depuis le capteur HTS221.
  * @param  hi2c: Pointeur vers la structure I2C.
  * @retval Humidité relative en pourcentage.
  */
float HTS221_ReadHumidity(I2C_HandleTypeDef *hi2c);

#endif /* STM32_HTS221_H_ */
