# Bibliothèque STM32 HAL pour Capteur de Température et d'Humidité HTS221

Cette bibliothèque permet de lire les données de température et d'humidité du capteur HTS221 en utilisant un microcontrôleur STM32 et la bibliothèque HAL.

## Fonctionnalités

- Initialisation du capteur HTS221.
- Lecture de la température en degrés Celsius (°C).
- Lecture de l'humidité relative en pourcentage (%).
- Gestion des erreurs de calibration et de communication.
- Interpolation linéaire pour convertir les données brutes en valeurs physiques.

## Connexions Matérielles

Connectez le capteur HTS221 à votre microcontrôleur STM32 comme suit :

- **VCC** -> 3.3V.
- **GND** -> GND.
- **SCL** -> Broche SCL I2C du STM32 (ex: PB6, PB8, PB10 selon le MCU et la configuration I2C).
- **SDA** -> Broche SDA I2C du STM32 (ex: PB7, PB9, PB11 selon le MCU et la configuration I2C).

Assurez-vous d'activer le périphérique I2C approprié dans votre configuration STM32CubeMX ou manuellement.

## Utilisation de base

Voici un exemple simple d'utilisation dans votre fichier `main.c` :

### 1. Inclure l'en-tête

```c
/* USER CODE BEGIN Includes */
#include <stdio.h>           // Pour printf
#include "STM32_HTS221.h"    // Inclure la bibliothèque HTS221
/* USER CODE END Includes */
```

### 2. Initialisation du capteur

```c
/* USER CODE BEGIN 2 */
printf("Test STM32_HTS221\r\n");

// Initialisation du capteur HTS221
HTS221_Init(&hi2c1); // Remplacez `hi2c1` par le handle I2C configuré pour votre projet
/* USER CODE END 2 */
```

### 3. Lecture des données

```c
/* USER CODE BEGIN WHILE */
while (1) {
    // Lire la température
    float temperature = HTS221_ReadTemperature(&hi2c1);
    if (temperature > -273.0f) { // Vérifie si la lecture est valide
        printf("Température: %.2f °C\r\n", temperature);
    } else {
        printf("Erreur de lecture de la température.\r\n");
    }

    // Lire l'humidité
    float humidity = HTS221_ReadHumidity(&hi2c1);
    if (humidity >= 0.0f) { // Vérifie si la lecture est valide
        printf("Humidité: %.2f %%\r\n", humidity);
    } else {
        printf("Erreur de lecture de l'humidité.\r\n");
    }

    HAL_Delay(2000); // Attendre 2 secondes avant la prochaine lecture
}
/* USER CODE END WHILE */
```

## Référence API

### Fonctions Principales

#### `void HTS221_Init(I2C_HandleTypeDef *hi2c)`

- Initialise le capteur HTS221 et lit les données de calibration.
- Affiche une erreur si le capteur n'est pas détecté.

#### `float HTS221_ReadTemperature(I2C_HandleTypeDef *hi2c)`

- Lit et retourne la température en degrés Celsius.
- Retourne `-273.15` en cas d'erreur.

#### `float HTS221_ReadHumidity(I2C_HandleTypeDef *hi2c)`

- Lit et retourne l'humidité relative en pourcentage.
- Retourne `-1.0` en cas d'erreur.

### Codes d'Erreur

- `-273.15`: Erreur de lecture ou calibration invalide pour la température.
- `-1.0`: Erreur de lecture ou calibration invalide pour l'humidité.