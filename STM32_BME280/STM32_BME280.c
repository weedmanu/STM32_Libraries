/*
 * STM32_BME280.c
 *
 *  Créé le : 3 mai 2025
 *      Auteur : weedm
 */

#include "STM32_BME280.h"

#ifdef DEBUG_ON
    #include <stdio.h> // Inclure stdio.h pour utiliser printf
    #define DEBUG_PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__) // Corriger la définition
#else
    #define DEBUG_PRINT(fmt, ...) // Ne rien faire si DEBUG n'est pas défini
#endif

// --- Fonctions d'assistance privées ---

/**
 * @brief Écrit un octet dans un registre spécifique du BME280.
 * @param dev Pointeur vers la structure de gestion du périphérique BME280.
 * @param reg Adresse du registre à écrire.
 * @param value Valeur de l'octet à écrire.
 * @retval Statut HAL.
 */
static HAL_StatusTypeDef BME280_WriteByte(BME280_Handle_t *dev, uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(dev->hi2c, (dev->dev_addr << 1), data, 2, HAL_MAX_DELAY); // Adresse décalée pour HAL
    if (status != HAL_OK) {
        DEBUG_PRINT("Erreur lors de l'écriture dans le registre 0x%02X (valeur: 0x%02X)\r\n", reg, value);
    }
    return status;
}

/**
 * @brief Lit un octet d'un registre spécifique du BME280.
 * @param dev Pointeur vers la structure de gestion du périphérique BME280.
 * @param reg Adresse du registre à lire.
 * @param value Pointeur pour stocker la valeur lue.
 * @retval Statut HAL.
 */
static HAL_StatusTypeDef BME280_ReadByte(BME280_Handle_t *dev, uint8_t reg, uint8_t *value) {
    HAL_StatusTypeDef status;
    status = HAL_I2C_Master_Transmit(dev->hi2c, (dev->dev_addr << 1), &reg, 1, HAL_MAX_DELAY); // Adresse décalée pour HAL
    if (status != HAL_OK) {
        DEBUG_PRINT("Erreur lors de l'envoi de l'adresse du registre 0x%02X\r\n", reg);
        return status;
    }
    status = HAL_I2C_Master_Receive(dev->hi2c, (dev->dev_addr << 1), value, 1, HAL_MAX_DELAY); // Adresse décalée pour HAL
    if (status != HAL_OK) {
        DEBUG_PRINT("Erreur lors de la lecture du registre 0x%02X\r\n", reg);
    } else {
        DEBUG_PRINT("Lecture réussie : registre 0x%02X = 0x%02X\r\n", reg, *value);
    }
    return status;
}

/**
 * @brief Lit plusieurs octets à partir de registres séquentiels du BME280.
 * @param dev Pointeur vers la structure de gestion du périphérique BME280.
 * @param reg Adresse du premier registre à lire.
 * @param buffer Pointeur vers le tampon pour stocker les données lues.
 * @param len Nombre d'octets à lire.
 * @retval Statut HAL.
 */
static HAL_StatusTypeDef BME280_ReadBytes(BME280_Handle_t *dev, uint8_t reg, uint8_t *buffer, uint16_t len) {
    HAL_StatusTypeDef status;
    status = HAL_I2C_Master_Transmit(dev->hi2c, (dev->dev_addr << 1), &reg, 1, HAL_MAX_DELAY); // Adresse décalée pour HAL
    if (status != HAL_OK) {
        DEBUG_PRINT("Erreur lors de l'envoi de l'adresse du registre 0x%02X pour lecture multiple\r\n", reg);
        return status;
    }
    status = HAL_I2C_Master_Receive(dev->hi2c, (dev->dev_addr << 1), buffer, len, HAL_MAX_DELAY); // Adresse décalée pour HAL
    if (status != HAL_OK) {
        DEBUG_PRINT("Erreur lors de la lecture multiple à partir du registre 0x%02X\r\n", reg);
    } else {
        DEBUG_PRINT("Lecture multiple réussie à partir du registre 0x%02X (taille : %d octets)\r\n", reg, len);
    }
    return status;
}

/**
 * @brief Lit les coefficients de calibration du BME280.
 * @param dev Pointeur vers la structure de gestion du périphérique BME280.
 * @retval Statut HAL.
 */
static HAL_StatusTypeDef BME280_ReadCoefficients(BME280_Handle_t *dev) {
    uint8_t buffer[26]; // Tampon pour lire T1-P9 (26 octets)
    uint8_t h_buffer[8]; // Tampon pour H1-H6 (8 octets au total, non contigus)
    HAL_StatusTypeDef status;

    // Lire T1-P9 (registres 0x88 à 0x9F + 0xA1)
    status = BME280_ReadBytes(dev, BME280_REGISTER_DIG_T1, buffer, 24); // T1-P9 sont contigus (0x88 à 0x9F)
    if (status != HAL_OK) return status;

    // Extraction des coefficients de calibration pour la température et la pression
    dev->calib_data.dig_T1 = (uint16_t)(((uint16_t)buffer[1] << 8) | buffer[0]);
    dev->calib_data.dig_T2 = (int16_t)(((int16_t)buffer[3] << 8) | buffer[2]);
    dev->calib_data.dig_T3 = (int16_t)(((int16_t)buffer[5] << 8) | buffer[4]);
    dev->calib_data.dig_P1 = (uint16_t)(((uint16_t)buffer[7] << 8) | buffer[6]);
    dev->calib_data.dig_P2 = (int16_t)(((int16_t)buffer[9] << 8) | buffer[8]);
    dev->calib_data.dig_P3 = (int16_t)(((int16_t)buffer[11] << 8) | buffer[10]);
    dev->calib_data.dig_P4 = (int16_t)(((int16_t)buffer[13] << 8) | buffer[12]);
    dev->calib_data.dig_P5 = (int16_t)(((int16_t)buffer[15] << 8) | buffer[14]);
    dev->calib_data.dig_P6 = (int16_t)(((int16_t)buffer[17] << 8) | buffer[16]);
    dev->calib_data.dig_P7 = (int16_t)(((int16_t)buffer[19] << 8) | buffer[18]);
    dev->calib_data.dig_P8 = (int16_t)(((int16_t)buffer[21] << 8) | buffer[20]);
    dev->calib_data.dig_P9 = (int16_t)(((int16_t)buffer[23] << 8) | buffer[22]);

    // Lire H1 (0xA1)
    status = BME280_ReadByte(dev, BME280_REGISTER_DIG_H1, &dev->calib_data.dig_H1);
    if (status != HAL_OK) return status;

    // Lire H2-H6 (registres 0xE1 à 0xE7)
    status = BME280_ReadBytes(dev, BME280_REGISTER_DIG_H2, h_buffer, 7); // Lire 7 octets de 0xE1 à 0xE7
    if (status != HAL_OK) return status;

    // Extraction des coefficients de calibration pour l'humidité
    dev->calib_data.dig_H2 = (int16_t)(((int16_t)h_buffer[1] << 8) | h_buffer[0]); // H2 (0xE1, 0xE2)
    dev->calib_data.dig_H3 = h_buffer[2];                                         // H3 (0xE3)
    dev->calib_data.dig_H4 = (int16_t)(((int16_t)h_buffer[3] << 4) | (h_buffer[4] & 0x0F)); // H4 (0xE4, 0xE5[3:0])
    dev->calib_data.dig_H5 = (int16_t)(((int16_t)h_buffer[5] << 4) | (h_buffer[4] >> 4));   // H5 (0xE6, 0xE5[7:4])
    dev->calib_data.dig_H6 = (int8_t)h_buffer[6];                                          // H6 (0xE7)

    return HAL_OK;
}

// --- Vérification de l'état I2C ---
static HAL_StatusTypeDef BME280_CheckI2CStatus(HAL_StatusTypeDef status) {
    if (status != HAL_OK) {
        DEBUG_PRINT("Erreur I2C : %d\r\n", status);
    }
    return status;
}

// --- Fonctions API publiques ---

/**
 * @brief Initialise le capteur BME280.
 * @param dev Pointeur vers la structure de gestion du périphérique BME280.
 * @param hi2c Pointeur vers la structure de gestion I2C.
 * @param dev_addr Adresse I2C du périphérique.
 * @retval Statut HAL.
 * @param config Pointeur vers la structure de configuration. Si NULL, utilise les valeurs par défaut.
 * @retval BME280_OK si succès, sinon un code d'erreur BME280_ERROR_xxx.
 */
int8_t BME280_Init(BME280_Handle_t *dev, I2C_HandleTypeDef *hi2c, uint8_t dev_addr, BME280_Config_t *config) {
    dev->hi2c = hi2c;
    dev->dev_addr = dev_addr;
    dev->t_fine = 0;

    uint8_t chip_id;
    HAL_StatusTypeDef status;

    // Configuration par défaut si non fournie
    BME280_Config_t default_config;
    if (config == NULL) {
        default_config.mode = BME280_MODE_NORMAL;
        default_config.oversampling_p = BME280_OVERSAMPLING_X1;
        default_config.oversampling_t = BME280_OVERSAMPLING_X1;
        default_config.oversampling_h = BME280_OVERSAMPLING_X1;
        default_config.filter = BME280_FILTER_OFF;
        default_config.standby_time = BME280_STANDBY_1000_MS;
        dev->config = default_config;
        DEBUG_PRINT("Utilisation de la configuration par défaut.\r\n");
    } else {
        dev->config = *config;
        DEBUG_PRINT("Utilisation de la configuration fournie.\r\n");
    }

    // Vérifier la communication et l'ID de la puce
    if (HAL_I2C_IsDeviceReady(dev->hi2c, (dev->dev_addr << 1), 2, 100) != HAL_OK) {
        DEBUG_PRINT("Erreur : Périphérique I2C non trouvé à l'adresse 0x%02X.\r\n", dev->dev_addr);
        return BME280_ERROR_COMM;
    }

    // Lire l'ID de la puce
    status = BME280_ReadByte(dev, BME280_REGISTER_CHIPID, &chip_id);
    if (status != HAL_OK) {
        return BME280_ERROR_COMM;
    }

    if (chip_id != 0x60) { // L'ID attendu pour le BME280 est 0x60
        DEBUG_PRINT("ID de puce incorrect : 0x%02X (attendu : 0x60)\r\n", chip_id);
        return BME280_ERROR_CHIP_ID;
    }

    // Lire les coefficients de calibration
    status = BME280_ReadCoefficients(dev);
    if (status != HAL_OK) {
        return BME280_ERROR_COMM;
    }

    // Réinitialiser le capteur
    status = BME280_WriteByte(dev, BME280_REGISTER_SOFTRESET, 0xB6);
    if (status != HAL_OK) {
        return BME280_ERROR_COMM;
    }
    HAL_Delay(10); // Attendre que le reset soit effectif

    // Écrire la configuration
    // 1. Configuration de l'humidité (doit être écrit avant ctrl_meas)
    uint8_t ctrl_hum = dev->config.oversampling_h & 0x07;
    status = BME280_WriteByte(dev, BME280_REGISTER_CONTROLHUMID, ctrl_hum);
    if (status != HAL_OK) {
        DEBUG_PRINT("Erreur lors de l'écriture dans BME280_REGISTER_CONTROLHUMID.\r\n");
        return BME280_ERROR_CONFIG;
    }

    // 2. Configuration du filtre et du temps d'attente
    uint8_t config_reg = ((dev->config.standby_time & 0x07) << 5) | ((dev->config.filter & 0x07) << 2);
    status = BME280_WriteByte(dev, BME280_REGISTER_CONFIG, config_reg);
     if (status != HAL_OK) {
        DEBUG_PRINT("Erreur lors de l'écriture dans BME280_REGISTER_CONFIG.\r\n");
        return BME280_ERROR_CONFIG;
    }

    // 3. Configuration du suréchantillonnage T/P et du mode
    // Note: Le mode est écrit en dernier. Si on écrit FORCED, une mesure démarre.
    uint8_t ctrl_meas = ((dev->config.oversampling_t & 0x07) << 5) | ((dev->config.oversampling_p & 0x07) << 2) | (dev->config.mode & 0x03);
    status = BME280_WriteByte(dev, BME280_REGISTER_CONTROL, ctrl_meas);
    if (status != HAL_OK) {
        DEBUG_PRINT("Erreur lors de l'écriture dans BME280_REGISTER_CONTROL.\r\n");
        return BME280_ERROR_CONFIG;
    }

    DEBUG_PRINT("Initialisation réussie du BME280.\r\n");
    return BME280_OK;
}

HAL_StatusTypeDef BME280_ReadChipID(BME280_Handle_t *dev, uint8_t *chip_id) {
    return BME280_ReadByte(dev, BME280_REGISTER_CHIPID, chip_id);
}

HAL_StatusTypeDef BME280_Reset(BME280_Handle_t *dev) {
    return BME280_WriteByte(dev, BME280_REGISTER_SOFTRESET, 0xB6);
}

/**
 * @brief Définit le mode de fonctionnement du capteur.
 * @param dev Pointeur vers la structure de gestion du capteur.
 * @param mode Le mode désiré (BME280_MODE_SLEEP, BME280_MODE_FORCED, BME280_MODE_NORMAL).
 * @retval Statut HAL.
 */
HAL_StatusTypeDef BME280_SetMode(BME280_Handle_t *dev, BME280_Mode_t mode) {
    uint8_t ctrl_meas;
    HAL_StatusTypeDef status;

    // Lire la valeur actuelle de ctrl_meas pour ne pas écraser les réglages d'oversampling
    status = BME280_ReadByte(dev, BME280_REGISTER_CONTROL, &ctrl_meas);
    if (status != HAL_OK) return status;

    // Modifier uniquement les bits de mode
    ctrl_meas = (ctrl_meas & 0xFC) | (mode & 0x03);
    dev->config.mode = mode; // Mettre à jour la configuration stockée
    return BME280_WriteByte(dev, BME280_REGISTER_CONTROL, ctrl_meas);
}

HAL_StatusTypeDef BME280_ReadTemperature(BME280_Handle_t *dev, float *temperature) {
    uint8_t buffer[3];
    HAL_StatusTypeDef status = BME280_ReadBytes(dev, BME280_REGISTER_TEMPDATA, buffer, 3);
    if (BME280_CheckI2CStatus(status) != HAL_OK) return status;

    int32_t adc_T = (int32_t)(((uint32_t)buffer[0] << 12) | ((uint32_t)buffer[1] << 4) | ((uint32_t)buffer[2] >> 4));

    // Formule de compensation du datasheet
    int32_t var1 = (int32_t)((adc_T / 8) - ((int32_t)dev->calib_data.dig_T1 * 2));
    var1 = (var1 * ((int32_t)dev->calib_data.dig_T2)) / 2048;
    int32_t var2 = (int32_t)((adc_T / 16) - ((int32_t)dev->calib_data.dig_T1));
    var2 = (((var2 * var2) / 4096) * ((int32_t)dev->calib_data.dig_T3)) / 16384;
    dev->t_fine = var1 + var2;
    *temperature = (float)((dev->t_fine * 5 + 128) / 256) / 100.0f;

    return HAL_OK;
}

HAL_StatusTypeDef BME280_ReadPressure(BME280_Handle_t *dev, float *pressure) {
    uint8_t buffer[3];
    HAL_StatusTypeDef status = BME280_ReadBytes(dev, BME280_REGISTER_PRESSUREDATA, buffer, 3);
    if (status != HAL_OK) return status;

    int32_t adc_P = (int32_t)(((uint32_t)buffer[0] << 12) | ((uint32_t)buffer[1] << 4) | ((uint32_t)buffer[2] >> 4));

    // S'assurer que t_fine est calculé (généralement en lisant d'abord la température)
    if (dev->t_fine == 0) {
        float temp;
        status = BME280_ReadTemperature(dev, &temp); // Calculer t_fine
        if (status != HAL_OK) return status;
    }

    // Formule de compensation du datasheet
    int64_t var1 = ((int64_t)dev->t_fine) - 128000;
    int64_t var2 = var1 * var1 * (int64_t)dev->calib_data.dig_P6;
    var2 = var2 + ((var1 * (int64_t)dev->calib_data.dig_P5) << 17); // * 131072
    var2 = var2 + (((int64_t)dev->calib_data.dig_P4) << 35); // * 34359738368
    var1 = ((var1 * var1 * (int64_t)dev->calib_data.dig_P3) >> 8) + ((var1 * (int64_t)dev->calib_data.dig_P2) << 12); // * 4096
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dev->calib_data.dig_P1) >> 33; // * 140737488355328 / 8589934592

    if (var1 == 0) {
        *pressure = 0; // Éviter la division par zéro
        return HAL_OK;
    }

    int64_t p_acc = 1048576 - adc_P;
    p_acc = (((p_acc << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dev->calib_data.dig_P9) * (p_acc >> 13) * (p_acc >> 13)) >> 25; // / 33554432
    var2 = (((int64_t)dev->calib_data.dig_P8) * p_acc) >> 19; // / 524288

    p_acc = ((p_acc + var1 + var2) >> 8) + (((int64_t)dev->calib_data.dig_P7) << 4); // * 16

    *pressure = (float)p_acc / 256.0f; // Pression en Pa

    return HAL_OK;
}

HAL_StatusTypeDef BME280_ReadHumidity(BME280_Handle_t *dev, float *humidity) {
    uint8_t buffer[2];
    HAL_StatusTypeDef status = BME280_ReadBytes(dev, BME280_REGISTER_HUMIDDATA, buffer, 2);
    if (status != HAL_OK) return status;

    int32_t adc_H = (int32_t)(((uint32_t)buffer[0] << 8) | (uint32_t)buffer[1]);

    if (adc_H == 0x8000) { // La valeur indique que la mesure a été ignorée
        *humidity = 0.0f / 0.0f; // Retourner NaN ou un indicateur de données invalides
        return HAL_ERROR; // Ou retourner HAL_OK mais indiquer des données invalides via NaN
    }

    // S'assurer que t_fine est calculé
    if (dev->t_fine == 0) {
        float temp;
        status = BME280_ReadTemperature(dev, &temp); // Calculer t_fine
        if (status != HAL_OK) return status;
    }

    // Formule de compensation du datasheet
    int32_t v_x1_u32r = (dev->t_fine - ((int32_t)76800));
    v_x1_u32r = (((((adc_H << 14) - (((int32_t)dev->calib_data.dig_H4) << 20) - (((int32_t)dev->calib_data.dig_H5) * v_x1_u32r)) +
                  ((int32_t)16384)) >> 15) * (((((((v_x1_u32r * ((int32_t)dev->calib_data.dig_H6)) >> 10) * (((v_x1_u32r *
                  ((int32_t)dev->calib_data.dig_H3)) >> 11) + ((int32_t)32768))) >> 10) + ((int32_t)2097152)) *
                  ((int32_t)dev->calib_data.dig_H2) + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)dev->calib_data.dig_H1)) >> 4));
    v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
    v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r); // Max 100% RH * 2^22

    *humidity = (float)(v_x1_u32r >> 12) / 1024.0f; // Humidité en %RH

    return HAL_OK;
}

/**
 * @brief Lit toutes les données (température, pression, humidité) du capteur.
 * @param dev Pointeur vers la structure de gestion du périphérique BME280.
 * @param temperature Pointeur pour stocker la température lue.
 * @param pressure Pointeur pour stocker la pression lue.
 * @param humidity Pointeur pour stocker l'humidité lue.
 * @retval Statut HAL.
 * @retval BME280_OK si la lecture réussit, sinon un code d'erreur BME280_ERROR_xxx.
 */
int8_t BME280_ReadAll(BME280_Handle_t *dev, float *temperature, float *pressure, float *humidity) {
    if (dev == NULL || temperature == NULL || pressure == NULL || humidity == NULL) {
        DEBUG_PRINT("Paramètre invalide fourni à BME280_ReadAll.\r\n");
        return BME280_ERROR_PARAM;
    }

    HAL_StatusTypeDef status;
    uint8_t buffer[8]; // Buffer pour lire Pression(3), Température(3), Humidité(2)

    // Si en mode FORCED, déclencher une mesure et attendre
    if (dev->config.mode == BME280_MODE_FORCED) {
        int8_t forced_status = BME280_TriggerForcedMeasurement(dev, 100); // Timeout de 100ms
        if (forced_status != BME280_OK) {
            return forced_status; // Retourne BME280_ERROR_COMM ou HAL_TIMEOUT
        }
    }

    // Lecture groupée des données de 0xF7 à 0xFE (8 octets)
    status = BME280_ReadBytes(dev, BME280_REGISTER_PRESSUREDATA, buffer, 8);
    if (status != HAL_OK) {
        DEBUG_PRINT("Erreur lors de la lecture groupée des données.\r\n");
        return BME280_ERROR_COMM;
    }

    // Extraire les données brutes ADC
    int32_t adc_P = (int32_t)(((uint32_t)buffer[0] << 12) | ((uint32_t)buffer[1] << 4) | ((uint32_t)buffer[2] >> 4));
    int32_t adc_T = (int32_t)(((uint32_t)buffer[3] << 12) | ((uint32_t)buffer[4] << 4) | ((uint32_t)buffer[5] >> 4));
    int32_t adc_H = (int32_t)(((uint32_t)buffer[6] << 8) | (uint32_t)buffer[7]);

    // --- Compensation Température (doit être faite en premier pour t_fine) ---
    int32_t var1_T = (int32_t)((adc_T / 8) - ((int32_t)dev->calib_data.dig_T1 * 2));
    var1_T = (var1_T * ((int32_t)dev->calib_data.dig_T2)) / 2048;
    int32_t var2_T = (int32_t)((adc_T / 16) - ((int32_t)dev->calib_data.dig_T1));
    var2_T = (((var2_T * var2_T) / 4096) * ((int32_t)dev->calib_data.dig_T3)) / 16384;
    dev->t_fine = var1_T + var2_T;
    *temperature = (float)((dev->t_fine * 5 + 128) / 256) / 100.0f;

    // --- Compensation Pression ---
    int64_t var1_P = ((int64_t)dev->t_fine) - 128000;
    int64_t var2_P = var1_P * var1_P * (int64_t)dev->calib_data.dig_P6;
    var2_P = var2_P + ((var1_P * (int64_t)dev->calib_data.dig_P5) << 17);
    var2_P = var2_P + (((int64_t)dev->calib_data.dig_P4) << 35);
    var1_P = ((var1_P * var1_P * (int64_t)dev->calib_data.dig_P3) >> 8) + ((var1_P * (int64_t)dev->calib_data.dig_P2) << 12);
    var1_P = (((((int64_t)1) << 47) + var1_P)) * ((int64_t)dev->calib_data.dig_P1) >> 33;

    if (var1_P == 0) {
        *pressure = 0; // Éviter la division par zéro
    } else {
        int64_t p_acc = 1048576 - adc_P;
        p_acc = (((p_acc << 31) - var2_P) * 3125) / var1_P;
        var1_P = (((int64_t)dev->calib_data.dig_P9) * (p_acc >> 13) * (p_acc >> 13)) >> 25;
        var2_P = (((int64_t)dev->calib_data.dig_P8) * p_acc) >> 19;
        p_acc = ((p_acc + var1_P + var2_P) >> 8) + (((int64_t)dev->calib_data.dig_P7) << 4);
        *pressure = (float)p_acc / 256.0f; // Pression en Pa
    }

    // --- Compensation Humidité ---
    if (adc_H == 0x8000) { // Mesure ignorée ou invalide
         *humidity = 0.0f / 0.0f; // NaN
         // On pourrait retourner une erreur ici, mais on a déjà lu T et P
         // return BME280_ERROR; // Ou un code spécifique
    } else {
        int32_t v_x1_u32r = (dev->t_fine - ((int32_t)76800));
        v_x1_u32r = (((((adc_H << 14) - (((int32_t)dev->calib_data.dig_H4) << 20) - (((int32_t)dev->calib_data.dig_H5) * v_x1_u32r)) +
                      ((int32_t)16384)) >> 15) * (((((((v_x1_u32r * ((int32_t)dev->calib_data.dig_H6)) >> 10) * (((v_x1_u32r *
                      ((int32_t)dev->calib_data.dig_H3)) >> 11) + ((int32_t)32768))) >> 10) + ((int32_t)2097152)) *
                      ((int32_t)dev->calib_data.dig_H2) + 8192) >> 14));
        v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)dev->calib_data.dig_H1)) >> 4));
        v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
        v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r); // Max 100% RH * 2^22
        *humidity = (float)(v_x1_u32r >> 12) / 1024.0f; // Humidité en %RH
    }

    return BME280_OK;
}

/**
 * @brief Déclenche une mesure en mode forcé et attend la fin.
 * @param dev Pointeur vers la structure de gestion du capteur.
 * @param timeout Temps maximum d'attente en ms.
 * @retval BME280_OK si la mesure est terminée, BME280_ERROR_COMM en cas d'erreur I2C, HAL_TIMEOUT si le délai est dépassé.
 */
int8_t BME280_TriggerForcedMeasurement(BME280_Handle_t *dev, uint32_t timeout) {
    HAL_StatusTypeDef status;
    uint8_t reg_status;
    uint32_t start_time = HAL_GetTick();

    // Déclencher la mesure en écrivant le mode FORCED
    status = BME280_SetMode(dev, BME280_MODE_FORCED);
    if (status != HAL_OK) {
        DEBUG_PRINT("Erreur lors du déclenchement du mode forcé.\r\n");
        return BME280_ERROR_COMM;
    }

    // Attendre la fin de la mesure en vérifiant le bit 'measuring' (bit 3) du registre STATUS (0xF3)
    do {
        status = BME280_ReadByte(dev, BME280_REGISTER_STATUS, &reg_status);
        if (status != HAL_OK) {
            DEBUG_PRINT("Erreur lors de la lecture du statut pendant l'attente.\r\n");
            return BME280_ERROR_COMM;
        }
        // Vérifier si le bit 'measuring' est à 0 (mesure terminée)
        if ((reg_status & (1 << 3)) == 0) {
            return BME280_OK;
        }
        // Vérifier le timeout
        if ((HAL_GetTick() - start_time) > timeout) {
            DEBUG_PRINT("Timeout en attente de la fin de la mesure forcée.\r\n");
            return HAL_TIMEOUT; // Utiliser HAL_TIMEOUT ici car c'est une attente
        }
        HAL_Delay(1); // Petite pause pour ne pas saturer le bus I2C
    } while (1);
}
