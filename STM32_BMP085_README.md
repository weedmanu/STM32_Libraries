# Bibliothèque STM32_BMP085

# Bibliothèque STM32_BMP085

La bibliothèque `STM32_BMP085` est conçue pour faciliter l'intégration du capteur de pression BMP085 avec les microcontrôleurs STM32. Elle fournit des fonctions d'initialisation et de lecture des données de pression et de température.

## Caractéristiques

- Initialisation simple du capteur BMP085.
- Lecture des données de pression et de température.
- Calcul de l'altitude à partir des données de pression.
- Gestion des erreurs de communication avec le capteur.

## Tutoriel d'utilisation

`On adapte le fichier **config.h** selon la carte STM32 et l'I2C utilisée`

On inclue les librairie :

```c
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "STM32_BMP085.h"
/* USER CODE END Includes */
```

puis pour initialiser le capteur :

```c
  /* USER CODE BEGIN 2 */
	HAL_Delay(500);  // Wait for 500 ms to stabilize the sensor
	printf("Testing BMP280...\r\n");
	// Initialisation du capteur avec les paramètres de configuration
	if (!BMP085_Init(CONFIG_MODE, &CONFIG_I2C)) {
		printf("Erreur d'initialisation du capteur\r\n");
		while (1);
	}
  /* USER CODE END 2 */
```

et lire les datas :

```c
  /* USER CODE BEGIN WHILE */
	while (1)
	{
		float Temperature = BMP085_readTemperature();
		int32_t Pressure = BMP085_readPressure();
		float altitude = BMP085_readAltitude(101500);
		int32_t SealevelPressure = BMP085_readSealevelPressure(altitude);

		printf("Temperature = %0.2f\r\n", Temperature);
		printf("Pressure = %0.2f hPa\r\n", (float)Pressure / 100);
		printf("Altitude = %0.2f m\r\n", altitude);
		printf("Sealevel Pressure = %0.2f hPa\r\n", (float)SealevelPressure / 100);
		HAL_Delay(5000);
	}
    /* USER CODE END WHILE */
```

On ajoute le printf sur un uart :

```c
/* USER CODE BEGIN 4 */
// Fonction pour envoyer un caractère via l'UART
int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart2, (uint8_t*)&ch, 1, HAL_MAX_DELAY);
    return ch;
}
/* USER CODE END 4 */
```
