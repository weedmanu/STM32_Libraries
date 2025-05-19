/*
 * STM32_SD_SPI.h
 *
 *  Created on: Apr 7, 2025
 *      Author: weedm
 *  Description: Header file for the STM32 SD card driver using SPI.
 *               Provides low-level disk I/O interface for FatFs
 *               and higher-level utility functions.
 */

#ifndef INC_STM32_SD_SPI_H_
#define INC_STM32_SD_SPI_H_

#include "diskio.h" // FatFs Disk I/O layer
#include "ff.h"     // FatFs file system layer
#include "ffconf.h" // FatFs configuration
#include <string.h>
#include "STM32_SD_SPI_config.h" // Driver specific configuration

// --- Low-Level Driver Configuration (from STM32_SD_SPI_config.h) ---
// SD_CS_PORT, SD_CS_PIN, HSPI_SDCARD, SPI_TIMEOUT, ENABLE_DEBUG, DEBUG_PRINTF

// --- SD Card Commands (CMD) ---
#define CMD0 (0x40 + 0)   // GO_IDLE_STATE
#define CMD1 (0x40 + 1)   // SEND_OP_COND (MMC)
#define CMD8 (0x40 + 8)   // SEND_IF_COND (SDv2)
#define CMD9 (0x40 + 9)   // SEND_CSD
#define CMD10 (0x40 + 10) // SEND_CID
#define CMD12 (0x40 + 12) // STOP_TRANSMISSION
#define CMD16 (0x40 + 16) // SET_BLOCKLEN
#define CMD17 (0x40 + 17) // READ_SINGLE_BLOCK
#define CMD18 (0x40 + 18) // READ_MULTIPLE_BLOCK
#define CMD23 (0x40 + 23) // SET_BLOCK_COUNT (MMC) / SET_WR_BLK_ERASE_COUNT (SDv1)
#define CMD24 (0x40 + 24) // WRITE_BLOCK
#define CMD25 (0x40 + 25) // WRITE_MULTIPLE_BLOCK
#define CMD41 (0x40 + 41) // SEND_OP_COND (SDC) - ACMD
#define CMD55 (0x40 + 55) // APP_CMD
#define CMD58 (0x40 + 58) // READ_OCR

// --- Command CRCs ---
#define CMD0_CRC 0x94 // CRC for CMD0 with argument 0
#define CMD8_CRC 0x87 // CRC for CMD8 with argument 0x1AA

// --- Data Tokens ---
#define SD_READY_TOKEN 0xFE       // Data start token for single/multiple block read/write
#define SD_MULTIPLE_WRITE 0xFC    // Data start token for multiple block write
#define SD_STOP_TRANSMISSION 0xFD // Stop transmission token for multiple block write

// --- Response Tokens ---
#define SD_ACCEPTED 0x05    // Data accepted response
#define SD_CRC_ERROR 0x0B   // Data rejected due to CRC error
#define SD_WRITE_ERROR 0x0D // Data rejected due to write error

// --- Dummy Byte ---
#define SPI_DUMMY_BYTE 0xFF // Dummy byte to send for receiving data

// --- Timeouts (in milliseconds) ---
#define SD_TIMEOUT_READY 500   // Timeout for waiting card to be ready (e.g., after write)
#define SD_TIMEOUT_BLOCK 100   // Timeout for waiting for a data block token
#define SD_TIMEOUT_INIT 2000   // Timeout for card initialization (CMD0, ACMD41/CMD1)
#define SD_TIMEOUT_CMD_RESP 10 // Timeout for waiting for a command response (short)
#define SD_TIMEOUT_BUSY 500    // Timeout for waiting for the card to become not busy after write

// --- Block Size ---
#define BLOCK_SIZE 512 // Standard block size for SD cards

// --- Card Types ---
#define CT_MMC 0x01   // MMC ver 3
#define CT_SD1 0x02   // SD ver 1
#define CT_SD2 0x04   // SD ver 2
#define CT_BLOCK 0x08 // Block addressing

// --- Disk Status Flags (from diskio.h) ---
// STA_NOINIT, STA_NODISK, STA_PROTECT

// --- Disk Result Codes (from diskio.h) ---
// RES_OK, RES_ERROR, RES_WRPRT, RES_NOTRDY, RES_PARERR

// --- External variables ---
extern SPI_HandleTypeDef hspi2; // Declare the SPI handle used by the driver

// --- Disk I/O Interface Functions (Implemented in STM32_SD_SPI.c) ---

DSTATUS SD_disk_initialize(BYTE drv);
DSTATUS SD_disk_status(BYTE drv);
DRESULT SD_disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
#if _USE_WRITE == 1
DRESULT SD_disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
#endif
#if _USE_IOCTL == 1
DRESULT SD_disk_ioctl(BYTE drv, BYTE ctrl, void *buff);
#endif

// --- Fonctions de gestion de la carte SD (Implémentées dans STM32_SD_SPI.c) ---

DRESULT SD_Monter(void);
DRESULT SD_Demonter(void);
DRESULT SD_LireEspace(uint64_t *espaceTotal, uint64_t *espaceLibre);
DRESULT SD_FormaterCarte(void); // Attention : Opération destructive ! Nécessite _USE_MKFS = 1 dans ffconf.h

// --- Fonctions de gestion de fichiers (Implémentées dans STM32_SD_SPI.c) ---

DRESULT SD_Creer(const char *chemin, const char *contenu);
DRESULT SD_Supprimer(const char *chemin);
DRESULT SD_Copier(const char *source, const char *destination);
DRESULT SD_Deplacer(const char *source, const char *destination);
DRESULT SD_Existe(const char *chemin, BYTE *pbExiste);
DRESULT SD_Lister(const char *chemin);
DRESULT SD_Lire(const char *chemin, char *tampon, size_t tailleTampon);
DRESULT SD_Ecrire(const char *chemin, const char *donnees);

// --- Fonctions de gestion des répertoires (Implémentées dans STM32_SD_SPI.c) ---

DRESULT SD_ListerFichiers(const char *chemin);
DRESULT SD_SupprimerRepertoire(const char *nomRepertoire);
DRESULT SD_CreerRepertoire(const char *nomRepertoire);

// --- Fonctions utilitaires (Implémentées dans STM32_SD_SPI.c) ---

DRESULT SD_VerifierExistence(const char *nomChemin, BYTE *pbExiste);
DRESULT SD_VerifierEtat(void);
DRESULT SD_ObtenirInfos(char *infoBuffer, size_t bufferSize);
DRESULT SD_VerifierRepertoireVide(const char *nomRepertoire, BYTE *estVide);
DRESULT SD_TesterVitesse(uint32_t *vitesseLecture, uint32_t *vitesseEcriture);

#endif /* INC_STM32_SD_SPI_H_ */
