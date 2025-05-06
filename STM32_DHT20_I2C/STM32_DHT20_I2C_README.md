# Bibliothèque STM32 HAL pour Capteur de Température et d'Humidité DHT20 (I2C)

Cette bibliothèque fournit une interface simple pour interagir avec le capteur de température et d'humidité DHT20 d'Aosong (ASAIR) en utilisant le bus I2C sur les microcontrôleurs STM32 via la couche d'abstraction matérielle (HAL) STM32.

## Fonctionnalités

*   Initialisation du capteur via I2C.
*   Vérification de l'état et de la calibration initiale du capteur.
*   Lecture de la température en degrés Celsius (°C).
*   Lecture de l'humidité relative en pourcentage (%RH).
*   Gestion des erreurs de communication et de lecture (timeout, checksum).
*   Vérification du CRC pour garantir l'intégrité des données.

## Connexions Matérielles

Connectez le module DHT20 à votre microcontrôleur STM32 en utilisant le bus I2C :

*   **VCC** -> 3.3V (ou 5V si le module a un régulateur et un level shifter)
*   **GND** -> GND
*   **SCL** -> Broche SCL I2C du STM32 (ex: PB6, PB8, PB10 selon le MCU et la configuration I2C)
*   **SDA** -> Broche SDA I2C du STM32 (ex: PB7, PB9, PB11 selon le MCU et la configuration I2C)

Assurez-vous d'activer le périphérique I2C approprié dans votre configuration STM32CubeMX ou manuellement. Des résistances de pull-up (généralement 4.7kΩ ou 2.2kΩ) sont nécessaires sur les lignes SCL et SDA si elles ne sont pas déjà présentes sur le module ou la carte STM32. L'adresse I2C par défaut du DHT20 est `0x38` (adresse 7 bits, soit `0x70` en écriture et `0x71` en lecture).

## Utilisation de base

Voici un exemple simple d'utilisation dans votre fichier `main.c` :

1.  **Inclure l'en-tête :**

    ```c
    /* USER CODE BEGIN Includes */
    #include <stdio.h>          // Pour printf
    #include "STM32_DHT20_I2C.h" // Inclure la bibliothèque du capteur DHT20
    /* USER CODE END Includes */
    ```

2.  **Déclarer des variables globales :**

    ```c
    /* USER CODE BEGIN PV */
    DHT20_Handle dht20_sensor; // Structure pour contenir les données du capteur DHT20
    float temperature;         // Température lue par le capteur DHT20
    float humidity;            // Humidité lue par le capteur DHT20
    /* USER CODE END PV */
    ```

3.  **Redirection `printf` pour l'affichage :**

    ```c
    /* USER CODE BEGIN 0 */
    // Fonction qui transmet un caractère via UART et le renvoie. Utilisé pour la sortie standard (printf).
    int __io_putchar(int ch) {
        HAL_UART_Transmit(&huart2, (uint8_t*) &ch, 1, 0xFFFF); // Pour envoyer le caractère via UART
        return ch;
    }
    /* USER CODE END 0 */
    ```

4.  **Initialiser le capteur :**

    ```c
    /* USER CODE BEGIN 2 */
    printf("--- Initialisation ---\r\n");
    // Initialiser le handle DHT20
    DHT20_Init(&dht20_sensor, &hi2c1, 100); // Initialise le capteur avec un timeout de 100ms
    if (DHT20_Check_Status(&dht20_sensor) == HAL_OK) {
        printf("Capteur DHT20 prêt.\r\n");
    } else {
        printf("Erreur lors de la vérification du statut du capteur DHT20.\r\n");
    }
    printf("--- Début de la lecture ---\r\n");
    /* USER CODE END 2 */
    ```

5.  **Lire les données dans la boucle principale :**

    ```c
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        // Lire les données via le handle
        HAL_StatusTypeDef dht_status = DHT20_ReadData(&dht20_sensor, &temperature, &humidity); 
        if (dht_status == HAL_OK) { 
            printf("Température: %.2f °C, Humidité: %.2f %%\r\n", temperature, humidity); // Afficher les données
        } else if (dht_status == HAL_BUSY) {
            printf("Erreur: Capteur DHT20 occupé.\r\n");
        } else if (dht_status == HAL_TIMEOUT) {
            printf("Erreur de timeout lors de la lecture du DHT20.\r\n");
        } else {
            printf("Echec de la lecture du DHT20, statut: %d\r\n", dht_status);
        }
        HAL_Delay(2000); // 2s entre chaque mesure
    }
    /* USER CODE END WHILE */
    ```

## Référence API

### Structures Principales

*   `DHT20_Handle`: Structure principale contenant le handle I2C, le timeout et l'adresse du capteur.

### Fonctions Principales

*   `void DHT20_Init(DHT20_Handle *dht, I2C_HandleTypeDef *hi2c, uint32_t timeout)`
    *   Initialise le capteur DHT20 avec le handle I2C et un timeout.
    *   Ne retourne rien.

*   `HAL_StatusTypeDef DHT20_Check_Status(DHT20_Handle *dht)`
    *   Vérifie l'état initial du capteur (calibration, disponibilité).
    *   Retourne `HAL_OK` en cas de succès ou un code d'erreur HAL.

*   `HAL_StatusTypeDef DHT20_ReadData(DHT20_Handle *dht, float *temperature, float *humidity)`
    *   Déclenche une mesure, lit les données brutes, vérifie le CRC et calcule la température (°C) et l'humidité (%RH).
    *   Retourne `HAL_OK` en cas de succès ou un code d'erreur HAL.

### Codes d'Erreur

*   `HAL_OK`
    *   Opération réussie.

*   `HAL_BUSY`
    *   Le capteur est occupé.

*   `HAL_TIMEOUT`
    *   Timeout en attendant la réponse ou la fin de mesure du capteur.

*   `HAL_ERROR`
    *   Erreur de communication ou de données (ex: CRC invalide).
