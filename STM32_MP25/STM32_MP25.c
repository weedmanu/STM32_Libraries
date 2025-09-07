/*
 * STM32_MP25.c
 *
 *  Created on: Sep 6, 2025
 *      Author: weedm
 */

// -----------------------------------------------------------------------------
//  INCLUDES
// -----------------------------------------------------------------------------
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "stm32l4xx_hal.h"
#include "STM32_MP25.h"

static int pm25_debug_mode = 0;

// -----------------------------------------------------------------------------
//  MACROS
// -----------------------------------------------------------------------------
#define PM25_DEBUG_PRINT(...)    \
    do                           \
    {                            \
        if (pm25_debug_mode)     \
            printf(__VA_ARGS__); \
    } while (0)

// -----------------------------------------------------------------------------
//  OPTIMISATION: Extraction des champs optimisée (évite répétitions)
// -----------------------------------------------------------------------------
static void pm25_extract_fields(PM25_FullData *data, const uint8_t *frame)
{
    // Copie optimisée du frame brut
    memcpy(data->raw_frame, frame, PM25_FRAME_LEN);

    // Extraction optimisée avec pointeur pour éviter calculs répétés
    const uint8_t *ptr = frame + 4; // Commencer après header+length

    // Utiliser une boucle pour les extractions répétitives
    uint16_t *fields[] = {
        &data->pm1_0_standard, &data->pm2_5_standard, &data->pm10_standard,
        &data->pm1_0_atmospheric, &data->pm2_5_atmospheric, &data->pm10_atmospheric,
        &data->particles_0_3um, &data->particles_0_5um, &data->particles_1_0um,
        &data->particles_2_5um, &data->particles_5_0um, &data->particles_10um};

    for (int i = 0; i < 12; i++)
    {
        *fields[i] = (ptr[0] << 8) | ptr[1];
        ptr += 2;
    }

    data->version = frame[28];
    data->checksum = (frame[30] << 8) | frame[31];
}

// -----------------------------------------------------------------------------
//  OPTIMISATION: Lecture UART optimisée (évite memmove répété)
// -----------------------------------------------------------------------------

// Variables globales pour la pin SET
static GPIO_TypeDef *pm25_set_port = NULL;
static uint16_t pm25_set_pin = 0;
// static UART_HandleTypeDef *pm25_uart_handle = NULL; // supprimé, non utilisé

void PM25_SetControlPin(GPIO_TypeDef *port, uint16_t pin)
{
    pm25_set_port = port;
    pm25_set_pin = pin;
}

// --- Async/DMA public flags used by IRQ handlers in project (exported symbols)
volatile uint8_t usart1_idle_flag = 0;
volatile unsigned long dma1_head_snapshot = 0;

// Async DMA internals
uint8_t async_buffer[PM25_FRAME_LEN * 2]; // circular DMA buffer

// -----------------------------------------------------------------------------
//  API Async / DMA minimal
// -----------------------------------------------------------------------------
void PM25_Init(UART_HandleTypeDef *huart)
{
    // backward-compatible initialization
    // Initialisation réelle à placer ici si besoin
    HAL_Delay(2000); // Attendre la stabilisation
    PM25_DEBUG_PRINT("[PM25 DEBUG] Init capteur PM2.5 terminée (capteur en veille)\n");
}

// -----------------------------------------------------------------------------
//  FONCTIONS D'INITIALISATION
// -----------------------------------------------------------------------------

void PM25_SetDebugMode(int enable)
{
    pm25_debug_mode = enable;
}

// -----------------------------------------------------------------------------
//  OPTIMISATION: Mode périodique ultra-efficace (meilleure économie CPU)
// -----------------------------------------------------------------------------
int PM25_Polling_ReadFull(UART_HandleTypeDef *huart, PM25_FullData *data, unsigned long interval_ms)
{
    static unsigned long last_reading_time = 0;
    static uint8_t buffer[PM25_FRAME_LEN * 2]; // Buffer circulaire statique
    static int head = 0;
    static unsigned long activation_time = 0;
    static int state = 0; // 0: veille, 1: activation, 2: lecture

    unsigned long current_time = HAL_GetTick();

    switch (state)
    {
    case 0: // Mode veille
        if ((current_time - last_reading_time) >= interval_ms)
        {
            // Activer le capteur
            HAL_GPIO_WritePin(pm25_set_port, pm25_set_pin, GPIO_PIN_SET);
            activation_time = current_time;
            state = 1;
            PM25_DEBUG_PRINT("[PM25 DEBUG] SET -> HIGH (activation optimisée)\n");
            return -1; // En cours d'activation
        }
        return -1; // Toujours en veille

    case 1: // Attente activation capteur
        if ((current_time - activation_time) >= 1000)
        { // 1s pour activation
            state = 2;
            head = 0; // Reset buffer
            PM25_DEBUG_PRINT("[PM25 DEBUG] Capteur prêt, début lecture optimisée\n");
        }
        return -1; // Toujours en activation

    case 2: // Lecture rapide
        // Essayer de lire un octet (timeout court pour ne pas bloquer)
        uint8_t b;
        HAL_StatusTypeDef status = HAL_UART_Receive(huart, &b, 1, 10); // 10ms timeout

        if (status == HAL_OK)
        {
            // Ajouter l'octet au buffer circulaire
            buffer[head] = b;
            head = (head + 1) % (PM25_FRAME_LEN * 2);

            // Vérifier si on a assez de données pour une trame complète
            if (head >= PM25_FRAME_LEN)
            {
                int window_start = (head - PM25_FRAME_LEN + (PM25_FRAME_LEN * 2)) % (PM25_FRAME_LEN * 2);

                // Vérifier le header
                if (buffer[window_start] == 0x42 && buffer[(window_start + 1) % (PM25_FRAME_LEN * 2)] == 0x4D)
                {
                    // Extraire la trame
                    uint8_t frame[PM25_FRAME_LEN];
                    for (int i = 0; i < PM25_FRAME_LEN; i++)
                    {
                        frame[i] = buffer[(window_start + i) % (PM25_FRAME_LEN * 2)];
                    }

                    // Vérifier le checksum
                    if (PM25_Validate_Checksum(frame))
                    {
                        PM25_DEBUG_PRINT("[PM25 DEBUG] Trame valide trouvée (optimisée)\n");
                        pm25_extract_fields(data, frame);

                        // Remettre en veille
                        HAL_GPIO_WritePin(pm25_set_port, pm25_set_pin, GPIO_PIN_RESET);
                        last_reading_time = current_time;
                        state = 0;
                        PM25_DEBUG_PRINT("[PM25 DEBUG] SET -> LOW (retour veille optimisée)\n");
                        return 1; // Succès
                    }
                }
            }
            return -1; // Continue à lire
        }
        else
        {
            // Timeout de lecture, vérifier si on doit abandonner
            if ((current_time - activation_time) > 3000)
            { // 3s max pour lecture
                PM25_DEBUG_PRINT("[PM25 DEBUG] Timeout lecture optimisée\n");
                HAL_GPIO_WritePin(pm25_set_port, pm25_set_pin, GPIO_PIN_RESET);
                last_reading_time = current_time;
                state = 0;
                return 0; // Échec
            }
            return -1; // Continue d'essayer
        }
    }

    return -1; // État inconnu
}

/**
 * @brief Vérifie l'en-tête d'une trame (0x42 0x4D)
 * @param frame Trame brute reçue
 * @return 1 si header valide, 0 sinon
 */
int PM25_Validate_Header(const uint8_t *frame)
{
    int valid = (frame[0] == 0x42 && frame[1] == 0x4D);
    PM25_DEBUG_PRINT("[PM25 DEBUG] Header validation: 0x%02X 0x%02X = %s\n",
                     frame[0], frame[1], valid ? "OK" : "ERREUR");
    return valid;
}

/**
 * @brief Vérifie le checksum d'une trame
 * @param frame Trame brute reçue
 * @return 1 si checksum OK, 0 sinon
 */
int PM25_Validate_Checksum(const uint8_t *frame)
{
    uint16_t sum = 0;
    // Somme optimisée des 30 premiers octets
    for (int i = 0; i < 30; i++)
        sum += frame[i];
    uint16_t chk = (frame[30] << 8) | frame[31];
    PM25_DEBUG_PRINT("[PM25 DEBUG] Checksum calculé: 0x%04X, reçu: 0x%04X (pos 30-31)\n", sum, chk);
    return (sum == chk);
}

// -----------------------------------------------------------------------------
//  FONCTIONS D'INTERPRETATION QUALITE AIR
// -----------------------------------------------------------------------------
/**
 * @brief Calcule le code de qualité d'air basé sur la valeur PM et le type
 * @param pm Valeur PM mesurée
 * @param type Type de mesure ("PM1.0", "PM2.5", "PM10")
 * @return Code de qualité (1=Très bon, 2=Bon, ... 6=Très mauvais)
 */
int PM25_Quality_GetCode(uint16_t pm, const char *type)
{
    if (strcmp(type, "PM2.5") == 0)
    {
        if (pm <= 10)
            return 1;
        else if (pm <= 20)
            return 2;
        else if (pm <= 25)
            return 3;
        else if (pm <= 50)
            return 4;
        else if (pm <= 75)
            return 5;
        else
            return 6;
    }
    else if (strcmp(type, "PM10") == 0)
    {
        if (pm <= 20)
            return 1;
        else if (pm <= 40)
            return 2;
        else if (pm <= 50)
            return 3;
        else if (pm <= 100)
            return 4;
        else if (pm <= 150)
            return 5;
        else
            return 6;
    }
    else
    {
        if (pm <= 10)
            return 1;
        else if (pm <= 20)
            return 2;
        else if (pm <= 25)
            return 3;
        else if (pm <= 50)
            return 4;
        else if (pm <= 75)
            return 5;
        else
            return 6;
    }
}

/**
 * @brief Interprète un code de qualité en informations lisibles
 * @param code Code de qualité (1-6)
 * @return Structure contenant emoji, label et description
 */
PM_StatusInfo PM25_Quality_InterpretCode(int code)
{
    switch (code)
    {
    case 1:
        return (PM_StatusInfo){1, "🟢", "Très bon", "Qualité de l'air excellente."};
    case 2:
        return (PM_StatusInfo){2, "🟡", "Bon", "Qualité de l'air satisfaisante."};
    case 3:
        return (PM_StatusInfo){3, "🟠", "Moyen", "Qualité acceptable pour tous."};
    case 4:
        return (PM_StatusInfo){4, "🔴", "Dégradé", "Risque pour personnes sensibles."};
    case 5:
        return (PM_StatusInfo){5, "🟣", "Mauvais", "Risque pour la santé générale."};
    case 6:
        return (PM_StatusInfo){6, "⚫", "Très mauvais", "Évitez les activités extérieures."};
    default:
        return (PM_StatusInfo){0, "❓", "Inconnu", "Valeur non reconnue."};
    }
}

// -----------------------------------------------------------------------------
//  FONCTIONS D'INTERPRETATION RATIO
// -----------------------------------------------------------------------------
/**
 * @brief Calcule le code de ratio PM2.5/PM10
 * @param pm25 Valeur PM2.5
 * @param pm10 Valeur PM10
 * @return Code ratio (1=PM10 dominant, 2=équilibré, 3=PM2.5 dominant, 4=PM2.5 très dominant)
 */
int PM25_Ratio_GetCode(uint16_t pm25, uint16_t pm10)
{
    float ratio = pm10 > 0 ? (float)pm25 / pm10 : 0.0f;
    if (ratio < 0.5f)
        return 1; // PM10 dominant
    else if (ratio < 0.8f)
        return 2; // Ratio équilibré
    else if (ratio < 1.2f)
        return 3; // PM2.5 dominant
    else
        return 4; // PM2.5 très dominant
}

/**
 * @brief Interprète un code de ratio PM2.5/PM10
 * @param code Code ratio (1-4)
 * @return Structure pastille (emoji, label, description)
 */
PM_StatusInfo PM25_Ratio_Interpret(int code)
{
    switch (code)
    {
    case 1:
        return (PM_StatusInfo){1, "🔵", "PM10 dominant", "Pollution PM10 prédominante."};
    case 2:
        return (PM_StatusInfo){2, "🟡", "Équilibré", "Pollution mixte PM2.5/PM10."};
    case 3:
        return (PM_StatusInfo){3, "🟠", "PM2.5 dominant", "PM2.5 légèrement dominant."};
    case 4:
        return (PM_StatusInfo){4, "🔴", "PM2.5 très dominant", "Pollution PM2.5 prédominante."};
    default:
        return (PM_StatusInfo){0, "❓", "Inconnu", "Ratio non reconnu."};
    }
}

// -----------------------------------------------------------------------------
//  NOUVELLE FONCTION DMA AVEC ÉTATS COMME POLLING
// -----------------------------------------------------------------------------
int PM25_DMA_ReadFull_IT(UART_HandleTypeDef *huart, PM25_FullData *data, unsigned long interval_ms)
{
    static unsigned long last_reading_time = 0;
    static unsigned long activation_time = 0;
    static int state = 0; // 0: veille, 1: activation, 2: lecture DMA
    static int dma_started = 0;
    unsigned long current_time = HAL_GetTick();

    switch (state)
    {
    case 0: // Mode veille
        if (interval_ms == 0 || (current_time - last_reading_time) >= interval_ms)
        {
            // Mode continu : pas d'attente entre lectures
            if (interval_ms == 0 && state == 0)
            {
                PM25_DEBUG_PRINT("[PM25 DMA IT] Mode continu activé\n");
            }

            // Vérifier que DMA n'est pas déjà actif
            if (dma_started)
            {
                PM25_DEBUG_PRINT("[PM25 DMA IT] DMA encore actif, arrêt forcé\n");
                HAL_UART_DMAStop(huart);
                __HAL_UART_DISABLE_IT(huart, UART_IT_IDLE);
                dma_started = 0;
                HAL_Delay(100);
            }

            // Vérifier l'état du UART
            if (HAL_UART_GetState(huart) != HAL_UART_STATE_READY)
            {
                PM25_DEBUG_PRINT("[PM25 DMA IT] UART pas prêt, réinitialisation...\n");
                HAL_UART_DeInit(huart);
                HAL_Delay(50);
            }

            // Petite pause pour stabilité avant activation
            HAL_Delay(100);
            // Activer le capteur
            HAL_GPIO_WritePin(pm25_set_port, pm25_set_pin, GPIO_PIN_SET);
            activation_time = current_time;
            state = 1;
            dma_started = 0;
            PM25_DEBUG_PRINT("[PM25 DMA IT] SET -> HIGH (activation)\n");
            return -1; // En cours d'activation
        }
        return -1; // Toujours en veille

    case 1: // Attente activation capteur
        if ((current_time - activation_time) >= 1000)
        { // 1s pour activation
            state = 2;
            // Nettoyer le buffer avant de démarrer DMA
            memset(async_buffer, 0, sizeof(async_buffer));
            usart1_idle_flag = 0; // Reset flag

            // S'assurer que DMA est bien arrêté avant de redémarrer
            if (dma_started)
            {
                PM25_DEBUG_PRINT("[PM25 DMA IT] Arrêt DMA en cours...\n");
                HAL_UART_DMAStop(huart);
                __HAL_UART_DISABLE_IT(huart, UART_IT_IDLE);
                dma_started = 0;
                HAL_Delay(100); // Pause plus longue pour stabilité
            }

            // Réinitialiser le canal DMA si nécessaire
            if (huart->hdmarx != NULL)
            {
                // Réinitialiser le stream DMA
                HAL_DMA_Abort(huart->hdmarx);
                HAL_Delay(50);
            }

            PM25_DEBUG_PRINT("[PM25 DMA IT] Tentative démarrage DMA...\n");
            // Démarrer DMA pour recevoir les données
            HAL_StatusTypeDef dma_status = HAL_UART_Receive_DMA(huart, async_buffer, sizeof(async_buffer));
            if (dma_status == HAL_OK)
            {
                __HAL_UART_ENABLE_IT(huart, UART_IT_IDLE);
                dma_started = 1;
                PM25_DEBUG_PRINT("[PM25 DMA IT] Capteur prêt, DMA démarrée avec succès\n");
            }
            else
            {
                PM25_DEBUG_PRINT("[PM25 DMA IT] Erreur démarrage DMA - Status: %d, UART State: %ld\n", dma_status, HAL_UART_GetState(huart));
                HAL_GPIO_WritePin(pm25_set_port, pm25_set_pin, GPIO_PIN_RESET);
                last_reading_time = current_time;
                state = 0;
                return 0; // Échec
            }
        }
        return -1; // Toujours en activation

    case 2: // Lecture avec DMA
    {
        current_time = HAL_GetTick(); // Mettre à jour le temps pour cette case

        // Debug: vérifier si DMA reçoit des données
        static unsigned long last_debug_time = 0;
        if ((current_time - last_debug_time) > 1000) // Debug toutes les secondes
        {
            if (huart->hdmarx != NULL)
            {
                PM25_DEBUG_PRINT("[PM25 DMA IT] DMA handle OK, flag IDLE: %d\n", usart1_idle_flag);
            }
            else
            {
                PM25_DEBUG_PRINT("[PM25 DMA IT] DMA handle NULL, flag IDLE: %d\n", usart1_idle_flag);
            }
            last_debug_time = current_time;
        }

        // Attendre que l'interruption IDLE signale l'arrivée de données
        if (usart1_idle_flag)
        {
            PM25_DEBUG_PRINT("[PM25 DMA IT] Interruption IDLE reçue!\n");
            usart1_idle_flag = 0;

            // Scanner le buffer pour trouver le header valide
            uint16_t buffer_size = sizeof(async_buffer);
            int found = 0;

            for (uint16_t i = 0; i < buffer_size - PM25_FRAME_LEN; i++)
            {
                if (async_buffer[i] == 0x42 && async_buffer[i + 1] == 0x4D)
                {
                    // Header trouvé, vérifier la trame complète
                    uint8_t frame[PM25_FRAME_LEN];
                    for (int j = 0; j < PM25_FRAME_LEN; j++)
                    {
                        frame[j] = async_buffer[i + j];
                    }

                    if (PM25_Validate_Checksum(frame))
                    {
                        PM25_DEBUG_PRINT("[PM25 DMA IT] Trame valide trouvée à position %d\n", i);
                        pm25_extract_fields(data, frame);
                        found = 1;

                        // Mode continu : ne pas arrêter le DMA, continuer à recevoir
                        if (interval_ms == 0)
                        {
                            PM25_DEBUG_PRINT("[PM25 DMA IT] Mode continu : trame traitée, DMA reste actif\n");
                            last_reading_time = current_time;
                            activation_time = current_time; // Reset timer pour éviter timeout en continu
                            // Rester dans l'état 2 pour continuer à recevoir
                            return 1; // Succès, mais continuer
                        }
                        else
                        {
                            // Mode périodique : arrêter DMA et remettre en veille
                            if (dma_started)
                            {
                                PM25_DEBUG_PRINT("[PM25 DMA IT] Arrêt DMA après succès...\n");
                                HAL_UART_DMAStop(huart);
                                __HAL_UART_DISABLE_IT(huart, UART_IT_IDLE);
                                dma_started = 0;
                            }

                            // Remettre en veille
                            HAL_GPIO_WritePin(pm25_set_port, pm25_set_pin, GPIO_PIN_RESET);
                            last_reading_time = current_time;
                            state = 0;
                            PM25_DEBUG_PRINT("[PM25 DMA IT] SET -> LOW (retour veille)\n");
                            return 1; // Succès
                        }
                    }
                }
            }

            if (!found)
            {
                PM25_DEBUG_PRINT("[PM25 DMA IT] Aucune trame valide trouvée dans le buffer\n");
                // Continuer à attendre (peut-être données partielles)
                return -1;
            }
        }
        else
        {
            // Timeout de lecture, vérifier si on doit abandonner
            unsigned long timeout_limit = (interval_ms == 0) ? 10000 : 3000; // 10s en continu, 3s en périodique

            if ((current_time - activation_time) > timeout_limit)
            { // Timeout adapté au mode
                PM25_DEBUG_PRINT("[PM25 DMA IT] Timeout lecture DMA (mode: %s)\n", interval_ms == 0 ? "continu" : "périodique");

                if (interval_ms == 0)
                {
                    // Mode continu : attendre l'interruption IDLE avant de redémarrer
                    PM25_DEBUG_PRINT("[PM25 DMA IT] Mode continu : attente interruption UART avant redémarrage DMA\n");

                    // Arrêter DMA proprement
                    if (dma_started)
                    {
                        HAL_UART_DMAStop(huart);
                        __HAL_UART_DISABLE_IT(huart, UART_IT_IDLE);
                        dma_started = 0;
                        HAL_Delay(200); // Pause pour stabilization
                    }

                    // Attendre que l'UART soit vraiment idle (pas de données en cours)
                    static unsigned long wait_idle_start = 0;
                    if (wait_idle_start == 0)
                    {
                        wait_idle_start = HAL_GetTick();
                        PM25_DEBUG_PRINT("[PM25 DMA IT] Attente UART idle...\n");
                    }

                    // Attendre jusqu'à 5 secondes pour que l'UART signale idle en mode continu
                    if ((HAL_GetTick() - wait_idle_start) < 5000)
                    {
                        // Vérifier si l'UART a signalé idle pendant l'attente
                        if (usart1_idle_flag)
                        {
                            PM25_DEBUG_PRINT("[PM25 DMA IT] UART idle détecté, redémarrage DMA\n");
                            usart1_idle_flag = 0;
                            wait_idle_start = 0;

                            // Redémarrer DMA
                            memset(async_buffer, 0, sizeof(async_buffer));
                            if (HAL_UART_Receive_DMA(huart, async_buffer, sizeof(async_buffer)) == HAL_OK)
                            {
                                __HAL_UART_ENABLE_IT(huart, UART_IT_IDLE);
                                dma_started = 1;
                                activation_time = HAL_GetTick(); // Reset timer après redémarrage
                                PM25_DEBUG_PRINT("[PM25 DMA IT] DMA redémarré après attente idle\n");
                            }
                            else
                            {
                                PM25_DEBUG_PRINT("[PM25 DMA IT] Échec redémarrage DMA après attente idle\n");
                                // Remettre en veille et réessayer plus tard
                                HAL_GPIO_WritePin(pm25_set_port, pm25_set_pin, GPIO_PIN_RESET);
                                last_reading_time = current_time;
                                state = 0;
                                return 0;
                            }
                        }
                        else
                        {
                            // Continuer à attendre
                            return -1;
                        }
                    }
                    else
                    {
                        // Timeout d'attente idle, forcer le redémarrage
                        PM25_DEBUG_PRINT("[PM25 DMA IT] Timeout attente idle, redémarrage forcé\n");
                        wait_idle_start = 0;

                        memset(async_buffer, 0, sizeof(async_buffer));
                        usart1_idle_flag = 0;
                        if (HAL_UART_Receive_DMA(huart, async_buffer, sizeof(async_buffer)) == HAL_OK)
                        {
                            __HAL_UART_ENABLE_IT(huart, UART_IT_IDLE);
                            dma_started = 1;
                            activation_time = HAL_GetTick(); // Reset timer après redémarrage forcé
                            PM25_DEBUG_PRINT("[PM25 DMA IT] DMA redémarré après timeout forcé\n");
                        }
                        else
                        {
                            PM25_DEBUG_PRINT("[PM25 DMA IT] Échec redémarrage DMA après timeout forcé\n");
                            HAL_GPIO_WritePin(pm25_set_port, pm25_set_pin, GPIO_PIN_RESET);
                            last_reading_time = current_time;
                            state = 0;
                            return 0;
                        }
                    }
                }
                else
                {
                    // Mode périodique : arrêter tout
                    if (dma_started)
                    {
                        HAL_UART_DMAStop(huart);
                        __HAL_UART_DISABLE_IT(huart, UART_IT_IDLE);
                        dma_started = 0;
                    }
                    HAL_GPIO_WritePin(pm25_set_port, pm25_set_pin, GPIO_PIN_RESET);
                    last_reading_time = current_time;
                    state = 0;
                    return 0; // Échec
                }
            }
            return -1; // Continue d'attendre les données
        }
    }
    }

    return -1; // État inconnu
}
