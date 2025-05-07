#include "STM32_HTS221.h"

/* Defines privés pour les registres du HTS221 */
#define HTS221_I2C_ADDRESS    (0x5F << 1)  // Adresse I2C pour le HTS221 (format 8 bits)

#define HTS221_WHO_AM_I_REG       0x0F
#define HTS221_CTRL_REG1          0x20
#define HTS221_HUMIDITY_OUT_L_REG 0x28
#define HTS221_HUMIDITY_OUT_H_REG 0x29
#define HTS221_TEMP_OUT_L_REG     0x2A
#define HTS221_TEMP_OUT_H_REG     0x2B

// Registres de calibration
#define HTS221_T0_DEGC_X8_REG       0x32
#define HTS221_T1_DEGC_X8_REG       0x33
#define HTS221_T0_T1_DEGC_MSB_REG   0x35
#define HTS221_T0_OUT_L_REG         0x3C
#define HTS221_T0_OUT_H_REG         0x3D
#define HTS221_T1_OUT_L_REG         0x3E
#define HTS221_T1_OUT_H_REG         0x3F

#define HTS221_H0_RH_X2_REG         0x30
#define HTS221_H1_RH_X2_REG         0x31
#define HTS221_H0_T0_OUT_L_REG      0x36
#define HTS221_H0_T0_OUT_H_REG      0x37
#define HTS221_H1_T0_OUT_L_REG      0x3A
#define HTS221_H1_T0_OUT_H_REG      0x3B

/* Variables statiques pour les données de calibration */
static float T0_degC = 0.0f;
static float T1_degC = 0.0f;
static int16_t T0_OUT = 0;
static int16_t T1_OUT = 0;
static float H0_rh = 0.0f;
static float H1_rh = 0.0f;
static int16_t H0_T0_OUT = 0;
static int16_t H1_T0_OUT = 0;

/* Prototypes de fonctions privées (statiques) */
static uint8_t HTS221_ReadReg(I2C_HandleTypeDef *hi2c, uint8_t reg);
static void HTS221_WriteReg(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t value);
static void HTS221_ReadCalibrationData(I2C_HandleTypeDef *hi2c);

static void HTS221_WriteReg(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    if (HAL_I2C_Master_Transmit(hi2c, HTS221_I2C_ADDRESS, data, 2, HAL_MAX_DELAY) != HAL_OK) {
        printf("Erreur HTS221: Ecriture registre 0x%02X\r\n", reg);
    }
}

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

static void HTS221_ReadCalibrationData(I2C_HandleTypeDef *hi2c) {
    uint8_t t0_degc_x8_val = HTS221_ReadReg(hi2c, HTS221_T0_DEGC_X8_REG);
    uint8_t t1_degc_x8_val = HTS221_ReadReg(hi2c, HTS221_T1_DEGC_X8_REG);
    uint8_t t0_t1_msb_val = HTS221_ReadReg(hi2c, HTS221_T0_T1_DEGC_MSB_REG);

    T0_degC = (float)((((uint16_t)(t0_t1_msb_val & 0x03)) << 8) | (uint16_t)t0_degc_x8_val) / 8.0f;
    T1_degC = (float)((((uint16_t)(t0_t1_msb_val & 0x0C)) << 6) | (uint16_t)t1_degc_x8_val) / 8.0f;

    T0_OUT = (int16_t)(((uint16_t)HTS221_ReadReg(hi2c, HTS221_T0_OUT_H_REG) << 8) | (uint16_t)HTS221_ReadReg(hi2c, HTS221_T0_OUT_L_REG));
    T1_OUT = (int16_t)(((uint16_t)HTS221_ReadReg(hi2c, HTS221_T1_OUT_H_REG) << 8) | (uint16_t)HTS221_ReadReg(hi2c, HTS221_T1_OUT_L_REG));

    uint8_t h0_rh_x2_val = HTS221_ReadReg(hi2c, HTS221_H0_RH_X2_REG);
    uint8_t h1_rh_x2_val = HTS221_ReadReg(hi2c, HTS221_H1_RH_X2_REG);

    H0_rh = (float)h0_rh_x2_val / 2.0f;
    H1_rh = (float)h1_rh_x2_val / 2.0f;

    H0_T0_OUT = (int16_t)(((uint16_t)HTS221_ReadReg(hi2c, HTS221_H0_T0_OUT_H_REG) << 8) | (uint16_t)HTS221_ReadReg(hi2c, HTS221_H0_T0_OUT_L_REG));
    H1_T0_OUT = (int16_t)(((uint16_t)HTS221_ReadReg(hi2c, HTS221_H1_T0_OUT_H_REG) << 8) | (uint16_t)HTS221_ReadReg(hi2c, HTS221_H1_T0_OUT_L_REG));
}

void HTS221_Init(I2C_HandleTypeDef *hi2c) {
    uint8_t who_am_i = HTS221_ReadReg(hi2c, HTS221_WHO_AM_I_REG);
    if (who_am_i != 0xBC) {
        printf("Erreur: HTS221 non détecté (WHO_AM_I = 0x%02X)\r\n", who_am_i);
        return;
    }
    printf("HTS221 détecté (WHO_AM_I = 0x%02X)\r\n", who_am_i);

    // Active le capteur, ODR = 1Hz (PD=1, BDU=0, ODR=01 -> 0x81)
    // Le code original utilisait 0x80 (mode one-shot). 0x81 est pour des lectures continues.
    HTS221_WriteReg(hi2c, HTS221_CTRL_REG1, 0x81);

    HTS221_ReadCalibrationData(hi2c);
}

float HTS221_ReadTemperature(I2C_HandleTypeDef *hi2c) {
    uint8_t temp_l = HTS221_ReadReg(hi2c, HTS221_TEMP_OUT_L_REG);
    uint8_t temp_h = HTS221_ReadReg(hi2c, HTS221_TEMP_OUT_H_REG);
    int16_t temp_raw = (int16_t)(((uint16_t)temp_h << 8) | (uint16_t)temp_l);

    if (T1_OUT == T0_OUT) { // Éviter la division par zéro
        printf("Erreur HTS221: Calibration température invalide (T1_OUT == T0_OUT)\r\n");
        return -273.15f; // Retourner une valeur anormale
    }

    // Interpolation linéaire
    float temperature = T0_degC + ((float)(temp_raw - T0_OUT) * (T1_degC - T0_degC) / (float)(T1_OUT - T0_OUT));
    return temperature;
}

float HTS221_ReadHumidity(I2C_HandleTypeDef *hi2c) {
    uint8_t hum_l = HTS221_ReadReg(hi2c, HTS221_HUMIDITY_OUT_L_REG);
    uint8_t hum_h = HTS221_ReadReg(hi2c, HTS221_HUMIDITY_OUT_H_REG);
    int16_t hum_raw = (int16_t)(((uint16_t)hum_h << 8) | (uint16_t)hum_l);

    if (H1_T0_OUT == H0_T0_OUT) { // Éviter la division par zéro
        printf("Erreur HTS221: Calibration humidité invalide (H1_T0_OUT == H0_T0_OUT)\r\n");
        return -1.0f; // Retourner une valeur anormale
    }

    // Interpolation linéaire
    float humidity = H0_rh + ((float)(hum_raw - H0_T0_OUT) * (H1_rh - H0_rh) / (float)(H1_T0_OUT - H0_T0_OUT));

    // Saturation aux limites physiques (0-100% RH)
    if (humidity < 0.0f) humidity = 0.0f;
    if (humidity > 100.0f) humidity = 100.0f;

    return humidity;
}
