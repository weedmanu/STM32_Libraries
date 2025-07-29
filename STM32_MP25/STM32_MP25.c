/**
 * @file STM32_MP25.c
 * @brief Implémentation de la librairie PM2.5 (DFRobot SEN0177) pour STM32 (HAL, DMA, UART)
 *
 * Fournit l'acquisition, le parsing, l'interprétation et l'affichage des mesures PM1.0/2.5/10,
 * avec gestion DMA, callback utilisateur, et API modulaire.
 *
 * (c) M@nu, 2025. Licence Libre.
 */

#include "STM32_MP25.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

// -----------------------------------------------------------------------------
// Variables statiques internes
// -----------------------------------------------------------------------------

/**
 * @brief Callback utilisateur appelé à chaque trame valide (NULL si désactivé)
 */
static PM25_UserCallback pm25_user_cb = NULL;

// -----------------------------------------------------------------------------
// API publique
// -----------------------------------------------------------------------------

/**
 * @brief Enregistre un callback utilisateur (NULL pour désactiver)
 */
void PM25_RegisterCallback(PM25_UserCallback cb) {
    pm25_user_cb = cb;
}

/**
 * @brief Initialise la réception DMA du capteur PM2.5
 */
void PM25_init(UART_HandleTypeDef *huart_PM25, uint8_t *dma_rx_buf, uint16_t dma_buf_size)
{
    // Flush du buffer DMA avant démarrage
    memset(dma_rx_buf, 0, dma_buf_size);
    HAL_UART_Receive_DMA(huart_PM25, dma_rx_buf, dma_buf_size);
    printf("Init capteur PM2.5 DMA sur UART1, buf=%p, size=%u\r\n", dma_rx_buf, dma_buf_size);
}

/**
 * @brief Retourne l'interprétation d'une valeur PM (indice, emoji, label, texte)
 */
PM_StatusInfo PM_Get_Status(uint16_t pm, const char *type) {
    if (strcmp(type, "PM2.5") == 0) {
        if (pm <= 10)      return (PM_StatusInfo){1, "🟢", "Très bon", "Air pur, aucun risque."};
        else if (pm <= 20) return (PM_StatusInfo){2, "🟢", "Bon", "Air sain, très faible risque."};
        else if (pm <= 25) return (PM_StatusInfo){3, "🟡", "Moyen", "Qualité correcte, attention sensibles."};
        else if (pm <= 50) return (PM_StatusInfo){4, "🟠", "Médiocre", "Limitez l'exposition prolongée."};
        else if (pm <= 75) return (PM_StatusInfo){5, "🟠", "Très mauvais", "Évitez l'exposition, risque accru."};
        else               return (PM_StatusInfo){6, "🔴", "Extrêmement mauvais", "Danger, évacuation recommandée !"};
    } else if (strcmp(type, "PM10") == 0) {
        if (pm <= 20)      return (PM_StatusInfo){1, "🟢", "Très bon", "Air pur, aucun risque."};
        else if (pm <= 40) return (PM_StatusInfo){2, "🟢", "Bon", "Air sain, très faible risque."};
        else if (pm <= 50) return (PM_StatusInfo){3, "🟡", "Moyen", "Qualité correcte, attention sensibles."};
        else if (pm <= 100) return (PM_StatusInfo){4, "🟠", "Médiocre", "Limitez l'exposition prolongée."};
        else if (pm <= 150) return (PM_StatusInfo){5, "🟠", "Très mauvais", "Évitez l'exposition, risque accru."};
        else               return (PM_StatusInfo){6, "🔴", "Extrêmement mauvais", "Danger, évacuation recommandée !"};
    } else { // PM1.0 ou inconnu
        if (pm <= 10)      return (PM_StatusInfo){1, "🟢", "Très bon", "Air pur, aucun risque."};
        else if (pm <= 20) return (PM_StatusInfo){2, "🟢", "Bon", "Air sain, très faible risque."};
        else if (pm <= 25) return (PM_StatusInfo){3, "🟡", "Moyen", "Qualité correcte, attention sensibles."};
        else if (pm <= 50) return (PM_StatusInfo){4, "🟠", "Médiocre", "Limitez l'exposition prolongée."};
        else if (pm <= 75) return (PM_StatusInfo){5, "🟠", "Très mauvais", "Évitez l'exposition, risque accru."};
        else               return (PM_StatusInfo){6, "🔴", "Extrêmement mauvais", "Danger, évacuation recommandée !"};
    }
}

/**
 * @brief Vérifie l'en-tête d'une trame (0x42 0x4D)
 */
int PM25_CheckHeader(const uint8_t *frame) {
    return (frame[0] == 0x42 && frame[1] == 0x4D);
}

/**
 * @brief Vérifie le checksum d'une trame
 */
int PM25_CheckChecksum(const uint8_t *frame) {
    uint16_t sum = 0;
    for (int i = 0; i < 30; i++)
        sum += frame[i];
    uint16_t checksum = (frame[30] << 8) | frame[31];
    return (sum == checksum);
}

/**
 * @brief Extrait les valeurs PM1.0, PM2.5, PM10 d'une trame
 */
void PM25_ExtractData(const uint8_t *frame, PM25_Data *data) {
    data->pm1_0 = (frame[10] << 8) | frame[11];
    data->pm2_5 = (frame[12] << 8) | frame[13];
    data->pm10  = (frame[14] << 8) | frame[15];
}

/**
 * @brief Affiche les valeurs et l'interprétation dans le terminal
 */
void PM25_PrintStatus(const PM25_Data *data) {
    PM_StatusInfo s1 = PM_Get_Status(data->pm1_0, "PM1.0");
    PM_StatusInfo s25 = PM_Get_Status(data->pm2_5, "PM2.5");
    PM_StatusInfo s10 = PM_Get_Status(data->pm10, "PM10");

    printf("\r\n=== MESURES PM2.5 (SEN0177) ===\r\n\r\n");
    printf("│ PM1.0 : %3u ug/m3 │ Indice %d │ %s %-18s│ %s\r\n", data->pm1_0, s1.index, s1.emoji, s1.label, s1.texte);
    printf("│ PM2.5 : %3u ug/m3 │ Indice %d │ %s %-18s│ %s\r\n", data->pm2_5, s25.index, s25.emoji, s25.label, s25.texte);
    printf("│ PM10  : %3u ug/m3 │ Indice %d │ %s %-18s│ %s\r\n", data->pm10, s10.index, s10.emoji, s10.label, s10.texte);
    printf("\r\n");

    int index_max = s25.index > s10.index ? s25.index : s10.index;
    const char *emoji, *label, *msg;
    if (index_max == 1) {
        emoji = "🟢"; label = "Très bon"; msg = "Air pur, aucun risque.";
    } else if (index_max == 2) {
        emoji = "🟢"; label = "Bon"; msg = "Air sain, très faible risque.";
    } else if (index_max == 3) {
        emoji = "🟡"; label = "Moyen"; msg = "Qualité correcte, attention sensibles.";
    } else if (index_max == 4) {
        emoji = "🟠"; label = "Médiocre"; msg = "Limitez l'exposition prolongée.";
    } else if (index_max == 5) {
        emoji = "🟠"; label = "Très mauvais"; msg = "Évitez l'exposition, risque accru.";
    } else {
        emoji = "🔴"; label = "Extrêmement mauvais"; msg = "Danger, évacuation recommandée !";
    }
    printf("Résumé global : Indice %d │ %s %-18s│ %s\r\n\r\n", index_max, emoji, label, msg);
}

/**
 * @brief Parse une trame complète, extrait les valeurs, appelle le callback si défini
 */
int PM25_ParseFrame(const uint8_t *frame, PM25_Data *data) {
    if (!PM25_CheckHeader(frame)) {
        return 0;
    }
    if (!PM25_CheckChecksum(frame)) {
        return 0;
    }
    PM25_ExtractData(frame, data);
    // Appel du callback utilisateur si défini
    if (pm25_user_cb) {
        pm25_user_cb(data);
    }
    return 1;
}

/**
 * @brief Fonction utilitaire à appeler dans la boucle principale pour gérer le buffer DMA et le parsing
 */
void PM25_Loop(volatile uint8_t *data_ready_flag, UART_HandleTypeDef *huart, uint8_t *dma_buf, uint16_t dma_buf_size) {
    if (*data_ready_flag) {
        *data_ready_flag = 0;
        PM25_Data data;
        if (!PM25_ParseFrame(dma_buf, &data)) {
            printf("Trame PM2.5 invalide !\r\n");
        }
        HAL_UART_Receive_DMA(huart, dma_buf, dma_buf_size);
    }
}