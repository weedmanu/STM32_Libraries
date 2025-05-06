# Librairie STM32_AHT20 pour STM32 HAL

Cette librairie fournit une interface simple pour communiquer avec le capteur de température et d'humidité AHT20 en utilisant les pilotes STM32 HAL.

## Table des matières

1.  [Introduction](#introduction)
2.  [Prérequis](#prérequis)
3.  [Fichiers de la librairie](#fichiers-de-la-librairie)
4.  [Installation et Intégration](#installation-et-intégration)
5.  [Utilisation](#utilisation)
    1.  [Inclusion](#inclusion)
    2.  [Initialisation](#initialisation)
    3.  [Lecture des mesures](#lecture-des-mesures)
    4.  [Gestion des erreurs](#gestion-des-erreurs)
6.  [Fonctions détaillées](#fonctions-détaillées)
    1.  [`AHT20_Init`](#aht20_init)
    2.  [`AHT20_SoftReset`](#aht20_softreset)
    3.  [`AHT20_GetStatus`](#aht20_getstatus)
    4.  [`AHT20_ReadMeasurements`](#aht20_readmeasurements)
7.  [Calibration](#calibration)
8.  [Constantes importantes](#constantes-importantes)
9.  [Dépannage](#dépannage)

## Introduction

Le capteur AHT20 est un capteur numérique de température et d'humidité relative. Cette librairie facilite son utilisation avec les microcontrôleurs STM32 en s'appuyant sur la couche d'abstraction matérielle (HAL) fournie par STMicroelectronics.

## Prérequis

*   Un microcontrôleur STM32.
*   STM32CubeIDE ou un environnement de développement compatible.
*   Le bus I2C doit être configuré et initialisé dans votre projet (généralement via STM32CubeMX).
*   La librairie HAL STM32.

## Fichiers de la librairie

La librairie est composée de deux fichiers principaux :

*   `STM32_AHT20/STM32_AHT20.h` : Fichier d'en-tête contenant les déclarations des fonctions, les définitions des structures de données (`AHT20_Data`), les énumérations de statut (`AHT20_Status`) et les constantes nécessaires.
*   `STM32_AHT20/STM32_AHT20.c` : Fichier source contenant l'implémentation des fonctions pour interagir avec le capteur AHT20.

## Installation et Intégration

1.  **Copiez les fichiers** :
    *   Copiez le dossier `STM32_AHT20` (contenant `STM32_AHT20.h` et `STM32_AHT20.c`) dans le répertoire de votre projet STM32, par exemple dans un dossier `Drivers` ou `Libs`.
2.  **Ajoutez les chemins d'inclusion** :
    *   Dans les propriétés de votre projet STM32CubeIDE (Clic droit sur le projet -> Properties) :
        *   Allez dans `C/C++ General -> Paths and Symbols`.
        *   Sous l'onglet `Includes`, pour `GNU C`, cliquez sur `Add...` et ajoutez le chemin vers le dossier où vous avez placé `STM32_AHT20.h` (par exemple, `../STM32_AHT20` si vous l'avez mis dans un dossier `STM32_AHT20` au même niveau que `Core`).
3.  **Ajoutez les fichiers sources au build** :
    *   Assurez-vous que `STM32_AHT20.c` est inclus dans le processus de compilation. STM32CubeIDE devrait le détecter automatiquement s'il est placé dans un dossier source connu (comme `Src` ou un sous-dossier). Sinon, ajoutez-le manuellement aux sources à compiler.

## Utilisation

### 1. Inclusion

Incluez le fichier d'en-tête dans votre fichier `main.c` :

```c
/* USER CODE BEGIN Includes */
#include <stdio.h>              // Pour printf
#include "STM32_AHT20.h"        // Pour l'utilisation de la librairie STM32_AHT20
/* USER CODE END Includes */
```

### Initialisation

Avant toute opération, le capteur doit être initialisé. Cela se fait généralement dans la section `USER CODE BEGIN 2` de votre `main.c`.

```c
  /* USER CODE BEGIN 2 */
  HAL_Delay(250);
  printf("Test du capteur AHT20\r\n");
  /* Initialisation du capteur AHT20 */
  AHT20_Status status = AHT20_Init(&hi2c1);                                 // 
  if (status != AHT20_OK) {
      printf("Erreur d'initialisation du capteur AHT20: %d\r\n", status);
      Error_Handler();
  }
  printf("Initialisation AHT20 réussie\r\n");
  HAL_Delay(1000);
  /* USER CODE END 2 */
```

### Lecture des mesures

Pour lire la température et l'humidité, utilisez la fonction `AHT20_ReadMeasurements`.

```c
AHT20_Data sensor_data;

// Dans votre boucle principale (while(1))
status = AHT20_ReadMeasurements(&hi2c1, &sensor_data);

if (status == AHT20_OK) {
    printf("Température: %.1f °C, Humidité: %.1f %%\r\n", sensor_data.temperature, sensor_data.humidity);
} else if (status == AHT20_ERROR_BUSY) {
    printf("Erreur de lecture: Capteur AHT20 occupé\r\n");
} else {
    printf("Erreur de lecture des mesures: %d\r\n", status);
    // Gérer l'erreur (par exemple, tenter une réinitialisation ou ignorer la lecture)
}
HAL_Delay(2000); // Attendre avant la prochaine lecture
```

### Gestion des erreurs

Toutes les fonctions de la librairie retournent un statut de type `AHT20_Status`. Il est crucial de vérifier cette valeur pour s'assurer que l'opération s'est déroulée correctement. Les statuts possibles sont définis dans `STM32_AHT20.h` (par exemple `AHT20_OK`, `AHT20_ERROR_I2C`, `AHT20_ERROR_CALIBRATION`, `AHT20_ERROR_BUSY`, `AHT20_ERROR_CHECKSUM`).


## Fonctions détaillées

### `AHT20_Init`

```c
AHT20_Status AHT20_Init(I2C_HandleTypeDef *hi2c);
```
Initialise le capteur AHT20. Cette fonction effectue un reset logiciel, envoie la commande d'initialisation et vérifie le bit de calibration du capteur.
*   `hi2c`: Pointeur vers la structure de handle I2C.
*   Retourne `AHT20_OK` en cas de succès, ou un code d'erreur sinon.

### `AHT20_SoftReset`

```c
AHT20_Status AHT20_SoftReset(I2C_HandleTypeDef *hi2c);
```
Effectue un reset logiciel du capteur.
*   `hi2c`: Pointeur vers la structure de handle I2C.
*   Retourne `AHT20_OK` en cas de succès, ou `AHT20_ERROR_I2C` en cas d'échec de communication.

### `AHT20_GetStatus`

```c
AHT20_Status AHT20_GetStatus(I2C_HandleTypeDef *hi2c, uint8_t *status_byte);
```
Récupère l'octet de statut brut du capteur.
*   `hi2c`: Pointeur vers la structure de handle I2C.
*   `status_byte`: Pointeur vers un `uint8_t` où l'octet de statut sera stocké.
*   Retourne `AHT20_OK` en cas de succès, ou `AHT20_ERROR_I2C`.

### `AHT20_ReadMeasurements`

```c
AHT20_Status AHT20_ReadMeasurements(I2C_HandleTypeDef *hi2c, AHT20_Data *data);
```
Déclenche une mesure et lit les données de température et d'humidité.
*   `hi2c`: Pointeur vers la structure de handle I2C.
*   `data`: Pointeur vers une structure `AHT20_Data` où les mesures converties (température en °C, humidité en %) seront stockées.
*   Retourne `AHT20_OK` en cas de succès, `AHT20_ERROR_BUSY` si le capteur est occupé, `AHT20_ERROR_CHECKSUM` si le CRC est incorrect, ou `AHT20_ERROR_I2C`.

## Calibration

Le capteur AHT20 effectue une **calibration interne** lors de son initialisation. La fonction `AHT20_Init` vérifie le bit de statut "CAL Enable" (bit 3 de l'octet de statut) pour s'assurer que cette calibration interne s'est bien déroulée. Si ce bit n'est pas positionné correctement après la séquence d'initialisation, `AHT20_Init` retourne `AHT20_ERROR_CALIBRATION`.
Il ne s'agit pas d'une procédure de calibration où l'utilisateur fournit des points de référence, mais d'une vérification de l'auto-calibration du capteur.

## Constantes importantes

Définies dans `STM32_AHT20.h` (non fourni explicitement, mais déductible du `.c`):

*   `AHT20_I2C_ADDR`: Adresse I2C du capteur (généralement `0x38 << 1`).
*   `AHT20_CMD_INIT`, `AHT20_CMD_SOFT_RESET`, `AHT20_CMD_TRIGGER`: Commandes I2C.
*   `AHT20_DELAY_INIT_CMD_WAIT_MS`, `AHT20_DELAY_SOFT_RESET_MS`, `AHT20_DELAY_MEASUREMENT_WAIT_MS`: Délais utilisés par la librairie.
*   `AHT20_I2C_TIMEOUT_MS`: Timeout pour les opérations I2C.

## Dépannage

*   **Erreur d'initialisation (`AHT20_ERROR_I2C`, `AHT20_ERROR_CALIBRATION`)**:
    *   Vérifiez le câblage du capteur AHT20 (SDA, SCL, VCC, GND).
    *   Assurez-vous que les résistances de pull-up sont présentes sur les lignes SDA et SCL si nécessaire.
    *   Vérifiez que l'adresse I2C (`AHT20_I2C_ADDR`) est correcte. L'AHT20 a une adresse fixe de `0x38`.
    *   Utilisez la fonction `I2C_Scan` (présente dans l'exemple `main.c`) pour vérifier si le capteur est détecté sur le bus I2C.
    *   Assurez-vous que le bus I2C est correctement initialisé avant d'appeler `AHT20_Init`.
*   **Erreur de lecture (`AHT20_ERROR_BUSY`)**:
    *   Le capteur peut être encore en train d'effectuer une mesure. La librairie attend `AHT20_DELAY_MEASUREMENT_WAIT_MS` (typiquement 80ms) après avoir déclenché une mesure. Si cette erreur persiste, il peut y avoir un problème avec le capteur ou la communication.
*   **Erreur de lecture (`AHT20_ERROR_CHECKSUM`)**:
    *   Indique que les données reçues du capteur sont corrompues. Cela peut être dû à du bruit sur le bus I2C, à des problèmes de timing ou à un capteur défectueux. Vérifiez la qualité des connexions.
*   **Pas de sortie `printf`**:
    *   Assurez-vous que l'UART est correctement configuré et que la fonction `__io_putchar` (ou équivalent) est implémentée pour rediriger `printf` vers l'UART (comme montré dans l'exemple `main.c`).
