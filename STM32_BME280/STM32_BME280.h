/*
 * STM32_BME280.h
 *
 *  Créé le : 3 mai 2025
 *      Auteur : weedm
 */

#ifndef INC_STM32_BME280_H_
#define INC_STM32_BME280_H_

#include "stm32l0xx_hal.h" // Remplacez stm32l0xx_hal.h si vous utilisez une autre série de carte ex : stm32f4xx_hal.h.
#include <stdint.h>

// #define DEBUG_ON

// --- Codes d'erreur spécifiques au BME280 ---
#define BME280_OK 0               // Opération réussie
#define BME280_ERROR_COMM -1      // Erreur de communication I2C
#define BME280_ERROR_CHIP_ID -2   // ID de puce incorrect
#define BME280_ERROR_CONFIG -3    // Erreur de configuration
#define BME280_ERROR_PARAM -4     // Paramètre invalide
#define BME280_ERROR_MEASURING -5 // Le capteur est en cours de mesure (mode forcé)

// --- Définitions des registres ---
#define BME280_ADDRESS_DEFAULT 0x76 // Adresse I2C par défaut

// Registres de calibration pour la température et la pression
#define BME280_REGISTER_DIG_T1 0x88
#define BME280_REGISTER_DIG_T2 0x8A
#define BME280_REGISTER_DIG_T3 0x8C
#define BME280_REGISTER_DIG_P1 0x8E
#define BME280_REGISTER_DIG_P2 0x90
#define BME280_REGISTER_DIG_P3 0x92
#define BME280_REGISTER_DIG_P4 0x94
#define BME280_REGISTER_DIG_P5 0x96
#define BME280_REGISTER_DIG_P6 0x98
#define BME280_REGISTER_DIG_P7 0x9A
#define BME280_REGISTER_DIG_P8 0x9C
#define BME280_REGISTER_DIG_P9 0x9E

// Registres de calibration pour l'humidité
#define BME280_REGISTER_DIG_H1 0xA1
#define BME280_REGISTER_DIG_H2 0xE1
#define BME280_REGISTER_DIG_H3 0xE3
#define BME280_REGISTER_DIG_H4 0xE4
#define BME280_REGISTER_DIG_H5 0xE5
#define BME280_REGISTER_DIG_H6 0xE7

// Autres registres importants
#define BME280_REGISTER_CHIPID 0xD0       // Identifiant du capteur
#define BME280_REGISTER_VERSION 0xD1      // Version du capteur
#define BME280_REGISTER_SOFTRESET 0xE0    // Registre pour réinitialiser le capteur
#define BME280_REGISTER_CONTROLHUMID 0xF2 // Contrôle du suréchantillonnage de l'humidité
#define BME280_REGISTER_STATUS 0xF3       // Statut du capteur
#define BME280_REGISTER_CONTROL 0xF4      // Contrôle du mode et du suréchantillonnage
#define BME280_REGISTER_CONFIG 0xF5       // Configuration (filtrage, temps d'attente)
#define BME280_REGISTER_PRESSUREDATA 0xF7 // Données de pression
#define BME280_REGISTER_TEMPDATA 0xFA     // Données de température
#define BME280_REGISTER_HUMIDDATA 0xFD    // Données d'humidité

// --- Définitions pour la configuration ---

// Modes de fonctionnement (Registre 0xF4 <1:0>)
typedef enum
{
    BME280_MODE_SLEEP = 0x00,  // Mode veille
    BME280_MODE_FORCED = 0x01, // Mode forcé (une mesure puis veille)
    BME280_MODE_NORMAL = 0x03  // Mode normal (cycle de mesures)
} BME280_Mode_t;

// Suréchantillonnage (Registres 0xF2 <2:0> pour Humidité, 0xF4 <4:2> pour Pression, 0xF4 <7:5> pour Température)
typedef enum
{
    BME280_OVERSAMPLING_SKIPPED = 0x00, // Mesure ignorée
    BME280_OVERSAMPLING_X1 = 0x01,
    BME280_OVERSAMPLING_X2 = 0x02,
    BME280_OVERSAMPLING_X4 = 0x03,
    BME280_OVERSAMPLING_X8 = 0x04,
    BME280_OVERSAMPLING_X16 = 0x05
} BME280_Oversampling_t;

// Coefficient du filtre IIR (Registre 0xF5 <4:2>)
typedef enum
{
    BME280_FILTER_OFF = 0x00, // Filtre désactivé
    BME280_FILTER_X2 = 0x01,
    BME280_FILTER_X4 = 0x02,
    BME280_FILTER_X8 = 0x03,
    BME280_FILTER_X16 = 0x04
} BME280_Filter_t;

// Temps d'attente en mode Normal (Registre 0xF5 <7:5>)
typedef enum
{
    BME280_STANDBY_0_5_MS = 0x00,  // 0.5 ms
    BME280_STANDBY_62_5_MS = 0x01, // 62.5 ms
    BME280_STANDBY_125_MS = 0x02,  // 125 ms
    BME280_STANDBY_250_MS = 0x03,  // 250 ms
    BME280_STANDBY_500_MS = 0x04,  // 500 ms
    BME280_STANDBY_1000_MS = 0x05, // 1000 ms
    BME280_STANDBY_10_MS = 0x06,   // 10 ms   (Pour BME280)
    BME280_STANDBY_20_MS = 0x07    // 20 ms   (Pour BME280)
    // Note: Les valeurs 10ms et 20ms peuvent différer pour BMP280
} BME280_StandbyTime_t;

// --- Structures ---

// --- Structures ---

/**
 * @brief Structure pour stocker les données de calibration du capteur.
 */
typedef struct
{
    uint16_t dig_T1; // Coefficient de calibration pour la température
    int16_t dig_T2;
    int16_t dig_T3;
    uint16_t dig_P1; // Coefficient de calibration pour la pression
    int16_t dig_P2;
    int16_t dig_P3;
    int16_t dig_P4;
    int16_t dig_P5;
    int16_t dig_P6;
    int16_t dig_P7;
    int16_t dig_P8;
    int16_t dig_P9;
    uint8_t dig_H1; // Coefficient de calibration pour l'humidité
    int16_t dig_H2;
    uint8_t dig_H3;
    int16_t dig_H4;
    int16_t dig_H5;
    int8_t dig_H6;
} BME280_CalibData_t;

/**
 * @brief Structure pour définir la configuration du capteur BME280.
 */
typedef struct
{
    BME280_Mode_t mode;                   // Mode de fonctionnement (Sleep, Forced, Normal)
    BME280_Oversampling_t oversampling_p; // Suréchantillonnage Pression
    BME280_Oversampling_t oversampling_t; // Suréchantillonnage Température
    BME280_Oversampling_t oversampling_h; // Suréchantillonnage Humidité
    BME280_Filter_t filter;               // Coefficient du filtre IIR
    BME280_StandbyTime_t standby_time;    // Temps d'attente en mode Normal
} BME280_Config_t;

/**
 * @brief Structure principale pour gérer le capteur BME280.
 */
typedef struct
{
    I2C_HandleTypeDef *hi2c;       // Pointeur vers la structure de gestion I2C (HAL)
    uint8_t dev_addr;              // Adresse I2C 7 bits du capteur (non décalée)
    BME280_Config_t config;        // Configuration actuelle du capteur
    BME280_CalibData_t calib_data; // Données de calibration lues depuis le capteur
    int32_t t_fine;                // Valeur fine de température utilisée pour la compensation
} BME280_Handle_t;

// --- Prototypes des fonctions ---

// --- Documentation améliorée ---
/**
 * @brief Initialise le capteur BME280.
 * @param dev Pointeur vers la structure de gestion du capteur.
 * @param hi2c Pointeur vers la structure de gestion I2C.
 * @param dev_addr Adresse I2C 7 bits du capteur.
 * @param config Pointeur vers la structure de configuration souhaitée. Si NULL, une configuration par défaut sera utilisée.
 * @retval BME280_OK si l'initialisation réussit, sinon un code d'erreur BME280_ERROR_xxx.
 */
int8_t BME280_Init(BME280_Handle_t *dev, I2C_HandleTypeDef *hi2c, uint8_t dev_addr, BME280_Config_t *config);

/**
 * @brief Lit la température depuis le capteur.
 * @param dev Pointeur vers la structure de gestion du capteur.
 * @param temperature Pointeur pour stocker la température lue (en °C).
 * @retval Statut HAL.
 */
HAL_StatusTypeDef BME280_ReadTemperature(BME280_Handle_t *dev, float *temperature);

/**
 * @brief Lit la pression depuis le capteur.
 * @param dev Pointeur vers la structure de gestion du capteur.
 * @param pressure Pointeur pour stocker la pression lue (en Pa).
 * @retval Statut HAL.
 */
HAL_StatusTypeDef BME280_ReadPressure(BME280_Handle_t *dev, float *pressure);

/**
 * @brief Lit l'humidité depuis le capteur.
 * @param dev Pointeur vers la structure de gestion du capteur.
 * @param humidity Pointeur pour stocker l'humidité lue (en %RH).
 * @retval Statut HAL.
 */
HAL_StatusTypeDef BME280_ReadHumidity(BME280_Handle_t *dev, float *humidity);

/**
 * @brief Lit toutes les données (température, pression, humidité) du capteur.
 * @param dev Pointeur vers la structure de gestion du capteur.
 * @param temperature Pointeur pour stocker la température (en °C).
 * @param pressure Pointeur pour stocker la pression (en Pa).
 * @param humidity Pointeur pour stocker l'humidité (en %RH).
 * @retval BME280_OK si la lecture réussit, sinon un code d'erreur BME280_ERROR_xxx.
 */
int8_t BME280_ReadAll(BME280_Handle_t *dev, float *temperature, float *pressure, float *humidity);

/**
 * @brief Définit le mode de fonctionnement du capteur.
 * @param dev Pointeur vers la structure de gestion du capteur.
 * @param mode Le mode désiré (BME280_MODE_SLEEP, BME280_MODE_FORCED, BME280_MODE_NORMAL).
 * @retval Statut HAL.
 */
HAL_StatusTypeDef BME280_SetMode(BME280_Handle_t *dev, BME280_Mode_t mode);

/**
 * @brief Lit l'identifiant du capteur.
 * @param dev Pointeur vers la structure de gestion du capteur.
 * @param chip_id Pointeur pour stocker l'identifiant du capteur.
 * @retval Statut HAL.
 */
HAL_StatusTypeDef BME280_ReadChipID(BME280_Handle_t *dev, uint8_t *chip_id);

/**
 * @brief Réinitialise le capteur BME280.
 * @param dev Pointeur vers la structure de gestion du capteur.
 * @retval Statut HAL.
 */
HAL_StatusTypeDef BME280_Reset(BME280_Handle_t *dev);

/**
 * @brief Déclenche une mesure en mode forcé et attend la fin.
 * @param dev Pointeur vers la structure de gestion du capteur.
 * @param timeout Temps maximum d'attente en ms.
 * @retval BME280_OK si la mesure est terminée, BME280_ERROR_COMM en cas d'erreur I2C, HAL_TIMEOUT si le délai est dépassé.
 */
int8_t BME280_TriggerForcedMeasurement(BME280_Handle_t *dev, uint32_t timeout);

#endif /* INC_STM32_BME280_H_ */
