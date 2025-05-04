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

```c
#include "main.h"
#include "STM32_BME280.h"
#include <stdio.h> // Pour printf

// Déclarer le handle I2C (généralement défini par CubeMX)
extern I2C_HandleTypeDef hi2c1; // Assurez-vous que c'est le bon handle I2C

// Déclarer le handle pour le BME280
BME280_Handle_t bme280_dev;

// Fonction pour rediriger printf vers UART (si nécessaire pour le débogage)
int __io_putchar(int ch) {
    // Remplacez huart2 par votre handle UART si vous en utilisez un autre
    extern UART_HandleTypeDef huart2;
    HAL_UART_Transmit(&huart2, (uint8_t*)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

int main(void) {
    // Initialisation HAL, SystemClock_Config, MX_GPIO_Init, MX_I2C1_Init, MX_USART2_UART_Init...
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init(); // Initialisation de l'UART pour printf
    MX_I2C1_Init();      // Initialisation de l'I2C

    HAL_Delay(100); // Petit délai

    printf("Initialisation BME280...\r\n");

    // Optionnel : Définir une configuration personnalisée
    BME280_Config_t bme_config;
    bme_config.mode = BME280_MODE_NORMAL;
    bme_config.filter = BME280_FILTER_OFF;
    bme_config.oversampling_p = BME280_OVERSAMPLING_X4;
    bme_config.oversampling_t = BME280_OVERSAMPLING_X2;
    bme_config.oversampling_h = BME280_OVERSAMPLING_X2;
    bme_config.standby_time = BME280_STANDBY_1000_MS;

    // Initialiser le capteur BME280 avec la configuration personnalisée
    // Si vous passez NULL pour le dernier argument, la configuration par défaut sera utilisée.
    int8_t init_status = BME280_Init(&bme280_dev, &hi2c1, BME280_ADDRESS_DEFAULT, &bme_config);

    // Vérifier le statut de l'initialisation
    if (init_status != BME280_OK) {
        printf("Erreur initialisation BME280: %d\r\n", init_status);
        // Gérer l'erreur (boucle infinie, redémarrage...)
        Error_Handler();
    } else {
        printf("BME280 initialisé avec succès.\r\n");
    }

    while (1) {
        float temperature, pressure, humidity;

        // Lire toutes les données du capteur
        int8_t read_status = BME280_ReadAll(&bme280_dev, &temperature, &pressure, &humidity);

        if (read_status == BME280_OK) {
            printf("Temperature: %.2f C\r\n", temperature);
            printf("Pressure: %.2f hPa\r\n", pressure / 100.0f); // Conversion Pa -> hPa
            printf("Humidity: %.2f %%\r\n", humidity);
        } else {
            printf("Erreur lecture BME280: %d\r\n", read_status);
        }

        printf("--------------------\r\n");
        HAL_Delay(2000); // Attendre 2 secondes
    }
}
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

La structure `BME280_Config_t` permet de régler :

*   `mode`: `BME280_MODE_SLEEP`, `BME280_MODE_FORCED`, `BME280_MODE_NORMAL`.
*   `oversampling_p`, `oversampling_t`, `oversampling_h`: `BME280_OVERSAMPLING_SKIPPED`, `_X1`, `_X2`, `_X4`, `_X8`, `_X16`.
*   `filter`: `BME280_FILTER_OFF`, `_X2`, `_X4`, `_X8`, `_X16`. Réduit les fluctuations rapides.
*   `standby_time`: `BME280_STANDBY_0_5_MS` à `BME280_STANDBY_1000_MS`, `_10_MS`, `_20_MS`. Temps entre les mesures en mode `NORMAL`.

## Débogage

Vous pouvez activer des messages de débogage via `printf` en décommentant la ligne `#define DEBUG_ON` au début du fichier `STM32_BME280.c`. Assurez-vous que `printf` est correctement redirigé (par exemple, vers une liaison série UART, comme montré dans l'exemple `__io_putchar`).
