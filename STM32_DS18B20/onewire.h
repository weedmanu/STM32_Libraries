#ifndef ONEWIRE_H
#define ONEWIRE_H 

#include "config.h"

//
// Structure du bus 1-Wire
//
typedef struct {
    GPIO_TypeDef* GPIOx;           // Port GPIO utilisé pour le bus 1-Wire
    uint16_t GPIO_Pin;             // Broche GPIO utilisée pour le bus 1-Wire
    TIM_HandleTypeDef* Timer;      // Timer utilisé pour générer les délais 1-Wire
    uint8_t LastDiscrepancy;       // Dernière divergence utilisée pour la recherche de périphériques
    uint8_t LastFamilyDiscrepancy; // Dernière divergence de famille pour la recherche
    uint8_t LastDeviceFlag;        // Indique si le dernier périphérique a été trouvé
    uint8_t ROM_NO[8];             // Adresse ROM (8 octets) du dernier périphérique trouvé
} OneWire_t;

//
// Commandes 1-Wire
//
#define ONEWIRE_CMD_RSCRATCHPAD         0xBE // Lire le scratchpad
#define ONEWIRE_CMD_WSCRATCHPAD         0x4E // Écrire dans le scratchpad
#define ONEWIRE_CMD_CPYSCRATCHPAD       0x48 // Copier le scratchpad dans l'EEPROM
#define ONEWIRE_CMD_RECEEPROM           0xB8 // Lire l'EEPROM
#define ONEWIRE_CMD_RPWRSUPPLY          0xB4 // Vérifier si l'alimentation parasite est utilisée
#define ONEWIRE_CMD_SEARCHROM           0xF0 // Rechercher les périphériques sur le bus
#define ONEWIRE_CMD_READROM             0x33 // Lire l'adresse ROM d'un périphérique
#define ONEWIRE_CMD_MATCHROM            0x55 // Sélectionner un périphérique spécifique par son adresse ROM
#define ONEWIRE_CMD_SKIPROM             0xCC // Ignorer l'adresse ROM (s'applique à tous les périphériques)

//
// Fonctions
//

//
// Initialisation
//
void OneWire_Init(OneWire_t* OneWireStruct); // Initialise le bus 1-Wire

//
// Réinitialisation du bus
//
uint8_t OneWire_Reset(OneWire_t* OneWireStruct); // Réinitialise le bus 1-Wire

//
// Recherche de périphériques
//
void OneWire_ResetSearch(OneWire_t* OneWireStruct); // Réinitialise les variables de recherche
uint8_t OneWire_First(OneWire_t* OneWireStruct);    // Recherche le premier périphérique sur le bus
uint8_t OneWire_Next(OneWire_t* OneWireStruct);     // Recherche le prochain périphérique sur le bus
uint8_t OneWire_Search(OneWire_t* OneWireStruct, uint8_t command); // Recherche un périphérique avec une commande spécifique

//
// Écriture/Lecture
//
void OneWire_WriteBit(OneWire_t* OneWireStruct, uint8_t bit); // Écrit un bit sur le bus 1-Wire
uint8_t OneWire_ReadBit(OneWire_t* OneWireStruct);            // Lit un bit depuis le bus 1-Wire
void OneWire_WriteByte(OneWire_t* OneWireStruct, uint8_t byte); // Écrit un octet sur le bus 1-Wire
uint8_t OneWire_ReadByte(OneWire_t* OneWireStruct);           // Lit un octet depuis le bus 1-Wire

//
// Opérations sur les adresses ROM
//
void OneWire_GetFullROM(OneWire_t* OneWireStruct, uint8_t *firstIndex); // Récupère l'adresse ROM complète
void OneWire_Select(OneWire_t* OneWireStruct, uint8_t* addr);           // Sélectionne un périphérique par son adresse ROM
void OneWire_SelectWithPointer(OneWire_t* OneWireStruct, uint8_t* ROM); // Sélectionne un périphérique avec un pointeur vers son adresse ROM

//
// Calcul du CRC
//
uint8_t OneWire_CRC8(uint8_t* addr, uint8_t len); // Calcule le CRC8 pour vérifier l'intégrité des données

#endif

