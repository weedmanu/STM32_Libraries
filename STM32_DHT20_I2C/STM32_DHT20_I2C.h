/*
 * STM32_DHT20_I2C.h
 *
 *  Created on: May 3, 2025
 *      Author: weedm
 */

#ifndef INC_STM32_DHT20_I2C_H_
#define INC_STM32_DHT20_I2C_H_

#include "main.h" // Change stm32l4xx_hal.h if you use a different STM32 series
#include <stdint.h>

// Define the default I2C address for the DHT20 sensor
#define DHT20_SENSOR_ADDR  (0x38 << 1) // Shifted left to match STM32 HAL requirements

/**
 * @brief DHT20 Sensor Handle Structure.
 *        Contains the I2C handle and the device address.
 */
typedef struct {
    I2C_HandleTypeDef *hi2c;     // Pointer to the I2C handle initialized in main.c
    uint32_t          i2c_timeout; // Timeout for I2C operations in ms
} DHT20_Handle;

/**
 * @brief Initializes the DHT20 sensor handle.
 * @param dht Pointer to the DHT20_Handle structure.
 * @param hi2c Pointer to the I2C_HandleTypeDef structure.
 * @param timeout I2C communication timeout in milliseconds.
 */
void DHT20_Init(DHT20_Handle *dht, I2C_HandleTypeDef *hi2c, uint32_t timeout);

/**
 * @brief Checks the initial status of the DHT20 sensor after power-on.
 * @param dht Pointer to the initialized DHT20_Handle structure.
 * @retval HAL_StatusTypeDef HAL status of the I2C communication.
 */
HAL_StatusTypeDef DHT20_Check_Status(DHT20_Handle *dht);

/**
 * @brief Reads temperature and humidity data from the DHT20 sensor.
 * @param dht Pointer to the initialized DHT20_Handle structure.
 * @param temperature Pointer to a float variable to store the temperature in Celsius.
 * @param humidity Pointer to a float variable to store the relative humidity in %.
 * @retval HAL_StatusTypeDef HAL status of the operation (HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT).
 */
HAL_StatusTypeDef DHT20_ReadData(DHT20_Handle *dht, float *temperature, float *humidity);

#endif /* INC_STM32_DHT20_I2C_H_ */


