/*
 * STM32_SD_SPI.h
 *
 *  Created on: Apr 7, 2025
 *      Author: weedm
 */

#ifndef INC_STM32_SD_SPI_H_
#define INC_STM32_SD_SPI_H_

#include "fatfs.h" // Inclure FatFs pour les types FATFS, FIL, FRESULT, etc.
#include <stdio.h> // Pour printf
#include <string.h> // Pour les fonctions de manipulation de chaînes
#include "TM32_SD_SPI_config.h"

/* Définitions pour les commandes MMC/SDC */
#define CMD0     (0x40+0)       /* GO_IDLE_STATE */
#define CMD1     (0x40+1)       /* SEND_OP_COND */
#define CMD8     (0x40+8)       /* SEND_IF_COND */
#define CMD9     (0x40+9)       /* SEND_CSD */
#define CMD10    (0x40+10)      /* SEND_CID */
#define CMD12    (0x40+12)      /* STOP_TRANSMISSION */
#define CMD16    (0x40+16)      /* SET_BLOCKLEN */
#define CMD17    (0x40+17)      /* READ_SINGLE_BLOCK */
#define CMD18    (0x40+18)      /* READ_MULTIPLE_BLOCK */
#define CMD23    (0x40+23)      /* SET_BLOCK_COUNT */
#define CMD24    (0x40+24)      /* WRITE_BLOCK */
#define CMD25    (0x40+25)      /* WRITE_MULTIPLE_BLOCK */
#define CMD41    (0x40+41)      /* SEND_OP_COND (ACMD) */
#define CMD55    (0x40+55)      /* APP_CMD */
#define CMD58    (0x40+58)      /* READ_OCR */

/* Indicateurs de type de carte MMC (MMC_GET_TYPE) */
#define CT_MMC    0x01    /* MMC ver 3 */
#define CT_SD1    0x02    /* SD ver 1 */
#define CT_SD2    0x04    /* SD ver 2 */
#define CT_SDC    0x06    /* SD */
#define CT_BLOCK  0x08    /* Adressage par bloc */

/* Indicateurs de statut de carte */
#define SPI_DUMMY_BYTE 0xFF          // Octet vide pour SPI
#define SD_TIMEOUT_INIT 0x1FFF       // Timeout pour l'initialisation
#define SD_TIMEOUT_READY 500         // Timeout pour attendre que la carte soit prête (ms)
#define SD_TIMEOUT_BLOCK 200         // Timeout pour la réception d'un bloc (ms)
#define CMD0_CRC 0x95                // CRC pour la commande CMD0
#define CMD8_CRC 0x87                // CRC pour la commande CMD8
#define BLOCK_SIZE 512               // Taille d'un bloc
#define SD_READY_TOKEN 0xFE          // Jeton indiquant que la carte SD est prête
#define SD_ACCEPTED 0x05             // Réponse acceptée
#define SD_STOP_TRANSMISSION 0xFD    // Jeton STOP_TRANSMISSION
#define SD_MULTIPLE_WRITE 0xFC       // Jeton pour écriture multiple

#define FF_MAX_SS 4096 // Taille maximale de secteur (en octets)

/* Fonctions de gestion de la carte SD */
DSTATUS SD_disk_initialize (BYTE pdrv); // Initialise le disque SD (pdrv est le numéro de lecteur logique)
DSTATUS SD_disk_status (BYTE pdrv); // Retourne le statut du disque SD (pdrv est le numéro de lecteur logique)
DRESULT SD_disk_read (BYTE pdrv, BYTE* buff, DWORD sector, UINT count); // Lit des secteurs du disque SD (pdrv est le numéro de lecteur logique, buff est le tampon de données, sector est le numéro de secteur, count est le nombre de secteurs à lire)
DRESULT SD_disk_write (BYTE pdrv, const BYTE* buff, DWORD sector, UINT count); // Écrit des secteurs sur le disque SD (pdrv est le numéro de lecteur logique, buff est le tampon de données, sector est le numéro de secteur, count est le nombre de secteurs à écrire)
DRESULT SD_disk_ioctl (BYTE pdrv, BYTE cmd, void* buff); // Effectue des opérations de contrôle sur le disque SD (pdrv est le numéro de lecteur logique, cmd est la commande, buff est un tampon pour les données de commande)

// Fonction pour utiliser la carte SD
DRESULT SD_Monter(void); // Monter la carte SD
void SD_Demonter(void); // Démonter la carte SD
DRESULT SD_LireEspace(uint32_t *espaceTotal, uint32_t *espaceLibre); // Lire l'espace total et libre
DRESULT SD_EcrireFichier(const char *nomFichier, const char *donnees); // Écrire dans un fichier
DRESULT SD_LireFichier(const char *nomFichier, char *tampon, size_t tailleTampon); // Lire depuis un fichier
DRESULT SD_ListerFichiers(const char *chemin); // Lister les fichiers d'un répertoire
DRESULT SD_SupprimerFichier(const char *nomFichier); // Formater la carte SD

#endif /* INC_STM32_SD_SPI_H_ */
