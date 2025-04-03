/*
 * config.h
 *
 *  Created on: Apr 3, 2025
 *      Author: weedm
 */

#ifndef INC_CONFIG_H_
#define INC_CONFIG_H_

// Inclure la bibliothèque HAL spécifique à la famille STM32 utilisée
#include "stm32l4xx_hal.h"

// Port GPIO utilisé pour le bus 1-Wire
#define ONEWIRE_GPIO_PORT GPIOA

// Broche GPIO utilisée pour le bus 1-Wire
#define ONEWIRE_GPIO_PIN GPIO_PIN_10

// Définir le timer utilisé pour les délais 1-Wire
extern TIM_HandleTypeDef htim1;
#define ONEWIRE_TIMER &htim1

// Définir le nombre maximal de capteurs DS18B20
#define DS18B20_MAX_SENSORS 1

// Définir la résolution par défaut des capteurs DS18B20 9, 10, 11 ou 12 bits
#define DS18B20_DEFAULT_RESOLUTION DS18B20_Resolution_12bits

#define _DS18B20_USE_CRC // Activer la vérification CRC

#endif /* INC_CONFIG_H_ */
