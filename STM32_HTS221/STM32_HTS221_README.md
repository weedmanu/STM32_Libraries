# Capteur d'Humidité et de Température HTS221 et Bibliothèque STM32

Ce document décrit le capteur HTS221 et la bibliothèque C fournie pour interagir avec lui en utilisant un microcontrôleur STM32 via I2C.

## 1. Introduction au Capteur HTS221

Le HTS221 est un capteur numérique d'humidité relative et de température. Il est conçu pour fournir des mesures précises et stables dans une large gamme de conditions environnementales.

**Caractéristiques principales :**
*   Interface de communication : I2C et SPI (cette bibliothèque utilise I2C).
*   Plage de mesure d'humidité : 0-100 % HR.
*   Plage de mesure de température : -40 à +120 °C.
*   Faible consommation d'énergie.
*   Données de calibration stockées en interne.

## 2. Aperçu de la Bibliothèque `STM32_HTS221`

La bibliothèque `STM32_HTS221` simplifie l'interaction avec le capteur HTS221 sur les microcontrôleurs STM32. Elle gère l'initialisation, la lecture des données de calibration, et la récupération des mesures de température et d'humidité.

**Fichiers clés :**
*   `STM32_HTS221.h`: Fichier d'en-tête déclarant les fonctions publiques et les types de données.
*   `STM32_HTS221.c`: Fichier source contenant l'implémentation des fonctions de la bibliothèque.

## 3. Connexion Matérielle

Le capteur HTS221 communique via le bus I2C. Les connexions typiques sont :
*   SCL (Serial Clock)
*   SDA (Serial Data)
*   VDD (Alimentation)
*   GND (Masse)

L'adresse I2C par défaut du HTS221 est `0x5F` (adresse 7 bits), ce qui correspond à `0xBE` pour l'écriture et `0xBF` pour la lecture (adresses 8 bits, incluant le bit R/W). La bibliothèque utilise `(0x5F << 1)` pour l'adresse 8 bits.

## 4. Fonctions de la Bibliothèque

La bibliothèque expose les fonctions suivantes (définies dans `STM32_HTS221.h`) :

### `HTS221_Status HTS221_Init(I2C_HandleTypeDef *hi2c)`
*   **Description :** Initialise le capteur HTS221.
    *   Vérifie l'identifiant du capteur (registre `WHO_AM_I`).
    *   Active le capteur en mode de mesure continue (registre `CTRL_REG1`).
    *   Lit et stocke les données de calibration internes du capteur.
*   **Paramètres :**
    *   `hi2c`: Pointeur vers la structure de gestion du bus I2C (ex: `&hi2c1`).
*   **Valeur de retour :**
    *   `HTS221_OK`: Initialisation réussie.
    *   `HTS221_ERROR`: Erreur lors de l'initialisation (par exemple, capteur non détecté).

### `float HTS221_ReadTemperature(I2C_HandleTypeDef *hi2c)`
*   **Description :** Lit la température mesurée par le capteur.
    *   Lit les registres de sortie brute de température (`TEMP_OUT_L_REG`, `TEMP_OUT_H_REG`).
    *   Utilise les données de calibration (T0_degC, T1_degC, T0_OUT, T1_OUT) pour convertir la valeur brute en degrés Celsius par interpolation linéaire.
*   **Paramètres :**
    *   `hi2c`: Pointeur vers la structure de gestion du bus I2C.
*   **Valeur de retour :**
    *   La température en degrés Celsius (°C).
    *   Retourne `-273.15f` (zéro absolu approximatif) en cas d'erreur de calibration.

### `float HTS221_ReadHumidity(I2C_HandleTypeDef *hi2c)`
*   **Description :** Lit l'humidité relative mesurée par le capteur.
    *   Lit les registres de sortie brute d'humidité (`HUMIDITY_OUT_L_REG`, `HUMIDITY_OUT_H_REG`).
    *   Utilise les données de calibration (H0_rh, H1_rh, H0_T0_OUT, H1_T0_OUT) pour convertir la valeur brute en pourcentage d'humidité relative (%HR) par interpolation linéaire.
    *   Sature la valeur entre 0% et 100%.
*   **Paramètres :**
    *   `hi2c`: Pointeur vers la structure de gestion du bus I2C.
*   **Valeur de retour :**
    *   L'humidité relative en pourcentage (%).
    *   Retourne `-1.0f` en cas d'erreur de calibration.

## 5. Exemple d'Utilisation dans le main.c

1. les includes nécessaires

```c
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "STM32_HTS221.h"
/* USER CODE END Includes */
```

2. la fonction pour rediriger printf vers UART2

```c
/* USER CODE BEGIN 0 */
/**
 * @brief  Redirige la sortie de printf vers l'UART2
 * @param  ch: Caractère à envoyer
 * @retval Caractère envoyé
 */
int __io_putchar(int ch) {
	HAL_UART_Transmit(&huart2, (uint8_t*) &ch, 1, 0xFFFF); // Envoie le caractère via l'UART2
	return ch; // Retourne le caractère envoyé
}
/* USER CODE END 0 */
```

3. initialisation du capteur HTS221

```c
/* USER CODE BEGIN 2 */
// Affiche un message pour indiquer le début de l'initialisation du capteur
	printf("Initialisation du capteur d'humidité et de température HTS221...\r\n");

	// Initialise le capteur HTS221
	HTS221_Init(&hi2c1);

	// Pause pour garantir la stabilité après la lecture des données de calibration
	HAL_Delay(100);
/* USER CODE END 2 */   
```

4. Boucle principale

```c
/* USER CODE BEGIN WHILE */
	while (1) {
		// Affiche un en-tête pour les lectures des capteurs
		printf("\r\n=== LECTURES DES CAPTEURS ===\r\n");

		// Lire la température en °C depuis le capteur HTS221
		float temperature = HTS221_ReadTemperature(&hi2c1);

		// Lire l'humidité relative en % depuis le capteur HTS221
		float humidity = HTS221_ReadHumidity(&hi2c1);

		// Affiche les lectures de température et d'humidité
		printf("HTS221 | Température : %.2f °C | Humidité : %.2f %%\r\n", temperature, humidity);

		// Attendre 1 seconde avant la prochaine lecture
		HAL_Delay(1000);
		/* USER CODE END WHILE */
```

