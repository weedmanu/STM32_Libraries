#ifndef STM32_BMP085_H // Vérifie si le fichier d'en-tête n'a pas déjà été inclus
#define STM32_BMP085_H // Définit une macro pour éviter les inclusions multiples

#include "math.h" // Inclut la bibliothèque mathématique standard
#include "config.h" // Inclut un fichier de configuration spécifique au projet

// Modes
#define BMP085_ULTRALOWPOWER 0 //!< Mode ultra basse consommation
#define BMP085_STANDARD 1      //!< Mode standard
#define BMP085_HIGHRES 2       //!< Mode haute résolution
#define BMP085_ULTRAHIGHRES 3  //!< Mode ultra haute résolution

#define STD_ATM_PRESS 101325 // Pression atmosphérique standard au niveau de la mer en Pascal

// Déclaration des fonctions pour interagir avec le capteur BMP085
uint8_t BMP085_Init(uint8_t mode, I2C_HandleTypeDef *i2cdev); // Initialise le capteur BMP085 avec un mode et un périphérique I2C
float BMP085_readTemperature(void); // Lit et retourne la température mesurée par le capteur
int32_t BMP085_readPressure(void); // Lit et retourne la pression mesurée par le capteur
int32_t BMP085_readSealevelPressure(float altitude); // Calcule et retourne la pression au niveau de la mer en fonction de l'altitude
float BMP085_readAltitude(float sealevelPressure); // Calcule et retourne l'altitude en fonction de la pression au niveau de la mer

#endif // Fin de la protection contre les inclusions multiples
