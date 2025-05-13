#ifndef STM32_BMP085_180_H // Vérifie si le fichier d'en-tête n'a pas déjà été inclus
#define STM32_BMP085_180_H // Définit une macro pour éviter les inclusions multiples

// Fichier d'en-tête pour la librairie de gestion des capteurs de pression barométrique
// Bosch BMP085 et BMP180 via I2C sur STM32.

#include "math.h" // Inclut la bibliothèque mathématique standard
#include <stdio.h>
#include "stm32l0xx_hal.h" // Remplacez stm32l0xx_hal.h si vous utilisez une autre série de carte ex : stm32f4xx_hal.h.

// #define DEBUG_ON

// Modes
#define BMP_ULTRALOWPOWER 0 //!< Mode ultra basse consommation
#define BMP_STANDARD 1      //!< Mode standard
#define BMP_HIGHRES 2       //!< Mode haute résolution
#define BMP_ULTRAHIGHRES 3  //!< Mode ultra haute résolution

#define BMP_DEFAULT_MODE BMP_STANDARD //!< Mode utilisé par BMP_Init_Default

// Codes de statut/erreur retournés par les fonctions de la librairie
typedef enum
{
  BMP_OK = 0,                //!< Opération réussie
  BMP_ERR_I2C_TX = 1,        //!< Erreur de transmission I2C
  BMP_ERR_I2C_RX = 2,        //!< Erreur de réception I2C
  BMP_ERR_INVALID_ID = 3,    //!< ID de puce incorrect lu
  BMP_ERR_CAL_READ = 4,      //!< Erreur lors de la lecture des données de calibration
  BMP_ERR_NULL_PTR = 5,      //!< Pointeur NULL fourni en argument
  BMP_ERR_INVALID_PARAM = 6, //!< Paramètre invalide fourni (ex: mode, pression/altitude négative)
  BMP_ERR_MATH = 7           //!< Erreur de calcul interne (ex: division par zéro)
} BMP_Status_t;

// Structure pour contenir les informations d'instance du capteur
typedef struct
{
  I2C_HandleTypeDef *i2cHandle; //!< Pointeur vers le handle I2C HAL
  uint8_t i2cAddr8bit;          //!< Adresse I2C 8 bits (décalée)
  uint8_t oversampling;         //!< Mode d'oversampling configuré

  // Données de calibration lues depuis le capteur
  int16_t ac1, ac2, ac3;
  uint16_t ac4, ac5, ac6;
  int16_t b1, b2;
  int16_t mb, mc, md;

  BMP_Status_t lastError; //!< Stocke la dernière erreur rencontrée (optionnel)
} BMP_Handle_t;

// Déclaration des fonctions pour interagir avec le capteur BMP085/BMP180
BMP_Status_t BMP_Init(BMP_Handle_t *bmp, uint8_t mode, I2C_HandleTypeDef *i2cdev, uint8_t i2c_address_7bit); // Initialise le capteur BMP.
BMP_Status_t BMP_Init_Default(BMP_Handle_t *bmp, I2C_HandleTypeDef *i2cdev, uint8_t i2c_address_7bit);       // Initialise le capteur BMP avec le mode par défaut.
BMP_Status_t BMP_readTemperature(BMP_Handle_t *bmp, float *temp_degC);                                       // Lit la température.
BMP_Status_t BMP_readPressure(BMP_Handle_t *bmp, int32_t *pressure_Pa);                                      // Lit la pression.
BMP_Status_t BMP_readAll(BMP_Handle_t *bmp, float *temp_degC, int32_t *pressure_Pa);                         // Lit température et pression efficacement.
BMP_Status_t BMP_calculateAltitude(int32_t pressure_Pa, float sealevelPressure_Pa, float *altitude_m);       // Calcule l'altitude à partir de la pression mesurée.
int32_t BMP_calculateSealevelPressure(int32_t pressure_Pa, float altitude_m);                                // Calcule la pression au niveau de la mer. Retourne 0 en cas d'erreur.

#endif // Fin de la protection contre les inclusions multiples
