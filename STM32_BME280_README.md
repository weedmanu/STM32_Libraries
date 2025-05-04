# Bibliothèque STM32 pour Capteur BME280 (I2C)

Cette bibliothèque fournit une interface simple pour interagir avec le capteur environnemental BME280 de Bosch Sensortec en utilisant le bus I2C sur les microcontrôleurs STM32 via la couche d'abstraction matérielle (HAL) STM32. Elle permet de lire la température, la pression atmosphérique et l'humidité relative.

## Fonctionnalités

*   Initialisation et configuration du capteur BME280.
*   Lecture des données de température (°C).
*   Lecture des données de pression (Pa).
*   Lecture des données d'humidité relative (%RH).
*   Lecture groupée de toutes les données (Température, Pression, Humidité).
*   Configuration du mode de fonctionnement (Sleep, Forced, Normal).
*   Configuration du suréchantillonnage pour chaque mesure.
*   Configuration du filtre IIR.
*   Configuration du temps d'attente en mode Normal.
*   Réinitialisation logicielle du capteur.
*   Lecture de l'identifiant (Chip ID) du capteur.
*   Gestion des erreurs de communication et de configuration.
*   Option de débogage via `printf`.

## Prérequis

*   Un microcontrôleur STM32.
*   STM32CubeIDE ou un environnement de développement compatible avec STM32 HAL.
*   Un périphérique I2C configuré via STM32CubeMX (ou manuellement).
*   Un capteur BME280 connecté au bus I2C du STM32.

## Intégration

1.  **Copiez les fichiers** : Copiez les fichiers `STM32_BME280.h` et `STM32_BME280.c` dans votre projet STM32 (par exemple, dans les dossiers `Core/Inc` et `Core/Src` ou dans des dossiers dédiés `Drivers/BME280`).
2.  **Incluez l'en-tête** : Ajoutez `#include "STM32_BME280.h"` dans les fichiers où vous souhaitez utiliser la bibliothèque (typiquement `main.c`).

## Utilisation de base

Voici un exemple simple d'utilisation dans votre fichier `main.c` :

1.  **Inclure l'en-tête :**

```c
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "STM32_BME280.h" // Include the new library header
/* USER CODE END Includes */
```

2.  **Déclarer les constantes préprocesseur :**

```c
/* USER CODE BEGIN PD */
#define SENSOR_READ_DELAY_MS 1000 // Délai entre les lectures du capteur
/* USER CODE END PD */
```

3.  **Déclarer des variables globales :**

```c
/* USER CODE BEGIN PV */
BME280_Handle_t bme280_dev; // Structure pour le capteur BME280
/* USER CODE END PV */
```

4.  **Redirection `printf` pour l'affichage :**    

```c
/* USER CODE BEGIN 0 */
// Fonction qui transmet un caractère via UART et le renvoie. Utilisé pour la sortie standard (printf).
int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart2, (uint8_t*) &ch, 1, 0xFFFF); // Pour Envoyer le caractère via UART
    return ch;  // Pour renvoyer le caractère pour la sortie standard
}
/* USER CODE END 0 */
```

5.  **Initialiser le capteur :**

### 5.1 Initialisation avec les paramètres par défaut

L'initialisation avec les paramètres par défaut configure le capteur en mode normal avec les réglages suivants :
- Filtre : désactivé (`filter = BME280_FILTER_OFF`)
- Suréchantillonnage de la pression : x4 (`oversampling_p = BME280_OVERSAMPLING_X4`)
- Suréchantillonnage de la température : x2 (`oversampling_t = BME280_OVERSAMPLING_X2`)
- Suréchantillonnage de l'humidité : x2 (`oversampling_h = BME280_OVERSAMPLING_X2`)
- Temps d'attente : 1000 ms (`standby_time = BME280_STANDBY_1000_MS`)

Voici un exemple d'initialisation avec ces paramètres par défaut :

```c
/* USER CODE BEGIN 2 */
HAL_Delay(250);
// Initialisation du BME280 via la librairie
printf("Initialisation BME280 avec les paramètres par défaut...\r\n");

int8_t init_status = BME280_Init(&bme280_dev, &hi2c1, BME280_ADDRESS_DEFAULT, NULL); // NULL pour utiliser les paramètres par défaut

if (init_status != BME280_OK) {     // Vérification de l'initialisation
    switch (init_status) {            // Gestion des erreurs
            case BME280_ERROR_COMM:
                    printf("Erreur de communication avec le capteur BME280.\r\n");
                    break;
            case BME280_ERROR_CHIP_ID:
                    printf("ID de puce incorrect pour le capteur BME280.\r\n");
                    break;
            case BME280_ERROR_CONFIG:
                    printf("Erreur lors de la configuration du capteur BME280.\r\n");
                    break;
            default:
                    printf("Erreur inconnue lors de l'initialisation du BME280.\r\n");
                    break;
    }
    Error_Handler();                   
}
printf("BME280 initialisé avec succès avec les paramètres par défaut.\r\n");
printf("-----------------------------------------------------\r\n\n");
HAL_Delay(100); // Petit délai pour laisser le capteur se stabiliser
/* USER CODE END 2 */
```

```c
/* USER CODE BEGIN 2 */
HAL_Delay(250);
// Initialisation du BME280 via la librairie
printf("Initialisation BME280...\r\n");

int8_t init_status = BME280_Init(&bme280_dev, &hi2c1, BME280_ADDRESS_DEFAULT, NULL); // Pas de config 

if (init_status != BME280_OK) {     // Vérification de l'initialisation
  switch (init_status) {            // Gestion des erreurs
      case BME280_ERROR_COMM:
          printf("Erreur de communication avec le capteur BME280.\r\n");
          break;
      case BME280_ERROR_CHIP_ID:
          printf("ID de puce incorrect pour le capteur BME280.\r\n");
          break;
      case BME280_ERROR_CONFIG:
          printf("Erreur lors de la configuration du capteur BME280.\r\n");
          break;
      default:
          printf("Erreur inconnue lors de l'initialisation du BME280.\r\n");
          break;
  }
  Error_Handler();                   
}
printf("BME280 initialisé avec succès.\r\n");
printf("-----------------------------------------------------\r\n\n");
HAL_Delay(100); // Petit délai pour laisser le capteur se stabiliser
/* USER CODE END 2 */
```

5.2 **Initialisation avec une configuration personnalisée**

Vous pouvez également initialiser le capteur avec une configuration personnalisée

```c
/* USER CODE BEGIN 2 */
HAL_Delay(250);
// Initialisation du BME280 via la librairie
printf("Initialisation BME280...\r\n");

// Définir la configuration souhaitée pour le BME280
BME280_Config_t bme_config;
bme_config.mode = BME280_MODE_NORMAL;          // Mode Normal
bme_config.filter = BME280_FILTER_OFF;         // Pas de filtre IIR
bme_config.oversampling_p = BME280_OVERSAMPLING_X4; // Suréchantillonnage Pression x4
bme_config.oversampling_t = BME280_OVERSAMPLING_X2; // Suréchantillonnage Température x2
bme_config.oversampling_h = BME280_OVERSAMPLING_X2; // Suréchantillonnage Humidité x2
bme_config.standby_time = BME280_STANDBY_1000_MS; // Temps d'attente 1000ms


int8_t init_status = BME280_Init(&bme280_dev, &hi2c1, BME280_ADDRESS_DEFAULT, &bme_config); // Passer la config
if (init_status != BME280_OK) {
  switch (init_status) {
      case BME280_ERROR_COMM:
          printf("Erreur de communication avec le capteur BME280.\r\n");
          break;
      case BME280_ERROR_CHIP_ID:
          printf("ID de puce incorrect pour le capteur BME280.\r\n");
          break;
      case BME280_ERROR_CONFIG:
          printf("Erreur lors de la configuration du capteur BME280.\r\n");
          break;
      default:
          printf("Erreur inconnue lors de l'initialisation du BME280.\r\n");
          break;
  }
  Error_Handler();
}
printf("BME280 initialisé avec succès.\r\n");
printf("-----------------------------------------------------\r\n\n");
HAL_Delay(100); // Petit délai pour laisser le capteur se stabiliser
/* USER CODE END 2 */
```

6.  **Lire les données du capteur :**

```c
/* USER CODE BEGIN WHILE */
  while (1)
  {
      float temperature = 0.0f, pressure = 0.0f, humidity = 0.0f;
      int8_t read_status = BME280_ReadAll(&bme280_dev, &temperature, &pressure, &humidity);

      if (read_status == BME280_OK) {
          printf("Température : %.2f °C\r\n", temperature);
          printf("Pression : %.2f hPa\r\n", pressure / 100.0);
          printf("Humidité : %.2f %%\r\n", humidity);
      } else {
          printf("Erreur lors de la lecture des données du BME280 (code : %d).\r\n", read_status);
      }
      printf("-----------------------------------------------------\r\n\n");
      HAL_Delay(SENSOR_READ_DELAY_MS);
    /* USER CODE END WHILE */
```

## Référence API

### Structures Principales

*   `BME280_Handle_t`: Structure principale contenant le handle I2C, l'adresse du capteur, la configuration et les données de calibration.
*   `BME280_Config_t`: Structure pour définir la configuration du capteur (mode, suréchantillonnage, filtre, temps d'attente).

### Fonctions Principales

*   `int8_t BME280_Init(BME280_Handle_t *dev, I2C_HandleTypeDef *hi2c, uint8_t dev_addr, BME280_Config_t *config)`
    *   Initialise le capteur BME280. Lit les coefficients de calibration, configure le capteur selon la structure `config` fournie (ou utilise une configuration par défaut si `config` est `NULL`).
    *   Retourne `BME280_OK` en cas de succès, ou un code d'erreur `BME280_ERROR_xxx`.
*   `int8_t BME280_ReadAll(BME280_Handle_t *dev, float *temperature, float *pressure, float *humidity)`
    *   Lit les données compensées de température, pression et humidité. Si le capteur est en mode `FORCED`, cette fonction déclenche d'abord une mesure.
    *   Retourne `BME280_OK` en cas de succès, ou un code d'erreur `BME280_ERROR_xxx`.
*   `HAL_StatusTypeDef BME280_ReadTemperature(BME280_Handle_t *dev, float *temperature)`
    *   Lit uniquement la température compensée. Calcule `t_fine` nécessaire pour les autres compensations.
*   `HAL_StatusTypeDef BME280_ReadPressure(BME280_Handle_t *dev, float *pressure)`
    *   Lit uniquement la pression compensée. Nécessite que `t_fine` ait été calculé (généralement en lisant d'abord la température).
*   `HAL_StatusTypeDef BME280_ReadHumidity(BME280_Handle_t *dev, float *humidity)`
    *   Lit uniquement l'humidité compensée. Nécessite que `t_fine` ait été calculé.
*   `HAL_StatusTypeDef BME280_SetMode(BME280_Handle_t *dev, BME280_Mode_t mode)`
    *   Change le mode de fonctionnement du capteur (`SLEEP`, `FORCED`, `NORMAL`).
*   `int8_t BME280_TriggerForcedMeasurement(BME280_Handle_t *dev, uint32_t timeout)`
    *   Met le capteur en mode `FORCED`, attend la fin de la mesure (ou le timeout). Utile avant d'appeler `BME280_ReadAll` si vous gérez le mode `FORCED` manuellement.
    *   Retourne `BME280_OK`, `BME280_ERROR_COMM`, ou `HAL_TIMEOUT`.
*   `HAL_StatusTypeDef BME280_Reset(BME280_Handle_t *dev)`
    *   Effectue une réinitialisation logicielle du capteur.
*   `HAL_StatusTypeDef BME280_ReadChipID(BME280_Handle_t *dev, uint8_t *chip_id)`
    *   Lit l'identifiant matériel du capteur (devrait être `0x60` pour BME280).

### Codes d'Erreur (`int8_t`)

*   `BME280_OK` (0): Opération réussie.
*   `BME280_ERROR_COMM` (-1): Erreur de communication I2C (vérifiez connexions, adresse, configuration I2C).
*   `BME280_ERROR_CHIP_ID` (-2): L'ID lu ne correspond pas à celui attendu (0x60).
*   `BME280_ERROR_CONFIG` (-3): Erreur lors de l'écriture de la configuration dans les registres.
*   `BME280_ERROR_PARAM` (-4): Paramètre invalide passé à une fonction (ex: pointeur NULL).
*   `BME280_ERROR_MEASURING` (-5): (Non utilisé actuellement, mais réservé).

## Options de Configuration

La structure `BME280_Config_t` permet de configurer les paramètres suivants :

### 1. **Mode de fonctionnement (`mode`)**
Détermine comment le capteur effectue les mesures :
- `BME280_MODE_SLEEP` : Mode veille, aucune mesure.
- `BME280_MODE_FORCED` : Une seule mesure, puis retour en veille.
- `BME280_MODE_NORMAL` : Mesures périodiques en continu.

### 2. **Suréchantillonnage**
Améliore la précision des mesures en effectuant plusieurs lectures :
- `oversampling_t` : Suréchantillonnage pour la température.
- `oversampling_p` : Suréchantillonnage pour la pression.
- `oversampling_h` : Suréchantillonnage pour l'humidité.

Options disponibles :
- `BME280_OVERSAMPLING_SKIPPED` : Désactivé.
- `BME280_OVERSAMPLING_X1` : Une seule mesure.
- `BME280_OVERSAMPLING_X2` : Suréchantillonnage x2.
- `BME280_OVERSAMPLING_X4` : Suréchantillonnage x4.
- `BME280_OVERSAMPLING_X8` : Suréchantillonnage x8.
- `BME280_OVERSAMPLING_X16` : Suréchantillonnage x16.

### 3. **Filtre IIR (`filter`)**
Réduit les fluctuations rapides dans les mesures :
- `BME280_FILTER_OFF` : Pas de filtrage.
- `BME280_FILTER_X2` : Filtrage léger.
- `BME280_FILTER_X4` : Filtrage modéré.
- `BME280_FILTER_X8` : Filtrage élevé.
- `BME280_FILTER_X16` : Filtrage très élevé.

### 4. **Temps d'attente (`standby_time`)**
Définit l'intervalle entre deux cycles de mesure en mode `NORMAL` :
- `BME280_STANDBY_0_5_MS` : 0,5 ms.
- `BME280_STANDBY_62_5_MS` : 62,5 ms.
- `BME280_STANDBY_125_MS` : 125 ms.
- `BME280_STANDBY_250_MS` : 250 ms.
- `BME280_STANDBY_500_MS` : 500 ms.
- `BME280_STANDBY_1000_MS` : 1000 ms.
- `BME280_STANDBY_10_MS` : 10 ms.
- `BME280_STANDBY_20_MS` : 20 ms.

---

Ces options permettent de personnaliser le comportement du capteur en fonction des besoins spécifiques de votre application.

## Débogage

Vous pouvez activer des messages de débogage via `printf` en décommentant la ligne `#define DEBUG_ON` au début du fichier `STM32_BME280.c`. Assurez-vous que `printf` est correctement redirigé (par exemple, vers une liaison série UART, comme montré dans l'exemple `__io_putchar`).
