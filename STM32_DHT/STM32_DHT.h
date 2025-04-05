/**
 * @file STM32_DHT.h
 * @brief Fichier d'en-tête pour l'interface des capteurs DHT
 *
 * Ce fichier contient les définitions et les prototypes de fonctions pour
 * interagir avec les capteurs DHT (DHT11, DHT22, DHT21) en utilisant la
 * bibliothèque STM32 HAL.
 *
 * @author weedm
 * @date 2 avril 2025
 */

#ifndef INC_STM32_DHT_H_
#define INC_STM32_DHT_H_

#include "config.h"  // Inclure le fichier de configuration


/**
 * @defgroup DHT_Types Types de capteurs DHT
 * @brief Définitions des différents types de capteurs DHT
 * @{
 */
#define DHT11 1  ///< Type de capteur DHT11
#define DHT22 2  ///< Type de capteur DHT22
#define DHT21 3  ///< Type de capteur DHT21
/** @} */

// Codes de retour pour les fonctions
#define DHT_OK 0          ///< Opération réussie
#define DHT_ERROR 1       ///< Erreur générale
#define DHT_CHECKSUM_ERR 2 ///< Erreur de somme de contrôle
#define DHT_NO_RESPONSE 3 ///< Pas de réponse du capteur

/**
 * @struct DHT_Sensor
 * @brief Structure représentant un capteur DHT
 *
 * Cette structure contient la configuration et l'état d'un capteur DHT.
 */
typedef struct {
    GPIO_TypeDef* DHT_PORT; ///< Port GPIO auquel le capteur est connecté
    uint16_t DHT_PIN;       ///< Broche GPIO à laquelle le capteur est connecté
    uint8_t sensorType;     ///< Type du capteur (DHT11, DHT22, DHT21)
    uint8_t hum1, hum2;     ///< Octets de données d'humidité
    uint8_t temp1, temp2;   ///< Octets de données de température
    uint8_t SUM, CHECK;     ///< Octets de somme de contrôle pour l'intégrité des données
} DHT_Sensor;

/**
 * @brief Initialise le capteur DHT
 *
 * @param sensor Pointeur vers la structure DHT_Sensor à initialiser
 * @param port Port GPIO auquel le capteur est connecté
 * @param pin Broche GPIO à laquelle le capteur est connecté
 * @param type Type du capteur (DHT11, DHT22, DHT21)
 * @return uint8_t Code de retour (DHT_OK, DHT_ERROR, etc.)
 */
uint8_t DHT_Init(DHT_Sensor* sensor, GPIO_TypeDef* port, uint16_t pin, uint8_t type);

/**
 * @brief Génère un délai en microsecondes à l'aide d'un timer
 *
 * @param htim Pointeur vers une structure TIM_HandleTypeDef contenant les informations de configuration pour le module TIM.
 * @param delay Durée du délai en microsecondes
 */
void microDelay(TIM_HandleTypeDef *htim, uint16_t delay);

/**
 * @brief Démarre la communication avec le capteur DHT
 *
 * @param htim Pointeur vers une structure TIM_HandleTypeDef contenant les informations de configuration pour le module TIM.
 * @param sensor Pointeur vers la structure DHT_Sensor
 * @return uint8_t 1 si le capteur a répondu, 0 sinon
 */
uint8_t DHT_Start(TIM_HandleTypeDef *htim, DHT_Sensor* sensor);

/**
 * @brief Lit un octet de données depuis le capteur DHT
 *
 * @param htim Pointeur vers une structure TIM_HandleTypeDef contenant les informations de configuration pour le module TIM.
 * @param sensor Pointeur vers la structure DHT_Sensor
 * @return uint8_t L'octet lu depuis le capteur
 */
uint8_t DHT_Read(TIM_HandleTypeDef *htim, DHT_Sensor* sensor);

/**
 * @brief Récupère les données de température et d'humidité du capteur DHT
 *
 * @param sensor Pointeur vers la structure DHT_Sensor
 * @param data Tableau pour stocker les données de température et d'humidité
 * @return uint8_t Code de retour (DHT_OK, DHT_ERROR, etc.)
 */
uint8_t DHT_GetData(DHT_Sensor* sensor, float data[2]);

#endif /* INC_STM32_DHT_H_ */
