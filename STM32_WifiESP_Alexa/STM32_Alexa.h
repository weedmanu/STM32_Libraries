// STM32_Alexa.h
#ifndef STM32_ALEXA_H
#define STM32_ALEXA_H

#include "STM32_WifiESP.h"
#include "main.h"

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

// Initialisation du module Alexa
void alexa_init(void);

// Ajout d'un périphérique GPIO (ON/OFF)
int alexa_add_gpio_device(const char *name, GPIO_TypeDef *port, uint16_t pin);

// Ajout d'un périphérique PWM (variable)
int alexa_add_pwm_device(const char *name, TIM_HandleTypeDef *htim, uint32_t channel);

// Traitement des requêtes Alexa (à appeler dans la boucle principale)
void alexa_process(void);

#endif // STM32_ALEXA_H
