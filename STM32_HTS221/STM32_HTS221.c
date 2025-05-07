#include "STM32_HTS221.h"
#include <stdio.h> // Pour printf

/* Définitions des registres du HTS221 */
#define HTS221_I2C_ADDRESS    (0x5F << 1)  // Adresse I2C pour le HTS221 (format 8 bits)

#define HTS221_WHO_AM_I_REG       0x0F     // Registre d'identification du capteur
#define HTS221_CTRL_REG1          0x20     // Registre de contrôle principal
#define HTS221_HUMIDITY_OUT_L_REG 0x28     // Registre de sortie de l'humidité (LSB)
#define HTS221_HUMIDITY_OUT_H_REG 0x29     // Registre de sortie de l'humidité (MSB)
#define HTS221_TEMP_OUT_L_REG     0x2A     // Registre de sortie de la température (LSB)
#define HTS221_TEMP_OUT_H_REG     0x2B     // Registre de sortie de la température (MSB)

// Registres de calibration
#define HTS221_T0_DEGC_X8_REG       0x32   // Température T0 en °C (multipliée par 8)
#define HTS221_T1_DEGC_X8_REG       0x33   // Température T1 en °C (multipliée par 8)
#define HTS221_T0_T1_DEGC_MSB_REG   0x35   // Bits MSB pour T0 et T1
#define HTS221_T0_OUT_L_REG         0x3C   // Sortie brute T0 (LSB)
#define HTS221_T0_OUT_H_REG         0x3D   // Sortie brute T0 (MSB)
#define HTS221_T1_OUT_L_REG         0x3E   // Sortie brute T1 (LSB)
#define HTS221_T1_OUT_H_REG         0x3F   // Sortie brute T1 (MSB)

#define HTS221_H0_RH_X2_REG         0x30   // Humidité H0 en % (multipliée par 2)
#define HTS221_H1_RH_X2_REG         0x31   // Humidité H1 en % (multipliée par 2)
#define HTS221_H0_T0_OUT_L_REG      0x36   // Sortie brute H0 (LSB)
#define HTS221_H0_T0_OUT_H_REG      0x37   // Sortie brute H0 (MSB)
#define HTS221_H1_T0_OUT_L_REG      0x3A   // Sortie brute H1 (LSB)
#define HTS221_H1_T0_OUT_H_REG      0x3B   // Sortie brute H1 (MSB)

/* Structure pour les données de calibration */
typedef struct {
    float T0_degC;       // Température T0 en °C
    float T1_degC;       // Température T1 en °C
    int16_t T0_OUT;      // Sortie brute pour T0
    int16_t T1_OUT;      // Sortie brute pour T1
    float H0_rh;         // Humidité H0 en %
    float H1_rh;         // Humidité H1 en %
    int16_t H0_T0_OUT;   // Sortie brute pour H0
    int16_t H1_T0_OUT;   // Sortie brute pour H1
} HTS221_CalibrationData;

static HTS221_CalibrationData calib_data; // Instance globale pour stocker les données de calibration

/* Prototypes de fonctions privées */
static uint8_t HTS221_ReadReg(I2C_HandleTypeDef *hi2c, uint8_t reg); // Lecture d'un registre
static void HTS221_WriteReg(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t value); // Écriture dans un registre
static HTS221_Status HTS221_ReadCalibrationData(I2C_HandleTypeDef *hi2c); // Lecture des données de calibration

/**
 * @brief Écrit une valeur dans un registre du HTS221.
 * @param hi2c: Pointeur vers la structure I2C.
 * @param reg: Adresse du registre.
 * @param value: Valeur à écrire.
 */
static void HTS221_WriteReg(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    if (HAL_I2C_Master_Transmit(hi2c, HTS221_I2C_ADDRESS, data, 2, HAL_MAX_DELAY) != HAL_OK) {
        printf("Erreur HTS221: Ecriture registre 0x%02X\r\n", reg);
    }
}

/**
 * @brief Lit une valeur depuis un registre du HTS221.
 * @param hi2c: Pointeur vers la structure I2C.
 * @param reg: Adresse du registre.
 * @retval Valeur lue.
 */
static uint8_t HTS221_ReadReg(I2C_HandleTypeDef *hi2c, uint8_t reg) {
    uint8_t value = 0;
    if (HAL_I2C_Master_Transmit(hi2c, HTS221_I2C_ADDRESS, &reg, 1, HAL_MAX_DELAY) != HAL_OK) {
        printf("Erreur HTS221: Envoi adresse registre 0x%02X\r\n", reg);
        return 0;
    }
    if (HAL_I2C_Master_Receive(hi2c, HTS221_I2C_ADDRESS, &value, 1, HAL_MAX_DELAY) != HAL_OK) {
        printf("Erreur HTS221: Lecture registre 0x%02X\r\n", reg);
        return 0;
    }
    return value;
}

/**
 * @brief Lit les données de calibration du HTS221.
 * @param hi2c: Pointeur vers la structure I2C.
 * @retval Statut de l'opération.
 */
static HTS221_Status HTS221_ReadCalibrationData(I2C_HandleTypeDef *hi2c) {
    // Lecture des températures T0 et T1
    uint8_t t0_degc_x8_val = HTS221_ReadReg(hi2c, HTS221_T0_DEGC_X8_REG);
    uint8_t t1_degc_x8_val = HTS221_ReadReg(hi2c, HTS221_T1_DEGC_X8_REG);
    uint8_t t0_t1_msb_val = HTS221_ReadReg(hi2c, HTS221_T0_T1_DEGC_MSB_REG);

    // Conversion des valeurs brutes en °C
    calib_data.T0_degC = (float)((((uint16_t)(t0_t1_msb_val & 0x03)) << 8) | (uint16_t)t0_degc_x8_val) / 8.0f;
    calib_data.T1_degC = (float)((((uint16_t)(t0_t1_msb_val & 0x0C)) << 6) | (uint16_t)t1_degc_x8_val) / 8.0f;

    // Lecture des sorties brutes pour T0 et T1
    calib_data.T0_OUT = (int16_t)(((uint16_t)HTS221_ReadReg(hi2c, HTS221_T0_OUT_H_REG) << 8) | (uint16_t)HTS221_ReadReg(hi2c, HTS221_T0_OUT_L_REG));
    calib_data.T1_OUT = (int16_t)(((uint16_t)HTS221_ReadReg(hi2c, HTS221_T1_OUT_H_REG) << 8) | (uint16_t)HTS221_ReadReg(hi2c, HTS221_T1_OUT_L_REG));

    // Lecture des humidités H0 et H1
    uint8_t h0_rh_x2_val = HTS221_ReadReg(hi2c, HTS221_H0_RH_X2_REG);
    uint8_t h1_rh_x2_val = HTS221_ReadReg(hi2c, HTS221_H1_RH_X2_REG);

    // Conversion des valeurs brutes en %
    calib_data.H0_rh = (float)h0_rh_x2_val / 2.0f;
    calib_data.H1_rh = (float)h1_rh_x2_val / 2.0f;

    // Lecture des sorties brutes pour H0 et H1
    calib_data.H0_T0_OUT = (int16_t)(((uint16_t)HTS221_ReadReg(hi2c, HTS221_H0_T0_OUT_H_REG) << 8) | (uint16_t)HTS221_ReadReg(hi2c, HTS221_H0_T0_OUT_L_REG));
    calib_data.H1_T0_OUT = (int16_t)(((uint16_t)HTS221_ReadReg(hi2c, HTS221_H1_T0_OUT_H_REG) << 8) | (uint16_t)HTS221_ReadReg(hi2c, HTS221_H1_T0_OUT_L_REG));

    return HTS221_OK;
}

/**
 * @brief Initialise le capteur HTS221.
 * @param hi2c: Pointeur vers la structure I2C.
 * @retval Statut de l'initialisation.
 */
HTS221_Status HTS221_Init(I2C_HandleTypeDef *hi2c) {
    // Vérifie l'identité du capteur
    uint8_t who_am_i = HTS221_ReadReg(hi2c, HTS221_WHO_AM_I_REG);
    if (who_am_i != 0xBC) {
        printf("Erreur: HTS221 non détecté (WHO_AM_I = 0x%02X)\r\n", who_am_i);
        return HTS221_ERROR;
    }
    printf("HTS221 détecté (WHO_AM_I = 0x%02X)\r\n", who_am_i);

    // Active le capteur en mode continu
    HTS221_WriteReg(hi2c, HTS221_CTRL_REG1, 0x81);

    // Lit les données de calibration
    return HTS221_ReadCalibrationData(hi2c);
}

/**
 * @brief Lit la température depuis le capteur HTS221.
 * @param hi2c: Pointeur vers la structure I2C.
 * @retval Température en degrés Celsius.
 */
float HTS221_ReadTemperature(I2C_HandleTypeDef *hi2c) {
    // Lecture des registres de température brute
    uint8_t temp_l = HTS221_ReadReg(hi2c, HTS221_TEMP_OUT_L_REG);
    uint8_t temp_h = HTS221_ReadReg(hi2c, HTS221_TEMP_OUT_H_REG);
    int16_t temp_raw = (int16_t)(((uint16_t)temp_h << 8) | (uint16_t)temp_l);

    // Vérifie la validité des données de calibration
    if (calib_data.T1_OUT == calib_data.T0_OUT) {
        printf("Erreur HTS221: Calibration température invalide\r\n");
        return -273.15f; // Retourne une valeur d'erreur
    }

    // Interpolation linéaire pour calculer la température
    return calib_data.T0_degC + ((float)(temp_raw - calib_data.T0_OUT) * 
           (calib_data.T1_degC - calib_data.T0_degC) / (float)(calib_data.T1_OUT - calib_data.T0_OUT));
}

/**
 * @brief Lit l'humidité depuis le capteur HTS221.
 * @param hi2c: Pointeur vers la structure I2C.
 * @retval Humidité relative en pourcentage.
 */
float HTS221_ReadHumidity(I2C_HandleTypeDef *hi2c) {
    // Lecture des registres d'humidité brute
    uint8_t hum_l = HTS221_ReadReg(hi2c, HTS221_HUMIDITY_OUT_L_REG);
    uint8_t hum_h = HTS221_ReadReg(hi2c, HTS221_HUMIDITY_OUT_H_REG);
    int16_t hum_raw = (int16_t)(((uint16_t)hum_h << 8) | (uint16_t)hum_l);

    // Vérifie la validité des données de calibration
    if (calib_data.H1_T0_OUT == calib_data.H0_T0_OUT) {
        printf("Erreur HTS221: Calibration humidité invalide\r\n");
        return -1.0f; // Retourne une valeur d'erreur
    }

    // Interpolation linéaire pour calculer l'humidité
    float humidity = calib_data.H0_rh + ((float)(hum_raw - calib_data.H0_T0_OUT) * 
                    (calib_data.H1_rh - calib_data.H0_rh) / (float)(calib_data.H1_T0_OUT - calib_data.H0_T0_OUT));

    // Saturation des valeurs entre 0 et 100 %
    if (humidity < 0.0f) humidity = 0.0f;
    if (humidity > 100.0f) humidity = 100.0f;

    return humidity;
}
