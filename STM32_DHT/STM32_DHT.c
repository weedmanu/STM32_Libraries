#include "STM32_DHT.h"      // Inclure le fichier d'en-tête pour les capteurs DHT

// --- Constantes de temporisation et de configuration ---

// Délais de démarrage (en microsecondes)
#define DHT_START_LOW_TIME_DHT11  18000 ///< Temps bas pour démarrer la communication DHT11
#define DHT_START_HIGH_TIME_DHT11 20    ///< Temps haut après le démarrage DHT11
#define DHT_START_LOW_TIME_DHT22  1000  ///< Temps bas pour démarrer la communication DHT22/21
#define DHT_START_HIGH_TIME_DHT22 30    ///< Temps haut après le démarrage DHT22/21

// Timeout pour la lecture des bits (en microsecondes)
// Doit être plus long que la durée maximale d'un bit (environ 70us pour un '1')
#define DHT_READ_BIT_TIMEOUT 100
// Timeout pour l'attente de la réponse initiale (en microsecondes)
#define DHT_START_RESPONSE_TIMEOUT 100

// Délais internes pour la lecture des bits (en microsecondes)
#define DHT_RESPONSE_WAIT_TIME 40 ///< Temps d'attente avant de vérifier la réponse basse
#define DHT_RESPONSE_LOW_DURATION 80 ///< Durée attendue de la réponse basse
#define DHT_BIT_DECISION_TIME 40 ///< Temps après lequel on décide si un bit est 0 ou 1

/**
 * @brief Initialise le capteur DHT
 *
 * Cette fonction configure le capteur DHT en définissant le port GPIO,
 * la broche GPIO et le type de capteur. Elle réinitialise également le
 * compteur du timer.
 *
 * @param sensor Pointeur vers la structure DHT_Sensor à initialiser
 * @param htim Pointeur vers le handle du timer à utiliser (doit être démarré avant)
 * @param port Port GPIO auquel le capteur est connecté
 * @param pin Broche GPIO à laquelle le capteur est connecté
 * @param type Type du capteur (DHT11, DHT22, DHT21)
 * @return HAL_StatusTypeDef Statut de l'opération
 */
 HAL_StatusTypeDef DHT_Init(DHT_Sensor* sensor, TIM_HandleTypeDef* htim, GPIO_TypeDef* port, uint16_t pin, uint8_t type) {
    if (!sensor || !port || !htim) {
        return HAL_ERROR; // Vérifiez les pointeurs nuls
    }

    sensor->DHT_PORT = port;
    sensor->DHT_PIN = pin;
    sensor->sensorType = type;
    sensor->htim = htim;

    // Vérification optionnelle : s'assurer que le timer est bien démarré
    // Note: La vérification HAL_TIM_Base_Start doit être faite *avant* d'appeler DHT_Init.
    // if (sensor->htim->Instance->CR1 & TIM_CR1_CEN == 0) return HAL_ERROR;
    __HAL_TIM_SET_COUNTER(sensor->htim, 0); // Reset counter

    return HAL_OK;
}

/**
 * @brief Génère un délai en microsecondes à l'aide d'un timer
 *
 * Cette fonction utilise un timer pour créer un délai précis en microsecondes.
 *
 * @param sensor Pointeur vers la structure DHT_Sensor (contient le timer handle).
 * @param delay Durée du délai en microsecondes
 */
static void microDelay(DHT_Sensor* sensor, uint16_t delay) {
    __HAL_TIM_SET_COUNTER(sensor->htim, 0);                 // Réinitialiser le compteur du timer
    while (__HAL_TIM_GET_COUNTER(sensor->htim) < delay);     // Attendre jusqu'à ce que le compteur atteigne la valeur de délai
}

/**
 * @brief Démarre la communication avec le capteur DHT
 *
 * Cette fonction envoie un signal de départ au capteur DHT et attend une réponse.
 *
 * @param htim Pointeur vers une structure TIM_HandleTypeDef contenant les informations de configuration pour le module TIM.
 * @param sensor Pointeur vers la structure DHT_Sensor configurée.
 * @return HAL_StatusTypeDef HAL_OK si réponse, HAL_ERROR/HAL_TIMEOUT sinon
 */
 static HAL_StatusTypeDef DHT_Start(DHT_Sensor* sensor) {
    HAL_StatusTypeDef response_status = HAL_ERROR;  // Initialiser le statut de réponse à Erreur par défaut (si la réponse n'est pas vue)
    GPIO_InitTypeDef GPIO_InitStruct = {0};         // Structure pour initialiser le GPIO

    // Configurer la broche du capteur en mode sortie
    GPIO_InitStruct.Pin = sensor->DHT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(sensor->DHT_PORT, &GPIO_InitStruct);

    // Envoyer le signal de démarrage
    HAL_GPIO_WritePin(sensor->DHT_PORT, sensor->DHT_PIN, GPIO_PIN_RESET); // Mettre la broche à l'état bas
    if (sensor->sensorType == DHT11) {
        microDelay(sensor, DHT_START_LOW_TIME_DHT11); // Délai bas pour DHT11
    } else { // DHT22 ou DHT21
        microDelay(sensor, DHT_START_LOW_TIME_DHT22); // Délai bas pour DHT22/21
    }
    HAL_GPIO_WritePin(sensor->DHT_PORT, sensor->DHT_PIN, GPIO_PIN_SET);   // Mettre la broche à l'état haut
    // Le délai haut est court, on peut utiliser microDelay directement ou une constante si définie
    microDelay(sensor, DHT_RESPONSE_WAIT_TIME); // Attendre un peu avant de passer en entrée (~30us pour DHT22, ~20us pour DHT11)

    // Configurer la broche du capteur en mode entrée
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(sensor->DHT_PORT, &GPIO_InitStruct);

    // Attendre la réponse du capteur (état bas)    
    __HAL_TIM_SET_COUNTER(sensor->htim, 0);
    while (!(HAL_GPIO_ReadPin(sensor->DHT_PORT, sensor->DHT_PIN))) { // Attendre que la broche soit basse (début de la réponse)
        if (__HAL_TIM_GET_COUNTER(sensor->htim) > DHT_START_RESPONSE_TIMEOUT) {
            return HAL_TIMEOUT; // Timeout, pas de réponse
        }
    }

    // Attendre la fin de la réponse (état haut)
    __HAL_TIM_SET_COUNTER(sensor->htim, 0);
    while (HAL_GPIO_ReadPin(sensor->DHT_PORT, sensor->DHT_PIN)) { // Attendre que la broche soit haute (fin de la réponse, début des données)
        // Cette boucle attend la fin de l'état HAUT de 80us qui suit l'état BAS de 80us.
        if (__HAL_TIM_GET_COUNTER(sensor->htim) > DHT_START_RESPONSE_TIMEOUT) {
             return HAL_TIMEOUT; // Timeout pendant l'attente de la fin de la réponse
        } else {
            response_status = HAL_OK; // Le capteur a répondu
        }
    }
    return response_status; // Retourner le statut de la réponse du capteur
}

/**
 * @brief Lit un octet de données depuis le capteur DHT
 *
 * Cette fonction lit un octet de données envoyé par le capteur DHT.
 *
 * @param htim Pointeur vers une structure TIM_HandleTypeDef contenant les informations de configuration pour le module TIM.
 * @param sensor Pointeur vers la structure DHT_Sensor configurée.
 * @param data_byte Pointeur vers un uint8_t où l'octet lu sera stocké.
 * @return HAL_StatusTypeDef Statut de l'opération (HAL_OK ou HAL_TIMEOUT).
 */
 static HAL_StatusTypeDef DHT_Read(DHT_Sensor* sensor, uint8_t* data_byte) {
    uint8_t i;
    uint8_t current_byte = 0; // Initialiser l'octet lu à 0

    for (i = 0; i < 8; i++) {
        // Attendre la fin de l'état bas (50us) qui précède chaque bit
        __HAL_TIM_SET_COUNTER(sensor->htim, 0);
        while (!(HAL_GPIO_ReadPin(sensor->DHT_PORT, sensor->DHT_PIN))) { // Attente du niveau bas de 50us
            if (__HAL_TIM_GET_COUNTER(sensor->htim) > DHT_READ_BIT_TIMEOUT) { // Timeout en attendant le début du bit (niveau haut)
                return HAL_TIMEOUT;
            }
        }

        // Mesurer la durée de l'état HAUT pour déterminer si c'est un 0 ou un 1
        microDelay(sensor, DHT_BIT_DECISION_TIME); // Attendre 40 µs

        if (!(HAL_GPIO_ReadPin(sensor->DHT_PORT, sensor->DHT_PIN))) {
            // Si la broche est BASSE après 40us -> bit '0'
            // C'est un 0, on ne fait rien car current_byte est déjà initialisé avec des 0
            // current_byte &= ~(1 << (7 - i)); // Optionnel: Mettre explicitement le bit à 0
        } else {
            // Si la broche est HAUTE après 40us -> bit '1'
            current_byte |= (1 << (7 - i)); // Mettre le bit à 1

            // Attendre que la ligne retourne à BAS (fin du bit '1')
            __HAL_TIM_SET_COUNTER(sensor->htim, 0); // Reset timer pour le timeout de fin de bit
            while ((HAL_GPIO_ReadPin(sensor->DHT_PORT, sensor->DHT_PIN))) { // Attente de la fin du niveau haut
                 if (__HAL_TIM_GET_COUNTER(sensor->htim) > DHT_READ_BIT_TIMEOUT) { // Utiliser le même timeout
                     // __enable_irq(); // --- Section Critique par Bit Fin (Erreur) --- // Supprimé
                     return HAL_TIMEOUT; // Timeout en attendant la fin du bit '1'
                 }
            }
        }
    }
    *data_byte = current_byte; // Stocker l'octet lu dans la variable pointée
    return HAL_OK; // Lecture réussie
}

/**
 * @brief Récupère les données de température et d'humidité du capteur DHT
 *
 * Cette fonction lit les données de température et d'humidité depuis le capteur DHT
 * et les stocke dans un tableau.
 *
 * @param sensor Pointeur vers la structure DHT_Sensor
 * @param data Tableau pour stocker les données de température et d'humidité
 * @return HAL_StatusTypeDef Statut de l'opération
 */
 HAL_StatusTypeDef DHT_GetData(DHT_Sensor* sensor, float data[2]) {
    HAL_StatusTypeDef status;

    status = DHT_Start(sensor); // Utilise sensor->htim
    if (status != HAL_OK) {
        return status; // Pas de réponse du capteur ou timeout pendant start
    }

    status = DHT_Read(sensor, &sensor->hum1); // Utilise sensor->htim
    if (status != HAL_OK) {
        return status; // Retourne HAL_TIMEOUT si la lecture a échoué
    }
    status = DHT_Read(sensor, &sensor->hum2);
    if (status != HAL_OK) {
        return status;
    }
    status = DHT_Read(sensor, &sensor->temp1);
    if (status != HAL_OK) {
        return status;
    }
    status = DHT_Read(sensor, &sensor->temp2);
    if (status != HAL_OK) {
        return status;
    }
    status = DHT_Read(sensor, &sensor->SUM);
    if (status != HAL_OK) {
        return status;
    }

    sensor->CHECK = sensor->hum1 + sensor->hum2 + sensor->temp1 + sensor->temp2;

    if (sensor->CHECK != sensor->SUM) {
        return HAL_ERROR; // Erreur de somme de contrôle (Checksum mismatch)
    }
    
     if (sensor->sensorType == DHT11) {
         data[0] = (float)sensor->temp1; // Température
         data[1] = (float)sensor->hum1;  // Humidité
     } else if (sensor->sensorType == DHT22 || sensor->sensorType == DHT21) {
         // Combinez les octets. Température peut être négative (bit 15 = signe).
         int16_t temp_raw = (int16_t)((sensor->temp1 << 8) | sensor->temp2);
         // Vérifiez si le bit de signe (bit 15) est à 1
         if (temp_raw & 0x8000) {
             temp_raw &= 0x7FFF; // Masquer le bit de signe
             data[0] = -(float)temp_raw / 10.0f; // Appliquer le signe négatif
         } else {
             data[0] = (float)temp_raw / 10.0f;
         }
         // L'humidité est toujours positive
         data[1] = (float)((sensor->hum1 << 8) | sensor->hum2) / 10.0f;
     }

    return HAL_OK;
} // Fin DHT_GetData
