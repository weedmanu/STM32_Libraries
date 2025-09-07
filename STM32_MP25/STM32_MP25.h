/*
 * STM32_MP25.h
 *
 *  Created on: Sep 6, 2025
 *      Author: weedm
 */

#ifndef STM32_MP25_H_
#define STM32_MP25_H_

// -----------------------------------------------------------------------------
//  INCLUDES
// -----------------------------------------------------------------------------
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "stm32l4xx_hal.h"

#ifdef __cplusplus
extern "C"
{
#endif

// -----------------------------------------------------------------------------
//  CONSTANTES / MACROS
// -----------------------------------------------------------------------------
/**
 * @brief Taille en octets d'une trame complète attendue depuis le capteur
 * (doit correspondre à l'implémentation dans le .c)
 */
#define PM25_FRAME_LEN 32

    // -----------------------------------------------------------------------------
    //  TYPES PUBLICS
    // -----------------------------------------------------------------------------
    /**
     * @brief Structure contenant les champs extraits d'une trame PM2.5
     *
     * raw_frame contient la trame brute telle que reçue (PM25_FRAME_LEN octets).
     * Les autres champs sont les valeurs décodées et prêtes à l'emploi.
     */
    typedef struct
    {
        uint8_t raw_frame[PM25_FRAME_LEN]; /**< Trame brute reçue du capteur */
        uint16_t pm1_0_standard;           /**< PM1.0 standard (µg/m³) */
        uint16_t pm2_5_standard;           /**< PM2.5 standard (µg/m³) */
        uint16_t pm10_standard;            /**< PM10 standard (µg/m³) */
        uint16_t pm1_0_atmospheric;        /**< PM1.0 atmosphérique (µg/m³) */
        uint16_t pm2_5_atmospheric;        /**< PM2.5 atmosphérique (µg/m³) */
        uint16_t pm10_atmospheric;         /**< PM10 atmosphérique (µg/m³) */
        uint16_t particles_0_3um;          /**< Particules >0.3µm (nombre / 0.1L) */
        uint16_t particles_0_5um;          /**< Particules >0.5µm (nombre / 0.1L) */
        uint16_t particles_1_0um;          /**< Particules >1.0µm (nombre / 0.1L) */
        uint16_t particles_2_5um;          /**< Particules >2.5µm (nombre / 0.1L) */
        uint16_t particles_5_0um;          /**< Particules >5.0µm (nombre / 0.1L) */
        uint16_t particles_10um;           /**< Particules >10µm (nombre / 0.1L) */
        uint8_t version;                   /**< Version / byte d'identification */
        uint16_t checksum;                 /**< Checksum lue depuis la trame */
    } PM25_FullData;

    /**
     * @brief Codes de retour / statut internes
     */
    typedef enum
    {
        PM25_STATUS_OK = 0,       /**< Succès */
        PM25_STATUS_TIMEOUT,      /**< Timeout lors de la lecture */
        PM25_STATUS_HEADER_ERR,   /**< En-tête de trame invalide */
        PM25_STATUS_CHECKSUM_ERR, /**< Checksum invalide */
        PM25_STATUS_UART_ERR      /**< Erreur liée à l'UART */
    } PM25_Status;

    /**
     * @brief Structure descriptive retournée par les fonctions d'interprétation
     * (emoji, label lisible et description courte)
     */
    typedef struct
    {
        int index;               /**< index / code original */
        const char *emoji;       /**< pictogramme court (UTF-8) */
        const char *label;       /**< libellé court */
        const char *description; /**< description détaillée */
    } PM_StatusInfo;

    /**
     * @brief Type de callback utilisateur pour notifications ou résultats
     * @param status Code de statut (voir PM25_Status)
     * @param data Pointeur vers PM25_FullData (NULL si erreur)
     * @param stats Pointeur générique pour statistiques / contexte (optionnel)
     */
    typedef void (*PM25_UserCallback)(int status, PM25_FullData *data, void *stats);

    // -----------------------------------------------------------------------------
    //  FONCTIONS D'INITIALISATION / CONFIGURATION
    // -----------------------------------------------------------------------------
    /**
     * @brief Configure la broche de contrôle "SET" du capteur (sortie)
     * @param port Port GPIO (ex: GPIOA)
     * @param pin  Pin (ex: GPIO_PIN_0)
     * @note La fonction ne configure pas le mode GPIO hardware, elle stocke
     *       uniquement le port/pin pour que la librairie puisse activer/mettre en veille
     */
    void PM25_SetControlPin(GPIO_TypeDef *port, uint16_t pin);

    /**
     * @brief Initialise le module PM2.5 (prépare le driver)
     * @param huart Handle UART utilisé pour communiquer avec le capteur
     * @note Implémentation minimale: peut contenir des délais de stabilisation
     */
    void PM25_Init(UART_HandleTypeDef *huart);

    /**
     * @brief Active ou désactive le mode debug (affichage via printf)
     * @param enable 1 = activer, 0 = désactiver
     */
    void PM25_SetDebugMode(int enable);

    // -----------------------------------------------------------------------------
    //  FONCTIONS DE LECTURE (synchrones / polling et DMA)
    // -----------------------------------------------------------------------------
    /**
     * @brief Lit une trame complète en mode polling (bloquant/cyclique selon impl.)
     * @param huart Handle UART
     * @param data Pointeur vers structure de sortie
     * @param interval_ms Intervalle entre lectures en ms (voir implémentation)
     * @return -1 si en cours, 1 si succès, 0 si timeout
     */
    int PM25_Polling_ReadFull(UART_HandleTypeDef *huart, PM25_FullData *data, uint32_t interval_ms);

    /**
     * @brief Lit une trame complète en mode DMA + interruption IDLE
     * @param huart Handle UART
     * @param data Pointeur vers structure de sortie
     * @param interval_ms Timeout / intervalle (0 = mode continu)
     * @return -1 si en cours, 1 si succès, 0 si timeout/erreur
     */
    int PM25_DMA_ReadFull_IT(UART_HandleTypeDef *huart, PM25_FullData *data, uint32_t interval_ms);

    /**
     * @brief Valide le checksum d'une trame brute
     * @param frame Pointeur vers la trame (au moins PM25_FRAME_LEN octets)
     * @return 1 si checksum valide, 0 sinon
     */
    int PM25_Validate_Checksum(const uint8_t *frame);

    /**
     * @brief Vérifie l'en-tête d'une trame (header attendu: 0x42 0x4D)
     * @param frame Pointeur vers la trame
     * @return 1 si header valide, 0 sinon
     */
    int PM25_Validate_Header(const uint8_t *frame);

    // -----------------------------------------------------------------------------
    //  FONCTIONS D'INTERPRÉTATION QUALITE D'AIR / RATIO
    // -----------------------------------------------------------------------------
    /**
     * @brief Retourne un code de qualité (catégorie) pour une valeur PM donnée
     * @param pm Valeur PM (µg/m³)
     * @param type Chaîne décrivant le type ("PM2.5", "PM10", ...)
     * @return Code de qualité (implémentation: entiers croissants = pire qualité)
     */
    int PM25_Quality_GetCode(uint16_t pm, const char *type);

    /**
     * @brief Interprète un code de qualité et retourne une structure lisible
     * @param code Code retourné par PM25_Quality_GetCode
     * @return PM_StatusInfo avec emoji/label/description (chaînes statiques)
     */
    PM_StatusInfo PM25_Quality_InterpretCode(int code);

    /**
     * @brief Calcule un code de ratio PM2.5/PM10
     * @param pm25 Valeur PM2.5
     * @param pm10 Valeur PM10
     * @return Code de ratio (voir implémentation pour les seuils)
     */
    int PM25_Ratio_GetCode(uint16_t pm25, uint16_t pm10);

    /**
     * @brief Interprète un code de ratio et retourne une pastille descriptive
     * @param code Code de ratio
     * @return PM_StatusInfo décrivant le ratio
     */
    PM_StatusInfo PM25_Ratio_Interpret(int code);

#ifdef __cplusplus
}
#endif

#endif /* STM32_MP25_H_ */
