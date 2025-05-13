/**
 * @file STM32_DHT.h
 * @brief Fichier d'en-tête pour l'interface des capteurs DHT
 *
 * Ce fichier contient les définitions et les prototypes de fonctions pour
 * interagir avec les capteurs DHT (DHT11, DHT22, DHT21) en utilisant la
 * bibliothèque STM32 HAL. Il définit les types de capteurs, les codes de
 * retour, la structure de données du capteur et les fonctions publiques
 * pour initialiser et lire les données du capteur.
 *
 * @author weedm
 * @date 4 avril 2025 // Mise à jour de la date si nécessaire
 */

#ifndef INC_STM32_DHT_H_
#define INC_STM32_DHT_H_

#include "stm32l0xx_hal.h" // Remplacez stm32l0xx_hal.h si vous utilisez une autre série de carte ex : stm32f4xx_hal.h.

/**
 * @defgroup DHT_Types Types de capteurs DHT
 * @brief Définitions des différents types de capteurs DHT supportés.
 * @{
 */
#define DHT11 1 ///< Identifiant pour le capteur DHT11
#define DHT22 2 ///< Identifiant pour le capteur DHT22
#define DHT21 3 ///< Identifiant pour le capteur DHT21 (équivalent au DHT22)
/** @} */

/**
 * @struct DHT_Sensor
 * @brief Structure représentant un capteur DHT et ses données.
 *
 * Cette structure contient les informations de configuration (port, broche, type)
 * le handle du timer à utiliser pour les délais et la lecture,
 * et les dernières données brutes lues depuis le capteur, y compris la somme de contrôle.
 */
typedef struct
{
    GPIO_TypeDef *DHT_PORT; ///< Pointeur vers le port GPIO auquel le capteur est connecté.
    uint16_t DHT_PIN;       ///< Numéro de la broche GPIO à laquelle le capteur est connecté.
    uint8_t sensorType;     ///< Type du capteur (utiliser les définitions DHT11, DHT22, DHT21).
    // Données brutes lues depuis le capteur
    TIM_HandleTypeDef *htim; ///< Pointeur vers le handle du timer utilisé pour les délais/mesures.
    uint8_t hum1;            ///< Octet de poids fort de l'humidité (ou partie entière pour DHT11).
    uint8_t hum2;            ///< Octet de poids faible de l'humidité (ou partie décimale pour DHT11).
    uint8_t temp1;           ///< Octet de poids fort de la température (ou partie entière pour DHT11).
    uint8_t temp2;           ///< Octet de poids faible de la température (ou partie décimale pour DHT11).
    uint8_t SUM;             ///< Somme de contrôle reçue du capteur.
    uint8_t CHECK;           ///< Somme de contrôle calculée localement.
} DHT_Sensor;

/**
 * @brief Initialise une instance de capteur DHT.
 *
 * Configure la structure DHT_Sensor avec le port, la broche et le type spécifiés.
 * Stocke également le handle du timer à utiliser. Le timer doit être démarré
 * (via HAL_TIM_Base_Start) *avant* d'appeler cette fonction.
 *
 * @param sensor Pointeur vers la structure DHT_Sensor à initialiser.
 * @param htim Pointeur vers la structure TIM_HandleTypeDef du timer à utiliser (doit être configuré pour compter en microsecondes).
 * @param port Pointeur vers le port GPIO (ex: GPIOA, GPIOB, ...).
 * @param pin Numéro de la broche GPIO (ex: GPIO_PIN_0, GPIO_PIN_1, ...).
 * @param type Type du capteur (DHT11, DHT22, ou DHT21).
 * @return HAL_StatusTypeDef Statut de l'opération (HAL_OK en cas de succès, HAL_ERROR sinon).
 */
HAL_StatusTypeDef DHT_Init(DHT_Sensor *sensor, TIM_HandleTypeDef *htim, GPIO_TypeDef *port, uint16_t pin, uint8_t type);

/**
 * @brief Récupère et traite les données de température et d'humidité.
 *
 * Lance la communication avec le capteur, lit les 5 octets de données (humidité,
 * température, somme de contrôle), vérifie la somme de contrôle, et convertit
 * les données brutes en valeurs flottantes (degrés Celsius et pourcentage).
 *
 * @param sensor Pointeur vers la structure DHT_Sensor configurée. Les données brutes
 *               seront stockées dans cette structure.
 * @param data Tableau de 2 flottants où stocker les résultats :
 *             data[0] contiendra la température en °C.
 *             data[1] contiendra l'humidité relative en %.
 * @return HAL_StatusTypeDef Statut de l'opération (HAL_OK, HAL_ERROR pour non-réponse/checksum, HAL_TIMEOUT pour timeout lecture).
 */
HAL_StatusTypeDef DHT_GetData(DHT_Sensor *sensor, float data[2]);

#endif /* INC_STM32_DHT_H_ */
