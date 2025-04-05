/*
 * config.h
 *
 *  Created on: Apr 4, 2025
 *      Author: weedm
 */

#ifndef INC_CONFIG_H_
#define INC_CONFIG_H_

#include "stm32l1xx_hal.h"  // Inclure le fichier HAL correspondant à votre carte

// Déclaration externe pour le timer utilisé
extern TIM_HandleTypeDef htim2;

// Définition du timer utilisé pour le capteur DHT
#define DHT_TIMER &htim2

#endif /* INC_CONFIG_H_ */
