/*
 * STM32_DHT20_I2C.c
 *
 *  Created on: May 3, 2025
 *      Author: weedm
 */

#include "STM32_DHT20_I2C.h"

// --- Debug Configuration ---
// Décommentez la ligne suivante pour activer les messages de débogage via printf
//#define DEBUG_ON

#ifdef DEBUG_ON
#include <stdio.h> // Inclure stdio uniquement si le débogage est activé
#define DHT20_PRINTF(...) printf(__VA_ARGS__)
#else
#define DHT20_PRINTF(...) // Se transforme en rien si DEBUG_ON n'est pas défini
#endif

// --- Private Defines ---
#define DHT20_CMD_TRIGGER {0xAC, 0x33, 0x00} // Command to trigger a measurement
#define DHT20_STATUS_BUSY_MASK 0x80         // Mask to check if the sensor is busy
#define DHT20_STATUS_CALIBRATED_MASK 0x08   // Mask to check if the sensor is calibrated

// --- Private Function Prototypes ---
static uint8_t DHT20_CalculateCRC8(uint8_t *data, uint8_t len);

// --- Function Implementations ---

/**
 * @brief Initializes the DHT20 sensor handle.
 */
void DHT20_Init(DHT20_Handle *dht, I2C_HandleTypeDef *hi2c, uint8_t address, uint32_t timeout) {
	dht->hi2c = hi2c;
	dht->address = (address << 1); // Store the shifted address
	dht->i2c_timeout = timeout;
}

/**
 * @brief Calculates the CRC8 for DHT20 data. (Internal function)
 */
static uint8_t DHT20_CalculateCRC8(uint8_t *data, uint8_t len) {
	uint8_t crc = 0xFF; // Initialize with 0xFF
	for (uint8_t i = 0; i < len; i++) {
		crc ^= data[i];
		for (uint8_t j = 0; j < 8; j++) {
			if (crc & 0x80) {
				crc = (crc << 1) ^ 0x31; // Polynomial 0x31 (x^8 + x^5 + x^4 + 1)
			} else {
				crc <<= 1;
			}
		}
	}
	return crc;
}

/**
 * @brief Checks the initial status of the DHT20 sensor.
 */
HAL_StatusTypeDef DHT20_Check_Status(DHT20_Handle *dht) {
	uint8_t status_byte;
	HAL_StatusTypeDef status;

	DHT20_PRINTF("Vérification de l'état initial du DHT20 (Addr: 0x%02X)...\r\n", dht->address >> 1);
	HAL_Delay(100); // Wait for sensor stabilization

	// Attempt to read the status byte directly
	status = HAL_I2C_Master_Receive(dht->hi2c, dht->address, &status_byte, 1, dht->i2c_timeout);

	if (status != HAL_OK) {
		DHT20_PRINTF("Erreur: Impossible de lire le statut initial du DHT20 (Status HAL: %d)\r\n", status);
		if (HAL_I2C_GetError(dht->hi2c) == HAL_I2C_ERROR_AF) {
			DHT20_PRINTF("   -> Le capteur n'a pas repondu (NACK).\r\n");
		}
		return status;
	}

	DHT20_PRINTF("Statut initial du DHT20: 0x%02X\r\n", status_byte);
	DHT20_PRINTF("   -> Calibration: %s\r\n", (status_byte & DHT20_STATUS_CALIBRATED_MASK) ? "OK" : "NON OK (Attention)");

	return HAL_OK;
}

/**
 * @brief Reads temperature and humidity data from the DHT20 sensor.
 */
HAL_StatusTypeDef DHT20_ReadData(DHT20_Handle *dht, float *temperature, float *humidity) {
	uint8_t cmd_trigger[] = DHT20_CMD_TRIGGER;
	uint8_t data[7]; // 1 status + 6 data + 1 CRC
	HAL_StatusTypeDef status;

	// 1. Send trigger measurement command
	status = HAL_I2C_Master_Transmit(dht->hi2c, dht->address, cmd_trigger, sizeof(cmd_trigger), dht->i2c_timeout);
	if (status != HAL_OK) {
		DHT20_PRINTF("Erreur: Echec de l'envoi de la commande de mesure DHT20\r\n");
		return status;
	}

	// 2. Wait for measurement completion (min 80ms)
	HAL_Delay(100); // Wait 100ms to be safe

	// 3. Read 7 bytes (Status, RH, Temp, CRC)
	status = HAL_I2C_Master_Receive(dht->hi2c, dht->address, data, sizeof(data), dht->i2c_timeout);
	if (status != HAL_OK) {
		DHT20_PRINTF("Erreur: Echec de la lecture des données DHT20\r\n");
		return status;
	}

	// 4. Check status byte
	if (data[0] & DHT20_STATUS_BUSY_MASK) {
		DHT20_PRINTF("Erreur: Capteur DHT20 occupé\r\n");
		return HAL_BUSY;
	}
	if (!(data[0] & DHT20_STATUS_CALIBRATED_MASK)) {
		DHT20_PRINTF("Attention: Capteur DHT20 non calibré (Status: 0x%02X)\r\n", data[0]);
		// Decide if this is an error for your application: return HAL_ERROR;
	}

	// 5. Verify CRC
	uint8_t calculated_crc = DHT20_CalculateCRC8(data, 6); // CRC is calculated over the first 6 bytes
	if (calculated_crc != data[6]) {
		DHT20_PRINTF("Erreur: CRC invalide! Calculé: 0x%02X, Reçu: 0x%02X\r\n", calculated_crc, data[6]);
		return HAL_ERROR; // Data integrity error
	}

	// 6. Calculate raw values and convert to Temperature/Humidity
	uint32_t humidity_raw = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (((uint32_t)data[3] & 0xF0) >> 4);
	uint32_t temperature_raw = (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | (uint32_t)data[5];

	*humidity = (float)humidity_raw * 100.0f / (1UL << 20);
	*temperature = (float)temperature_raw * 200.0f / (1UL << 20) - 50.0f;

	return HAL_OK;
}
