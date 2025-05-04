# Bibliothèque STM32 HAL pour Capteur de Pression Barométrique BMP085 / BMP180 (I2C)

Cette bibliothèque fournit une interface simple pour interagir avec les capteurs de pression barométrique et de température BMP085 et BMP180 de Bosch Sensortec en utilisant le bus I2C sur les microcontrôleurs STM32 via la couche d'abstraction matérielle (HAL) STM32.

## Fonctionnalités

*   Initialisation du capteur via I2C et lecture des coefficients de calibration.
*   Lecture de la température en degrés Celsius (°C).
*   Lecture de la pression atmosphérique en Pascals (Pa).
*   Calcul de l'altitude approximative en mètres (m) basée sur la pression.

## Connexions Matérielles

Connectez le module BMP085/180 à votre microcontrôleur STM32 en utilisant le bus I2C :

*   **VCC** -> 3.3V (ou 5V si le module a un régulateur et un level shifter)
*   **GND** -> GND
*   **SCL** -> Broche SCL I2C du STM32 (ex: PB6, PB8, PB10 selon le MCU et la configuration I2C)
*   **SDA** -> Broche SDA I2C du STM32 (ex: PB7, PB9, PB11 selon le MCU et la configuration I2C)

Assurez-vous d'activer le périphérique I2C approprié dans votre configuration STM32CubeMX ou manuellement. Des résistances de pull-up (généralement 4.7kΩ ou 2.2kΩ) sont nécessaires sur les lignes SCL et SDA si elles ne sont pas déjà présentes sur le module ou la carte STM32. L'adresse I2C par défaut du BMP085/180 est `0x77` (adresse 7 bits, soit `0xEE` en écriture et `0xEF` en lecture).

## Prérequis

*   Un microcontrôleur STM32.
*   Bibliothèques STM32 HAL (en particulier `stm32xxxx_hal_i2c.h`)
*   Une configuration I2C fonctionnelle.

## Installation

1.  Copiez les fichiers `BMP085_180.c` et `BMP085_180.h` dans votre projet STM32 (par exemple, dans les dossiers `Src` et `Inc` respectivement).
2.  Ajoutez le chemin vers le fichier d'en-tête (`Inc`) aux chemins d'inclusion de votre projet si nécessaire.
3.  Incluez l'en-tête : Ajoutez `#include "BMP085_180.h"` dans les fichiers où vous souhaitez utiliser la bibliothèque (typiquement `main.c`).

## Utilisation de base

Voici un exemple simple d'utilisation dans votre fichier `main.c`, basé sur le code fourni :

1.  **Inclure l'en-tête :**
    ```c
    /* USER CODE BEGIN Includes */
    #include <stdio.h>      // Pour printf
    #include "BMP085_180.h" // Inclure la bibliothèque du capteur
    /* USER CODE END Includes */
    ```

2.  **Déclarer des variables globales (si nécessaire) :**
    *(Note: L'exemple fourni utilise des variables locales dans la boucle `while`, ce qui est aussi une bonne pratique)*
    ```c
    /* USER CODE BEGIN PV */
    // Aucune variable globale spécifique au capteur nécessaire dans cet exemple
    /* USER CODE END PV */
    ```

3.  **(Optionnel) Redirection `printf` pour l'affichage (si vous utilisez `printf`) :**
    ```c
    /* USER CODE BEGIN 0 */
    // Fonction pour rediriger printf vers UART (ex: UART2)
    int __io_putchar(int ch) { HAL_UART_Transmit(&huart2, (uint8_t*)&ch, 1, HAL_MAX_DELAY); return ch; }
    /* USER CODE END 0 */
    ```

4.  **Initialiser le capteur** (après l'initialisation de l'I2C par HAL, par exemple dans `main()` après `MX_I2C1_Init()`):
    ```c
    /* USER CODE BEGIN 2 */
    // Assurez-vous que votre handle I2C (ex: hi2c1) est correctement initialisé
    BMP_Init(&hi2c1);
    printf("BMP085/180 Initialisé.\r\n");
    HAL_Delay(100); // Petit délai
    /* USER CODE END 2 */
    ```
    *(Note: La fonction `BMP_Init` dans certaines implémentations peut retourner un statut. Vérifiez l'en-tête `.h` pour voir si une gestion d'erreur est possible à l'initialisation).*

5.  **Lire les données dans la boucle principale :**
    ```c
    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        // Déclaration des variables locales pour stocker les lectures
        float temperature, pressure, altitude;

        temperature = BMP_GetTemperature(); // Lire la température en °C
        pressure = BMP_GetPressure();       // Lire la pression en Pascals (Pa)
        altitude = BMP_GetAltitude();       // Calculer l'altitude en mètres (m)

        // Afficher les valeurs (nécessite une configuration printf, par exemple via UART)
        printf("Temperature: %.2f C\r\n", temperature);
        printf("Pressure: %.2f Pa\r\n", pressure);
        printf("Altitude: %.2f m\r\n\n", altitude);

        HAL_Delay(1000); // Attendre 1 seconde
    /* USER CODE END WHILE */
    }
    ```

## API (Fonctions principales)

*   `void BMP_Init(I2C_HandleTypeDef *hi2c)`: Initialise le capteur en utilisant le handle I2C fourni et lit les coefficients de calibration internes. Doit être appelée une fois avant toute lecture. *(Vérifiez le type de retour dans votre fichier .h, il pourrait être `int` ou `HAL_StatusTypeDef` pour indiquer le succès/échec).*
*   `float BMP_GetTemperature(void)`: Déclenche une mesure de température, attend la conversion et retourne la température compensée en degrés Celsius.
*   `float BMP_GetPressure(void)`: Déclenche une mesure de pression (avec un certain niveau d'oversampling, souvent défini en interne dans la bibliothèque), attend la conversion et retourne la pression compensée en Pascals.
*   `float BMP_GetAltitude(void)`: Calcule et retourne l'altitude approximative en mètres, basée sur la dernière lecture de pression et une pression de référence au niveau de la mer (par défaut 101325 Pa). Cette fonction appelle généralement `BMP_GetPressure()` en interne si aucune lecture récente n'est disponible.

## Licence

Ce code est généralement fourni sous une licence permissive comme MIT ou BSD. Veuillez vérifier les fichiers sources (`BMP085_180.c` et `BMP085_180.h`) pour les détails spécifiques de la licence.
