#include "ds18b20.h"

// Variables globales
Ds18b20Sensor_t ds18b20[_DS18B20_MAX_SENSORS]; // Tableau pour stocker les capteurs détectés
OneWire_t OneWire;                             // Structure pour gérer le bus 1-Wire
uint8_t OneWireDevices;                        // Nombre total de périphériques détectés sur le bus
uint8_t TempSensorCount = 0;                   // Nombre de capteurs DS18B20 détectés

//
// Fonctions
//

/**
 * @brief Démarre une conversion de température pour un capteur spécifique.
 * @param number Numéro du capteur (index dans le tableau des capteurs détectés).
 * @retval uint8_t Retourne 1 si la commande a été envoyée avec succès, 0 sinon.
 */
uint8_t DS18B20_Start(uint8_t number) {
    if (number >= TempSensorCount) // Vérifie si le numéro du capteur est valide
        return 0;

    if (!DS18B20_Is((uint8_t*)&ds18b20[number].Address)) // Vérifie si l'adresse ROM correspond à un DS18B20
        return 0;

    OneWire_Reset(&OneWire); // Réinitialise le bus 1-Wire
    OneWire_SelectWithPointer(&OneWire, (uint8_t*)ds18b20[number].Address); // Sélectionne le capteur
    OneWire_WriteByte(&OneWire, DS18B20_CMD_CONVERTTEMP); // Envoie la commande pour démarrer la conversion

    return 1; // Conversion démarrée avec succès
}

/**
 * @brief Démarre une conversion de température pour tous les capteurs connectés.
 */
void DS18B20_StartAll() {
    OneWire_Reset(&OneWire); // Réinitialise le bus 1-Wire
    OneWire_WriteByte(&OneWire, ONEWIRE_CMD_SKIPROM); // Ignore les adresses ROM (commande pour tous les capteurs)
    OneWire_WriteByte(&OneWire, DS18B20_CMD_CONVERTTEMP); // Envoie la commande pour démarrer la conversion
}

/**
 * @brief Lit la température d'un capteur spécifique.
 * @param number Numéro du capteur (index dans le tableau des capteurs détectés).
 * @param destination Pointeur où la température lue sera stockée.
 * @retval uint8_t Retourne 1 si la lecture a réussi, 0 sinon.
 */
uint8_t DS18B20_Read(uint8_t number, float* destination) {
    uint8_t data[9]; // Tableau pour stocker les données lues depuis le scratchpad
    int16_t raw_temp; // Température brute lue depuis le capteur

    // Sélectionner le capteur
    OneWire_Reset(&OneWire);
    OneWire_SelectWithPointer(&OneWire, (uint8_t*)&ds18b20[number].Address);
    OneWire_WriteByte(&OneWire, ONEWIRE_CMD_RSCRATCHPAD); // Commande pour lire le scratchpad

    // Lire les 9 octets du scratchpad
    for (uint8_t i = 0; i < 9; i++) {
        data[i] = OneWire_ReadByte(&OneWire);
    }

    // Vérification CRC (si activé)
#ifdef _DS18B20_USE_CRC
    uint8_t crc = OneWire_CRC8(data, 8); // Calculer le CRC sur les 8 premiers octets
    if (crc != data[8]) { // Comparer avec le 9e octet (CRC attendu)
        return 0; // Erreur CRC
    }
#endif

    // Convertir les données brutes en température
    raw_temp = (data[1] << 8) | data[0]; // Combine les octets pour obtenir la température brute
    *destination = raw_temp * DS18B20_STEP_12BIT; // Conversion en °C

    // Vérifiez si la température est dans la plage valide
    if (*destination < -55.0 || *destination > 125.0) {
        return 0; // Température hors plage
    }

    return 1; // Lecture réussie
}

/**
 * @brief Vérifie si une adresse ROM correspond à un capteur DS18B20.
 * @param ROM Adresse ROM à vérifier.
 * @retval uint8_t Retourne 1 si l'adresse correspond à un DS18B20, 0 sinon.
 */
uint8_t DS18B20_Is(uint8_t* ROM) {
    return (ROM[0] == DS18B20_FAMILY_CODE); // Vérifie le code de famille
}

/**
 * @brief Initialise les capteurs DS18B20 connectés au bus 1-Wire.
 */
void DS18B20_Init(void) {
    uint8_t next = 0, i = 0, j;

    // Initialise le bus 1-Wire
    OneWire_Init(&OneWire);

    TempSensorCount = 0; // Réinitialise le compteur de capteurs
    next = OneWire_First(&OneWire); // Recherche le premier périphérique sur le bus
    while (next) {
        TempSensorCount++; // Incrémente le compteur de capteurs
        OneWire_GetFullROM(&OneWire, (uint8_t*)&ds18b20[i++].Address); // Stocke l'adresse ROM du capteur
        next = OneWire_Next(&OneWire); // Recherche le périphérique suivant
        if (TempSensorCount >= DS18B20_MAX_SENSORS) // Limite au nombre maximal de capteurs
            break;
    }

    // Configure la résolution pour tous les capteurs détectés
    for (j = 0; j < TempSensorCount; j++) {
        DS18B20_SetResolution(j, DS18B20_DEFAULT_RESOLUTION);
    }
}

/**
 * @brief Configure la résolution d'un capteur DS18B20.
 * @param number Numéro du capteur (index dans le tableau des capteurs détectés).
 * @param resolution Résolution souhaitée (9, 10, 11 ou 12 bits).
 * @retval uint8_t Retourne 1 si la configuration a réussi, 0 sinon.
 */
uint8_t DS18B20_SetResolution(uint8_t number, DS18B20_Resolution_t resolution) {
    if (number >= TempSensorCount) {
        return 0; // Numéro de capteur invalide
    }

    OneWire_Reset(&OneWire); // Réinitialise le bus 1-Wire
    OneWire_SelectWithPointer(&OneWire, (uint8_t*)&ds18b20[number].Address); // Sélectionne le capteur
    OneWire_WriteByte(&OneWire, ONEWIRE_CMD_WSCRATCHPAD); // Commande pour écrire dans le scratchpad

    // Écrit les octets de configuration
    OneWire_WriteByte(&OneWire, 0); // TH register
    OneWire_WriteByte(&OneWire, 0); // TL register
    OneWire_WriteByte(&OneWire, (resolution - 9) << 5); // Configuration register

    return 1; // Succès
}

/**
 * @brief Retourne le nombre de capteurs DS18B20 détectés sur le bus.
 * @retval uint8_t Nombre de capteurs détectés.
 */
uint8_t DS18B20_Quantity(void) {
    return TempSensorCount; // Retourne le nombre de capteurs détectés
}

/**
 * @brief Vérifie si tous les capteurs ont terminé leur conversion.
 * @retval uint8_t Retourne 1 si tous les capteurs ont terminé, 0 sinon.
 */
uint8_t DS18B20_AllDone(void) {
    // Vérifie si tous les capteurs ont terminé leur conversion
    OneWire_Reset(&OneWire);
    OneWire_WriteByte(&OneWire, ONEWIRE_CMD_SKIPROM);
    OneWire_WriteByte(&OneWire, ONEWIRE_CMD_RPWRSUPPLY);
    return HAL_GPIO_ReadPin(ONEWIRE_GPIO_PORT, ONEWIRE_GPIO_PIN);
}

/**
 * @brief Calcule le délai nécessaire pour une conversion en fonction de la résolution.
 * @param resolution Résolution du capteur (9, 10, 11 ou 12 bits).
 * @retval uint16_t Délai en millisecondes.
 */
uint16_t DS18B20_GetConversionDelay(DS18B20_Resolution_t resolution) {
    switch (resolution) {
        case DS18B20_Resolution_9bits:
            return 94; // Délai en millisecondes pour 9 bits
        case DS18B20_Resolution_10bits:
            return 188; // Délai en millisecondes pour 10 bits
        case DS18B20_Resolution_11bits:
            return 375; // Délai en millisecondes pour 11 bits
        case DS18B20_Resolution_12bits:
            return 750; // Délai en millisecondes pour 12 bits
        default:
            return 750; // Valeur par défaut (résolution maximale)
    }
}
