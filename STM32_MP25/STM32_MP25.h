/**
 * @file STM32_MP25.h
 * @brief Librairie pour capteur PM2.5 (DFRobot SEN0177) sur STM32 (HAL, DMA, UART)
 *
 * Fournit une API simple pour acquérir, parser et interpréter les mesures PM1.0/2.5/10,
 * avec gestion DMA, callback utilisateur, et affichage terminal.
 *
 * (c) M@nu, 2025. Licence libre.
 */

#ifndef STM32_MP25_H
#define STM32_MP25_H

#include <stdint.h>
#include <stddef.h>
#include "stm32l4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @def PM25_FRAME_LEN
 * @brief Taille d'une trame brute du capteur (octets)
 */
#define PM25_FRAME_LEN 32

/**
 * @brief Structure contenant les valeurs PM1.0, PM2.5, PM10 (en ug/m3)
 */
typedef struct {
    uint16_t pm1_0;
    uint16_t pm2_5;
    uint16_t pm10;
} PM25_Data;

/**
 * @brief Structure d'interprétation d'une valeur PM (indice, emoji, label, texte)
 */
typedef struct {
    int index;
    const char *emoji;
    const char *label;
    const char *texte;
} PM_StatusInfo;

/**
 * @brief Prototype de callback utilisateur appelé à chaque trame valide
 * @param data Pointeur vers les valeurs PM extraites
 */
typedef void (*PM25_UserCallback)(const PM25_Data *data);

/**
 * @brief Enregistre un callback utilisateur (NULL pour désactiver)
 * @param cb Fonction callback à appeler à chaque trame valide
 */
void PM25_RegisterCallback(PM25_UserCallback cb);

/**
 * @brief Initialise la réception DMA du capteur PM2.5
 * @param huart_PM25 UART à utiliser (ex: &huart1)
 * @param dma_rx_buf Buffer DMA de réception (taille PM25_FRAME_LEN)
 * @param dma_buf_size Taille du buffer DMA (doit valoir PM25_FRAME_LEN)
 */
void PM25_init(UART_HandleTypeDef *huart_PM25, uint8_t *dma_rx_buf, uint16_t dma_buf_size);

/**
 * @brief Vérifie l'en-tête d'une trame (0x42 0x4D)
 * @param frame Trame brute reçue
 * @return 1 si header valide, 0 sinon
 */
int PM25_CheckHeader(const uint8_t *frame);

/**
 * @brief Vérifie le checksum d'une trame
 * @param frame Trame brute reçue
 * @return 1 si checksum OK, 0 sinon
 */
int PM25_CheckChecksum(const uint8_t *frame);

/**
 * @brief Extrait les valeurs PM1.0, PM2.5, PM10 d'une trame
 * @param frame Trame brute reçue
 * @param data Structure de sortie
 */
void PM25_ExtractData(const uint8_t *frame, PM25_Data *data);

/**
 * @brief Affiche les valeurs et l'interprétation dans le terminal
 * @param data Structure contenant les valeurs PM
 */
void PM25_PrintStatus(const PM25_Data *data);

/**
 * @brief Parse une trame complète, extrait les valeurs, appelle le callback si défini
 * @param frame Trame brute reçue
 * @param data Structure de sortie
 * @return 1 si trame valide, 0 sinon
 */
int PM25_ParseFrame(const uint8_t *frame, PM25_Data *data);

/**
 * @brief Retourne l'interprétation d'une valeur PM (indice, emoji, label, texte)
 * @param pm Valeur PM à interpréter
 * @param type "PM1.0", "PM2.5" ou "PM10"
 * @return Structure d'interprétation
 */
PM_StatusInfo PM_Get_Status(uint16_t pm, const char *type);

/**
 * @brief Fonction utilitaire à appeler dans la boucle principale pour gérer le buffer DMA et le parsing
 * @param data_ready_flag Pointeur sur le flag de trame prête (volatile)
 * @param huart UART utilisé pour la réception
 * @param dma_buf Buffer DMA de réception
 * @param dma_buf_size Taille du buffer DMA
 */
void PM25_Loop(volatile uint8_t *data_ready_flag, UART_HandleTypeDef *huart, uint8_t *dma_buf, uint16_t dma_buf_size);

#ifdef __cplusplus
}
#endif

#endif // STM32_MP25_H
