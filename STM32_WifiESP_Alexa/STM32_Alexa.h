/*
 * STM32_Alexa.h
 *
 *  Created on: Jun 3, 2025
 *      Author: weedm
 */

#ifndef STM32_ALEXA_H
#define STM32_ALEXA_H

#include "STM32_WifiESP.h"
#include "main.h"

// Macros de débogage
#define ALEXA_DEBUG 1

#if ALEXA_DEBUG
#define alexa_logln(fmt, ...) printf("[ALEXA] " fmt "\r\n", ##__VA_ARGS__)
#define alexa_log(fmt, ...) printf("[ALEXA] " fmt, ##__VA_ARGS__)
#else
#define alexa_logln(fmt, ...)
#define alexa_log(fmt, ...)
#endif

// Types de périphériques supportés
typedef enum
{
	ALEXA_DEVICE_ONOFF,	  // Périphérique ON/OFF (GPIO)
	ALEXA_DEVICE_DIMMABLE // Périphérique variable (PWM)
} alexa_device_type_t;

// Structure d'un périphérique Alexa
typedef struct
{
	char name[32];			  // Nom du périphérique (affiché dans Alexa)
	char uniqueid[32];		  // ID unique du périphérique
	alexa_device_type_t type; // Type de périphérique
	GPIO_TypeDef *gpio_port;  // Port GPIO (pour type ONOFF)
	uint16_t gpio_pin;		  // Pin GPIO (pour type ONOFF)
	TIM_HandleTypeDef *htim;  // Timer PWM (pour type DIMMABLE)
	uint32_t channel;		  // Canal PWM (pour type DIMMABLE)
	uint8_t state;			  // État actuel (0=OFF, 1=ON)
	uint8_t brightness;		  // Luminosité (0-255, pour type DIMMABLE)
} alexa_device_t;

// Ces fonctions sont implémentées dans le fichier .c
// Ne pas déclarer les fonctions statiques dans le fichier d'en-tête

// Initialisation du module Alexa
void alexa_init(void);

// Ajout d'un périphérique GPIO (ON/OFF)
int alexa_add_gpio_device(const char *name, GPIO_TypeDef *port, uint16_t pin);

// Ajout d'un périphérique PWM (variable)
int alexa_add_pwm_device(const char *name, TIM_HandleTypeDef *htim, uint32_t channel);

// Traitement des requêtes Alexa (à appeler dans la boucle principale)
void alexa_process(void);

// Déclaration des fonctions pour la découverte et le contrôle des appareils
void alexa_handle_discovery(void);

void alexa_send_ssdp_notify(void);

#endif // STM32_ALEXA_H
