/**
 * @file STM32_MP25_temp.c
 * @brief Librairie minimaliste pour capteur PM2.5 SEN0177 (DFRobot) sur STM32 HAL
 *
 * Organisation par sections :
 *  - Includes
 *  - Structures & constantes
 *  - Fonctions d'initialisation
 *  - Fonctions de lecture & validation
 *  - Fonctions d'interprétation qualité air
 *  - Fonctions d'interprétation ratio
 */

// -----------------------------------------------------------------------------
//  INCLUDES
// -----------------------------------------------------------------------------
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
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
        &data->particles_2_5um, &data->particles_5_0um, &data->particles_10um
    };

    for (int i = 0; i < 12; i++) {
        *fields[i] = (ptr[0] << 8) | ptr[1];
        ptr += 2;
    }

    data->version = frame[28];
    data->checksum = (frame[30] << 8) | frame[31];
}

// -----------------------------------------------------------------------------
//  OPTIMISATION: Lecture UART optimisée (évite memmove répété)
// -----------------------------------------------------------------------------
static int read_and_extract(UART_HandleTypeDef *huart, PM25_FullData *data, uint32_t overall_timeout)
{
    uint8_t buffer[PM25_FRAME_LEN * 2]; // Buffer circulaire double taille
    int head = 0; // Position d'écriture dans le buffer circulaire
    uint32_t start = HAL_GetTick();

    if (overall_timeout == 0)
        overall_timeout = 1500;

    PM25_DEBUG_PRINT("[PM25 DEBUG] Lecture optimisée: recherche trame valide...\n");

    while ((HAL_GetTick() - start) < overall_timeout)
    {
        uint8_t b;
        if (HAL_UART_Receive(huart, &b, 1, 200) != HAL_OK)
            continue;

        // Ajouter l'octet au buffer circulaire
        buffer[head] = b;
        head = (head + 1) % (PM25_FRAME_LEN * 2);

        // Vérifier si on a assez de données pour une trame complète
        if (head >= PM25_FRAME_LEN)
        {
            // Calculer la position de début de la fenêtre de 32 octets
            int window_start = (head - PM25_FRAME_LEN + (PM25_FRAME_LEN * 2)) % (PM25_FRAME_LEN * 2);

            // Vérifier le header
            if (buffer[window_start] == 0x42 &&
                buffer[(window_start + 1) % (PM25_FRAME_LEN * 2)] == 0x4D)
            {
                // Extraire la trame dans un buffer temporaire
                uint8_t frame[PM25_FRAME_LEN];
                for (int i = 0; i < PM25_FRAME_LEN; i++) {
                    frame[i] = buffer[(window_start + i) % (PM25_FRAME_LEN * 2)];
                }

                // Vérifier le checksum
                if (PM25_Validate_Checksum(frame))
                {
                    PM25_DEBUG_PRINT("[PM25 DEBUG] Trame valide trouvée - arrêt lecture\n");
                    pm25_extract_fields(data, frame);
                    return 1;
                }
            }
        }
    }

    PM25_DEBUG_PRINT("[PM25 DEBUG] Timeout - trame non trouvée\n");
    return 0;
}



// Variables globales pour la pin SET
static GPIO_TypeDef *pm25_set_port = GPIOA;
static uint16_t pm25_set_pin = GPIO_PIN_8;
static UART_HandleTypeDef *pm25_uart_handle = NULL;
void PM25_SetControlPin(GPIO_TypeDef *port, uint16_t pin)
{
    pm25_set_port = port;
    pm25_set_pin = pin;
}

// -----------------------------------------------------------------------------
//  FONCTIONS D'INITIALISATION
// -----------------------------------------------------------------------------
void PM25_Polling_Init(UART_HandleTypeDef *huart)
{
    pm25_uart_handle = huart;

    // Mettre le capteur en veille par défaut (SET = LOW)
    PM25_DEBUG_PRINT("[PM25 DEBUG] Initialisation - mise en veille du capteur\n");
    HAL_GPIO_WritePin(pm25_set_port, pm25_set_pin, GPIO_PIN_RESET);

    HAL_Delay(2000); // Attendre la stabilisation
    PM25_DEBUG_PRINT("[PM25 DEBUG] Init capteur PM2.5 terminée (capteur en veille)\n");
}

void PM25_SetDebugMode(int enable)
{
    pm25_debug_mode = enable;
}


// -----------------------------------------------------------------------------
//  FONCTIONS DE LECTURE & VALIDATION
// -----------------------------------------------------------------------------
/**
 * @brief Lit une trame complète avec toutes les données
 * @param huart Handle UART
 * @param data Structure de sortie complète
 * @param timeout_ms Timeout en millisecondes pour la réception
 * @return 1 si succès, 0 si échec
 */
int PM25_Polling_ReadFull(UART_HandleTypeDef *huart, PM25_FullData *data, const uint32_t *timeout_ms)
{
    int result = 0;
    if (timeout_ms == NULL)
    {
        // Mode continu (caller passed NULL)
        PM25_DEBUG_PRINT("[PM25 DEBUG] Mode continu (timeout NULL)\n");
        result = read_and_extract(huart, data, 0);
    }
    else
    {
        // Mode périodique : contrôle SET via PA8 pour économie d'énergie
        PM25_DEBUG_PRINT("[PM25 DEBUG] Mode périodique (interval %lu ms)\n", (unsigned long)*timeout_ms);

        // Attendre d'abord l'intervalle demandé EN VEILLE
        PM25_DEBUG_PRINT("[PM25 DEBUG] Attente %lu ms en mode veille...\n", (unsigned long)*timeout_ms);
        HAL_Delay(*timeout_ms);

        // Activer brièvement le capteur pour une seule lecture
        PM25_DEBUG_PRINT("[PM25 DEBUG] SET -> HIGH (activation temporaire)\n");
        HAL_GPIO_WritePin(pm25_set_port, pm25_set_pin, GPIO_PIN_SET);

        // Attendre activation du capteur (optimisé)
        HAL_Delay(1000);

        // Lecture rapide d'une seule trame
        PM25_DEBUG_PRINT("[PM25 DEBUG] Lecture d'une trame...\n");
        result = read_and_extract(huart, data, 2000); // Timeout réduit à 2 secondes

        // Remettre immédiatement en veille
        PM25_DEBUG_PRINT("[PM25 DEBUG] SET -> LOW (retour en veille)\n");
        HAL_GPIO_WritePin(pm25_set_port, pm25_set_pin, GPIO_PIN_RESET);
    }
    return result;
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
