#ifndef _DS18B20_H
#define _DS18B20_H

#include "onewire.h" // Inclusion de la bibliothèque pour le protocole 1-Wire
#include "config.h"  // Inclusion de la configuration globale du projet

//
// Configuration
//
#define _DS18B20_MAX_SENSORS    4 // Nombre maximal de capteurs DS18B20 pris en charge

//
// Structure du capteur DS18B20
//
typedef struct {
    uint8_t  Address[8];      // Adresse ROM unique du capteur (8 octets)
    float    Temperature;     // Dernière température mesurée
    uint8_t  ValidDataFlag;   // Indique si les données sont valides (1 = valides, 0 = invalides)
} Ds18b20Sensor_t;

//
// Commandes spécifiques au DS18B20
//
#define DS18B20_FAMILY_CODE      0x28 // Code de famille pour les capteurs DS18B20
#define DS18B20_CMD_ALARMSEARCH  0xEC // Commande pour rechercher les capteurs en alarme
#define DS18B20_CMD_CONVERTTEMP  0x44 // Commande pour démarrer une conversion de température

//
// Pas de conversion pour chaque résolution
//
#define DS18B20_STEP_12BIT       0.0625 // Pas pour une résolution de 12 bits
#define DS18B20_STEP_11BIT       0.125  // Pas pour une résolution de 11 bits
#define DS18B20_STEP_10BIT       0.25   // Pas pour une résolution de 10 bits
#define DS18B20_STEP_9BIT        0.5    // Pas pour une résolution de 9 bits

//
// Bits de configuration pour la résolution
//
#define DS18B20_RESOLUTION_R1    6 // Bit R1 pour configurer la résolution
#define DS18B20_RESOLUTION_R0    5 // Bit R0 pour configurer la résolution

//
// Longueur des données en fonction de l'utilisation du CRC
//
#ifdef _DS18B20_USE_CRC
#define DS18B20_DATA_LEN         9 // Longueur des données avec CRC (9 octets)
#else
#define DS18B20_DATA_LEN         5 // Longueur des données sans CRC (5 octets)
#endif

//
// Résolutions disponibles pour le capteur DS18B20
//
typedef enum {
    DS18B20_Resolution_9bits = 9,   // Résolution de 9 bits
    DS18B20_Resolution_10bits = 10, // Résolution de 10 bits
    DS18B20_Resolution_11bits = 11, // Résolution de 11 bits
    DS18B20_Resolution_12bits = 12  // Résolution de 12 bits
} DS18B20_Resolution_t;

//
// Fonctions
//

/**
 * @brief Initialise les capteurs DS18B20 connectés au bus 1-Wire.
 */
void DS18B20_Init(void);

/**
 * @brief Configure la résolution d'un capteur DS18B20.
 * @param number Numéro du capteur (index dans le tableau des capteurs détectés).
 * @param resolution Résolution souhaitée (9, 10, 11 ou 12 bits).
 * @retval uint8_t Retourne 1 si la configuration a réussi, 0 sinon.
 */
uint8_t DS18B20_SetResolution(uint8_t number, DS18B20_Resolution_t resolution);

/**
 * @brief Démarre une conversion de température pour un capteur spécifique.
 * @param number Numéro du capteur (index dans le tableau des capteurs détectés).
 * @retval uint8_t Retourne 1 si la commande a été envoyée avec succès, 0 sinon.
 */
uint8_t DS18B20_Start(uint8_t number);

/**
 * @brief Démarre une conversion de température pour tous les capteurs connectés.
 */
void DS18B20_StartAll(void);

/**
 * @brief Lit la température d'un capteur spécifique.
 * @param number Numéro du capteur (index dans le tableau des capteurs détectés).
 * @param destination Pointeur où la température lue sera stockée.
 * @retval uint8_t Retourne 1 si la lecture a réussi, 0 sinon.
 */
uint8_t DS18B20_Read(uint8_t number, float* destination);

/**
 * @brief Lit les températures de tous les capteurs connectés.
 */
void DS18B20_ReadAll(void);

/**
 * @brief Vérifie si une adresse ROM correspond à un capteur DS18B20.
 * @param ROM Adresse ROM à vérifier.
 * @retval uint8_t Retourne 1 si l'adresse correspond à un DS18B20, 0 sinon.
 */
uint8_t DS18B20_Is(uint8_t* ROM);

/**
 * @brief Vérifie si tous les capteurs ont terminé leur conversion.
 * @retval uint8_t Retourne 1 si tous les capteurs ont terminé, 0 sinon.
 */
uint8_t DS18B20_AllDone(void);

/**
 * @brief Récupère l'adresse ROM d'un capteur spécifique.
 * @param number Numéro du capteur (index dans le tableau des capteurs détectés).
 * @param ROM Pointeur où l'adresse ROM sera stockée.
 */
void DS18B20_GetROM(uint8_t number, uint8_t* ROM);

/**
 * @brief Écrit une nouvelle adresse ROM pour un capteur spécifique.
 * @param number Numéro du capteur (index dans le tableau des capteurs détectés).
 * @param ROM Nouvelle adresse ROM à écrire.
 */
void DS18B20_WriteROM(uint8_t number, uint8_t* ROM);

/**
 * @brief Retourne le nombre de capteurs DS18B20 détectés sur le bus.
 * @retval uint8_t Nombre de capteurs détectés.
 */
uint8_t DS18B20_Quantity(void);

/**
 * @brief Lit la température d'un capteur spécifique et la retourne.
 * @param number Numéro du capteur (index dans le tableau des capteurs détectés).
 * @param destination Pointeur où la température lue sera stockée.
 * @retval uint8_t Retourne 1 si la lecture a réussi, 0 sinon.
 */
uint8_t DS18B20_GetTemperature(uint8_t number, float* destination);

/**
 * @brief Calcule le délai nécessaire pour une conversion en fonction de la résolution.
 * @param resolution Résolution du capteur (9, 10, 11 ou 12 bits).
 * @retval uint16_t Délai en millisecondes.
 */
uint16_t DS18B20_GetConversionDelay(DS18B20_Resolution_t resolution);

#endif // _DS18B20_H
