/*
 * config.h
 *
 *  Created on: Apr 7, 2025
 *      Author: weedm
 */

#ifndef INC_STM32_SD_SPI_CONFIG_H_
#define INC_STM32_SD_SPI_CONFIG_H_

// Inclusion du driver HAL spécifique au microcontrôleur
#include "stm32l0xx_hal.h" // Assurez-vous que c'est le bon fichier HAL pour votre MCU

// Configuration des broches pour la carte SD
#define SD_CS_PORT GPIOB
#define SD_CS_PIN GPIO_PIN_1

// Configuration du SPI
extern SPI_HandleTypeDef hspi2;
#define HSPI_SDCARD &hspi2
#define SPI_TIMEOUT 100

// Macro pour activer/désactiver le débogage via printf
#define ENABLE_DEBUG 1 // Mettre à 1 pour activer le débogage, 0 pour le désactiver

#if ENABLE_DEBUG
#include <stdio.h>
#define DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...)
#endif

#endif /* INC_STM32_SD_SPI_CONFIG_H_ */
