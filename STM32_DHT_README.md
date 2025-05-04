# Bibliothèque STM32 HAL pour Capteur de Température et d'Humidité DHT (DHT11, DHT22, DHT21)

Cette bibliothèque permet de lire les données de température et d'humidité des capteurs DHT11, DHT22 et DHT21 en utilisant un microcontrôleur STM32 et la bibliothèque HAL.

## Fonctionnalités

* Initialisation des capteurs DHT (DHT11, DHT22, DHT21).
* Lecture de la température en degrés Celsius (°C).
* Lecture de l'humidité relative en pourcentage (%).
* Vérification de la somme de contrôle pour garantir l'intégrité des données.
* Gestion des délais précis en microsecondes via un timer STM32.

## Connexions Matérielles

Connectez le capteur DHT à votre microcontrôleur STM32 comme suit :

* **VCC** -> 3.3V ou 5V (selon votre capteur).
* **GND** -> GND.
* **DATA** -> Broche GPIO configurée en entrée/sortie (ex: GPIO_PIN_0, GPIO_PIN_1, ...).

Assurez-vous d'activer le timer approprié dans votre configuration STM32CubeMX ou manuellement. Le timer doit être configuré pour compter en microsecondes.

## Utilisation de base

Voici un exemple simple d'utilisation dans votre fichier `main.c` :

### 1. Inclure l'en-tête

```c
/* USER CODE BEGIN Includes */
#include <stdio.h>           // Pour printf
#include "STM32_DHT.h"       // Inclure la bibliothèque DHT
/* USER CODE END Includes */
```

### 2. Déclarer les constantes préprocesseur

```c
/* USER CODE BEGIN PD */
#define DHT_TYPE DHT22       // Type de capteur (DHT11, DHT22, ou DHT21)
/* USER CODE END PD */
```

### 3. Initialiser le timer et le capteur DHT 

```c
/* USER CODE BEGIN 2 */
printf("test STM32_DHT\r\n");  // Affiche un message de test dans la console
// Démarrer le timer utilisé pour les délais en microsecondes
HAL_TIM_Base_Start(&htim1); // démarre le timer
// Déclaration des structures pour les capteurs DHT
DHT_Sensor DHT22sensor;
// Initialisation du capteur DHT22
DHT_Init(&DHT22sensor, &htim1, GPIOA, GPIO_PIN_10, DHT22);  // Initialise le capteur DHT22 sur GPIOA, PIN 10 avec TIM1
/* USER CODE END 2 */
```

### 4. Lire les données dans la boucle principale

```c
/* USER CODE BEGIN WHILE */
  	while (1) {
  	    // Tableaux pour stocker les données de température et d'humidité pour chaque capteur
  	    float dataDHT22[2];
  	    HAL_StatusTypeDef result;  // Variable pour stocker le résultat de la lecture des données

  	    // Lit les données du capteur DHT22
  	    result = DHT_GetData(&DHT22sensor, dataDHT22);
  	    if (result == HAL_OK) {
  	        // Affiche les données de température et d'humidité du capteur DHT22
  	        printf("Sensor DHT22 - Temperature: %.0f C, Humidity: %.0f %%\r\n", dataDHT22[0], dataDHT22[1]);
  	    } else {
  	        // Affiche un message d'erreur si la lecture a échoué
  	        DisplayError("DHT22", result);
  	    }

  	    // Attend 2000 ms (2 secondes) avant la prochaine lecture
  	    HAL_Delay(2000);
      /* USER CODE END WHILE */
```

## Référence API

### Structures Principales

* `DHT_Sensor` : Structure principale contenant les informations de configuration du capteur (port GPIO, broche, type) et les données brutes lues.

### Fonctions Principales

* `HAL_StatusTypeDef DHT_Init(DHT_Sensor* sensor, TIM_HandleTypeDef* htim, GPIO_TypeDef* port, uint16_t pin, uint8_t type)`
    * Initialise le capteur DHT avec les paramètres spécifiés.
    * Retourne `HAL_OK` en cas de succès, ou un code d'erreur `HAL_ERROR`.

* `HAL_StatusTypeDef DHT_GetData(DHT_Sensor* sensor, float* data)`
    * Lit les données de température et d'humidité depuis le capteur.
    * Stocke la température en `data[0]` (°C) et l'humidité en `data[1]` (%).
    * Retourne HAL_OK en cas de succès, ou un code d'erreur (`HAL_ERROR, HAL_TIMEOUT`).

## Codes d'Erreur (HAL_StatusTypeDef)

* `HAL_OK`: Opération réussie.
* `HAL_ERROR`: Erreur générale (ex: somme de contrôle incorrecte).
* `HAL_TIMEOUT`: Timeout lors de la communication avec le capteur.