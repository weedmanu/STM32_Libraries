#ifndef INC_STM32_BME680_H_
#define INC_STM32_BME680_H_

/* Inclusions ------------------------------------------------------------------*/
#include "stm32l0xx_hal.h"
#include <math.h>
#include "STM32_BME680_defs.h"

/* Définitions des constantes -------------------------------------------------*/
#define GAS_CAL_DATA_WINDOW_SIZE 100

/* Structues ------------------------------------------------------------*/

/**
 * @brief Structure pour contenir les données traitées du capteur BME680.
 *
 * Cette structure est utilisée pour retourner les valeurs de température, pression, humidité,
 * et résistance du gaz après qu'elles aient été calculées à partir des lectures brutes du capteur.
 */
typedef struct
{
    float temperature;
    float pressure;
    float humidity;
    float gas_resistance;
} BME680Data;

/**
 * @brief Structure pour suivre les paramètres de la Qualité de l'Air Intérieur (QAI).
 *
 * Cette structure contient l'état et les données de calibration nécessaires pour le calcul de la QAI.
 */
typedef struct
{
    float slope;
    int burn_in_cycles_remaining;
    float gas_cal_data[GAS_CAL_DATA_WINDOW_SIZE];
    int gas_cal_data_idx;
    int gas_cal_data_filled_count;
    float gas_baseline;
} IAQTracker;

/**
 * @brief Structure pour contenir les paramètres initiaux du capteur BME680.
 *
 * Cela permet une initialisation simplifiée en regroupant les paramètres communs.
 */
typedef struct
{
    struct bme680_tph_sett tph_sett;
    struct bme680_gas_sett gas_sett;
    uint8_t power_mode;
} BME680_InitialSettings;

/* Prototypes des fonctions ---------------------------------------------*/

/*
 * Fonctions d'Interface Publique (API Utilisateur STM32 spécifique)
 * Ces fonctions constituent le point d'entrée principal pour utiliser le pilote BME680 avec STM32.
 */
int8_t BME680_Start(struct bme680_dev *dev, I2C_HandleTypeDef *i2c_handle,
                    bme680_delay_fptr_t delay_fptr, const BME680_InitialSettings *settings);

int8_t BME680_Config_Advanced(struct bme680_dev *dev, struct bme680_tph_sett *tph_settings, struct bme680_gas_sett *gas_settings, uint8_t power_mode);

/*
 * Fonctions de l'API Bosch BME680 (Wrappers ou implémentations directes)
 * Ces fonctions fournissent une interface similaire à l'API standard de Bosch
 * et sont utilisées par les fonctions d'interface publique STM32 ou directement.
 */

/**
 * @brief Initialise le capteur BME680.
 * Cette fonction lit l'ID de la puce du capteur, effectue une réinitialisation logicielle du capteur, et lit les données de calibration.
 */
int8_t bme680_init(struct bme680_dev *dev);

/**
 * @brief Écrit des données dans un registre ou une série de registres.
 */
int8_t bme680_set_regs(const uint8_t *reg_addr, const uint8_t *reg_data, uint8_t len, struct bme680_dev *dev);

/**
 * @brief Lit des données depuis un registre ou une série de registres.
 */
int8_t bme680_get_regs(uint8_t reg_addr, uint8_t *reg_data, uint16_t len, struct bme680_dev *dev);

/**
 * @brief Effectue une réinitialisation logicielle du capteur.
 */
int8_t bme680_soft_reset(struct bme680_dev *dev);

/**
 * @brief Définit les paramètres TPH (Température, Pression, Humidité), gaz, et filtre du capteur.
 */
int8_t bme680_set_sensor_settings(uint16_t desired_settings, struct bme680_dev *dev);

/**
 * @brief Obtient les paramètres TPH, gaz, et filtre du capteur.
 */
int8_t bme680_get_sensor_settings(uint16_t desired_settings, struct bme680_dev *dev);

/**
 * @brief Définit le mode d'alimentation du capteur (veille ou forcé).
 */
int8_t bme680_set_sensor_mode(struct bme680_dev *dev);

/**
 * @brief Obtient le mode d'alimentation actuel du capteur.
 */
int8_t bme680_get_sensor_mode(struct bme680_dev *dev);

/**
 * @brief Get the sensor data (temperature, pressure, humidity, gas resistance).
 */
int8_t bme680_get_sensor_data(struct bme680_field_data *data, struct bme680_dev *dev);

/**
 * @brief Définit la durée du profil de mesure du gaz.
 */
void bme680_set_profile_dur(uint16_t duration, struct bme680_dev *dev);

/**
 * @brief Obtient la durée du profil de mesure du gaz.
 */
void bme680_get_profile_dur(uint16_t *duration, const struct bme680_dev *dev);

/*
 * Fonctions pour la Qualité de l'Air Intérieur (IAQ)
 * Ces fonctions sont dédiées au calcul et à l'interprétation de l'indice de qualité de l'air.
 */
void initIAQTracker(IAQTracker *tracker, int burn_in_total_cycles, float ph_slope);

float getIAQ(IAQTracker *tracker, BME680Data bme_data);

float waterSatDensity(float temp);

const char *getIAQCategory(float iaq);

/*
 * Fonction de Temporisation (Callback Utilisateur)
 * Cette fonction est une implémentation de la fonction de délai requise par l'API BME680,
 * typiquement utilisant HAL_Delay pour STM32.
 */
void user_delay_ms(uint32_t period);

#endif
