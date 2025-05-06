#include "STM32_AHT20.h"

/**
 * @brief Initialise le capteur AHT20
 * @param hi2c Pointeur vers la structure de handle I2C
 * @return Statut de l'opération
 */
AHT20_Status AHT20_Init(I2C_HandleTypeDef *hi2c) {
    uint8_t cmd[3] = {AHT20_CMD_INIT, AHT20_INIT_PARAM1, AHT20_INIT_PARAM2};
    uint8_t status_byte;
    HAL_StatusTypeDef hal_status;

    // Reset logiciel avant initialisation
    if (AHT20_SoftReset(hi2c) != AHT20_OK) {
        // AHT20_SoftReset() contient déjà HAL_Delay(AHT20_DELAY_SOFT_RESET_MS)
        return AHT20_ERROR_I2C;
    }

    // Le délai de 20ms est déjà dans AHT20_SoftReset.
    // Pas de délai supplémentaire spécifié avant la commande d'initialisation.

    // Envoyer la commande d'initialisation
    hal_status = HAL_I2C_Master_Transmit(hi2c, AHT20_I2C_ADDR, cmd, 3, AHT20_I2C_TIMEOUT_MS);
    if (hal_status != HAL_OK) {
        printf("Erreur I2C: Code %d\n", hal_status); // Ajouter un log pour débogage
        return AHT20_ERROR_I2C;
    }

    // Attendre que l'initialisation soit terminée (datasheet: 10ms, on utilise 20ms pour sécurité)
    HAL_Delay(AHT20_DELAY_INIT_CMD_WAIT_MS);

    // Vérifier le statut du capteur
    if (AHT20_GetStatus(hi2c, &status_byte) != AHT20_OK) {
        return AHT20_ERROR_I2C;
    }

    // Vérifier si le bit de calibration est défini (bit 3) et que le capteur n'est pas occupé (bit 7)
    // Status byte format: [Busy, Mode, Reserved, CAL Enable, Reserved, Reserved, Reserved, Reserved]
    if (!(status_byte & 0x08)) { // Bit 3: CAL Enable
        return AHT20_ERROR_CALIBRATION;
    }
    if (status_byte & 0x80) { // Bit 7: Busy
        return AHT20_ERROR_BUSY; // Capteur occupé après initialisation
    }

    return AHT20_OK;
}

/**
 * @brief Effectue un reset logiciel du capteur
 * @param hi2c Pointeur vers la structure de handle I2C
 * @return Statut de l'opération
 */
AHT20_Status AHT20_SoftReset(I2C_HandleTypeDef *hi2c) {
    uint8_t cmd = AHT20_CMD_SOFT_RESET;
    HAL_StatusTypeDef hal_status;

    hal_status = HAL_I2C_Master_Transmit(hi2c, AHT20_I2C_ADDR, &cmd, 1, AHT20_I2C_TIMEOUT_MS);
    if (hal_status != HAL_OK) {
        printf("Erreur I2C: Code %d\n", hal_status); // Ajouter un log pour débogage
        return AHT20_ERROR_I2C;
    }

    // Attendre que le reset soit terminé
    HAL_Delay(AHT20_DELAY_SOFT_RESET_MS);

    return AHT20_OK;
}

/**
 * @brief Récupère le statut du capteur
 * @param hi2c Pointeur vers la structure de handle I2C
 * @param status Pointeur pour stocker l'octet de statut
 * @return Statut de l'opération
 */
AHT20_Status AHT20_GetStatus(I2C_HandleTypeDef *hi2c, uint8_t *status) {
    HAL_StatusTypeDef hal_status;

    hal_status = HAL_I2C_Master_Receive(hi2c, AHT20_I2C_ADDR, status, 1, AHT20_I2C_TIMEOUT_MS);
    if (hal_status != HAL_OK) {
        printf("Erreur I2C: Code %d\n", hal_status); // Ajouter un log pour débogage
        return AHT20_ERROR_I2C;
    }

    return AHT20_OK;
}

static uint8_t AHT20_CalculateChecksum(uint8_t *data, uint8_t len) {
    uint8_t crc = 0xFF; // Initialisation du CRC
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31; // Polynôme 0x31
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief Lit les mesures de température et d'humidité
 * @param hi2c Pointeur vers la structure de handle I2C
 * @param data Pointeur vers une structure pour stocker les mesures
 * @return Statut de l'opération
 */
AHT20_Status AHT20_ReadMeasurements(I2C_HandleTypeDef *hi2c, AHT20_Data *data) {
    uint8_t cmd[3] = {AHT20_CMD_TRIGGER, AHT20_MEASURE_PARAM1, AHT20_MEASURE_PARAM2};
    uint8_t buffer[7];
    HAL_StatusTypeDef hal_status;
    uint32_t raw_humidity, raw_temperature;

    if (hi2c == NULL || data == NULL) {
        return AHT20_ERROR_INVALID_PARAM;
    }

    // Envoyer la commande de mesure
    hal_status = HAL_I2C_Master_Transmit(hi2c, AHT20_I2C_ADDR, cmd, 3, AHT20_I2C_TIMEOUT_MS);
    if (hal_status != HAL_OK) {
        printf("Erreur I2C lors de la transmission: %d\r\n", hal_status);
        return AHT20_ERROR_I2C;
    }

    // Attendre la fin de la mesure (datasheet: >=75ms, on utilise 80ms)
    HAL_Delay(AHT20_DELAY_MEASUREMENT_WAIT_MS);

    // Lire les données (7 octets: Statut, H1, H2, H3, T1, T2, CRC)
    hal_status = HAL_I2C_Master_Receive(hi2c, AHT20_I2C_ADDR, buffer, 7, AHT20_I2C_TIMEOUT_MS);
    if (hal_status != HAL_OK) {
        printf("Erreur I2C lors de la réception: %d\r\n", hal_status);
        return AHT20_ERROR_I2C;
    }

    // Vérifier si le capteur est occupé (bit 7 de l'octet de statut buffer[0])
    if (buffer[0] & 0x80) {
        printf("Capteur occupé, statut: 0x%02X\r\n", buffer[0]);
        return AHT20_ERROR_BUSY;
    }

    // Vérifier le checksum (sur les 6 premiers octets: Statut, H_data, T_data)
    uint8_t calculated_crc = AHT20_CalculateChecksum(buffer, 6);
    if (calculated_crc != buffer[6]) {
        printf("Checksum invalide. Données reçues: ");
        for (int i = 0; i < 7; i++) {
            printf("0x%02X ", buffer[i]);
        }
        printf("\r\nChecksum attendu: 0x%02X, Calculé: 0x%02X\r\n", buffer[6], calculated_crc);
        return AHT20_ERROR_CHECKSUM;
    }

    // Convertir les données
    raw_humidity = ((buffer[1] << 12) | (buffer[2] << 4) | (buffer[3] >> 4));
    raw_temperature = ((buffer[3] & 0x0F) << 16) | (buffer[4] << 8) | buffer[5];

    data->humidity = (raw_humidity * 100.0) / 1048576.0;
    data->temperature = ((raw_temperature * 200.0) / 1048576.0) - 50.0;

    return AHT20_OK;
}
