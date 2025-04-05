#include "STM32_DHT.h"      // Inclure le fichier d'en-tête pour les capteurs DHT
#include "config.h"         // Inclure le fichier de configuration

/**
 * @brief Initialise le capteur DHT
 *
 * Cette fonction configure le capteur DHT en définissant le port GPIO,
 * la broche GPIO et le type de capteur. Elle réinitialise également le
 * compteur du timer.
 *
 * @param sensor Pointeur vers la structure DHT_Sensor à initialiser
 * @param port Port GPIO auquel le capteur est connecté
 * @param pin Broche GPIO à laquelle le capteur est connecté
 * @param type Type du capteur (DHT11, DHT22, DHT21)
 */
uint8_t DHT_Init(DHT_Sensor* sensor, GPIO_TypeDef* port, uint16_t pin, uint8_t type) {
    if (!sensor || !port) {
        return DHT_ERROR; // Vérifiez les pointeurs nuls
    }

    sensor->DHT_PORT = port;
    sensor->DHT_PIN = pin;
    sensor->sensorType = type;
    __HAL_TIM_SET_COUNTER(DHT_TIMER, 0);

    if (HAL_TIM_Base_Start(DHT_TIMER) != HAL_OK) {
        return DHT_ERROR; // Vérifiez si le timer démarre correctement
    }

    return DHT_OK;
}

/**
 * @brief Génère un délai en microsecondes à l'aide d'un timer
 *
 * Cette fonction utilise un timer pour créer un délai précis en microsecondes.
 *
 * @param htim Pointeur vers une structure TIM_HandleTypeDef contenant les informations de configuration pour le module TIM.
 * @param delay Durée du délai en microsecondes
 */
void microDelay(TIM_HandleTypeDef *htim, uint16_t delay) {
    __HAL_TIM_SET_COUNTER(htim, 0);                 // Réinitialiser le compteur du timer
    while (__HAL_TIM_GET_COUNTER(htim) < delay);     // Attendre jusqu'à ce que le compteur atteigne la valeur de délai
}

/**
 * @brief Génère un délai spécifique en fonction du type de capteur et de la phase
 *
 * Cette fonction utilise un timer pour créer un délai précis en fonction du type de capteur et de la phase.
 *
 * @param htim Pointeur vers une structure TIM_HandleTypeDef contenant les informations de configuration pour le module TIM.
 * @param sensorType Type du capteur (DHT11, DHT22, DHT21)
 * @param phase Phase du délai (1 ou 2)
 */
static void DHT_Delay(TIM_HandleTypeDef *htim, uint8_t sensorType, uint8_t phase) {
    switch (sensorType) {
        case DHT22:
            if (phase == 1) microDelay(htim, 1300); // Phase 1 pour DHT22
            else microDelay(htim, 30);             // Phase 2 pour DHT22
            break;
        case DHT11:
            if (phase == 1) microDelay(htim, 18000); // Phase 1 pour DHT11
            else microDelay(htim, 20);              // Phase 2 pour DHT11
            break;
        case DHT21:
            if (phase == 1) microDelay(htim, 1000);  // Phase 1 pour DHT21
            else microDelay(htim, 30);              // Phase 2 pour DHT21
            break;
    }
}

/**
 * @brief Démarre la communication avec le capteur DHT
 *
 * Cette fonction envoie un signal de départ au capteur DHT et attend une réponse.
 *
 * @param htim Pointeur vers une structure TIM_HandleTypeDef contenant les informations de configuration pour le module TIM.
 * @param sensor Pointeur vers la structure DHT_Sensor
 * @return uint8_t 1 si le capteur a répondu, 0 sinon
 */
uint8_t DHT_Start(TIM_HandleTypeDef *htim, DHT_Sensor* sensor) {
    uint8_t Response = 0;                            // Initialiser la variable de réponse
    GPIO_InitTypeDef GPIO_InitStruct = {0};         // Structure pour initialiser le GPIO

    // Configurer la broche du capteur en mode sortie
    GPIO_InitStruct.Pin = sensor->DHT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(sensor->DHT_PORT, &GPIO_InitStruct);

    // Envoyer le signal de démarrage
    HAL_GPIO_WritePin(sensor->DHT_PORT, sensor->DHT_PIN, GPIO_PIN_RESET); // Mettre la broche à l'état bas
    DHT_Delay(htim, sensor->sensorType, 1);                               // Délai pour la phase 1
    HAL_GPIO_WritePin(sensor->DHT_PORT, sensor->DHT_PIN, GPIO_PIN_SET);   // Mettre la broche à l'état haut
    DHT_Delay(htim, sensor->sensorType, 2);                               // Délai pour la phase 2

    // Configurer la broche du capteur en mode entrée
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(sensor->DHT_PORT, &GPIO_InitStruct);

    // Vérifier la réponse du capteur
    microDelay(htim, 40); // Attendre 40 µs
    if (!(HAL_GPIO_ReadPin(sensor->DHT_PORT, sensor->DHT_PIN))) { // Si la broche est à l'état bas
        microDelay(htim, 80); // Attendre 80 µs
        if (HAL_GPIO_ReadPin(sensor->DHT_PORT, sensor->DHT_PIN)) { // Si la broche passe à l'état haut
            Response = 1; // Le capteur a répondu
        }
    }

    // Attendre la fin de la réponse
    uint32_t startTime = HAL_GetTick();
    while ((HAL_GPIO_ReadPin(sensor->DHT_PORT, sensor->DHT_PIN)) && (HAL_GetTick() - startTime < 2));

    return Response; // Retourner la réponse du capteur
}

/**
 * @brief Lit un octet de données depuis le capteur DHT
 *
 * Cette fonction lit un octet de données envoyé par le capteur DHT.
 *
 * @param htim Pointeur vers une structure TIM_HandleTypeDef contenant les informations de configuration pour le module TIM.
 * @param sensor Pointeur vers la structure DHT_Sensor
 * @return uint8_t L'octet lu depuis le capteur
 */
uint8_t DHT_Read(TIM_HandleTypeDef *htim, DHT_Sensor* sensor) {
    uint8_t x, y = 0;                                // Initialiser les variables de boucle et de données
    for (x = 0; x < 8; x++) {                        // Boucle pour lire 8 bits
        while (!(HAL_GPIO_ReadPin(sensor->DHT_PORT, sensor->DHT_PIN))); // Attendre que la broche soit à l'état haut
        microDelay(htim, 40);                        // Attendre 40 µs
        if (!(HAL_GPIO_ReadPin(sensor->DHT_PORT, sensor->DHT_PIN))) { // Si la broche est à l'état bas
            y &= ~(1 << (7 - x));                    // Mettre le bit correspondant à 0
        } else {
            y |= (1 << (7 - x));                     // Mettre le bit correspondant à 1
        }
        while ((HAL_GPIO_ReadPin(sensor->DHT_PORT, sensor->DHT_PIN))); // Attendre que la broche soit à l'état bas
    }
    return y;                                         // Retourner l'octet lu
}

/**
 * @brief Récupère les données de température et d'humidité du capteur DHT
 *
 * Cette fonction lit les données de température et d'humidité depuis le capteur DHT
 * et les stocke dans un tableau.
 *
 * @param sensor Pointeur vers la structure DHT_Sensor
 * @param data Tableau pour stocker les données de température et d'humidité
 */
uint8_t DHT_GetData(DHT_Sensor* sensor, float data[2]) {
    if (!DHT_Start(DHT_TIMER, sensor)) {
        return DHT_NO_RESPONSE; // Pas de réponse du capteur
    }

    sensor->hum1 = DHT_Read(DHT_TIMER, sensor);
    sensor->hum2 = DHT_Read(DHT_TIMER, sensor);
    sensor->temp1 = DHT_Read(DHT_TIMER, sensor);
    sensor->temp2 = DHT_Read(DHT_TIMER, sensor);
    sensor->SUM = DHT_Read(DHT_TIMER, sensor);

    sensor->CHECK = sensor->hum1 + sensor->hum2 + sensor->temp1 + sensor->temp2;

    if (sensor->CHECK != sensor->SUM) {
        return DHT_CHECKSUM_ERR; // Erreur de somme de contrôle
    }

    if (sensor->sensorType == DHT11) {
        data[0] = (float)sensor->temp1;
        data[1] = (float)sensor->hum1;
    } else if (sensor->sensorType == DHT22 || sensor->sensorType == DHT21) {
        if (sensor->temp1 > 127) {
            data[0] = (float)((sensor->temp1 << 8) | sensor->temp2) / 10 * -1;
        } else {
            data[0] = (float)((sensor->temp1 << 8) | sensor->temp2) / 10;
        }
        data[1] = (float)((sensor->hum1 << 8) | sensor->hum2) / 10;
    }

    return DHT_OK;
}


