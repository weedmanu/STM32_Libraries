# Bibliothèque STM32 HAL pour Capteur de Température et d'Humidité DHT20 (I2C)

Cette bibliothèque fournit une interface simple pour interagir avec le capteur de température et d'humidité DHT20 d'Aosong (ASAIR) en utilisant le bus I2C sur les microcontrôleurs STM32 via la couche d'abstraction matérielle (HAL) STM32.

## Fonctionnalités

*   Initialisation du capteur via I2C.
*   Vérification de l'état et de la calibration initiale du capteur.
*   Lecture de la température en degrés Celsius (°C).
*   Lecture de l'humidité relative en pourcentage (%RH).
*   Gestion des erreurs de communication et de lecture (timeout, checksum).

## Connexions Matérielles

Connectez le module DHT20 à votre microcontrôleur STM32 en utilisant le bus I2C :

*   **VCC** -> 3.3V (ou 5V si le module a un régulateur et un level shifter)
*   **GND** -> GND
*   **SCL** -> Broche SCL I2C du STM32 (ex: PB6, PB8, PB10 selon le MCU et la configuration I2C)
*   **SDA** -> Broche SDA I2C du STM32 (ex: PB7, PB9, PB11 selon le MCU et la configuration I2C)

Assurez-vous d'activer le périphérique I2C approprié dans votre configuration STM32CubeMX ou manuellement. Des résistances de pull-up (généralement 4.7kΩ ou 2.2kΩ) sont nécessaires sur les lignes SCL et SDA si elles ne sont pas déjà présentes sur le module ou la carte STM32. L'adresse I2C par défaut du BMP085/180 est `0x77` (adresse 7 bits, soit `0xEE` en écriture et `0xEF` en lecture).
L'adresse I2C par défaut du DHT20 est `0x38` (adresse 7 bits, soit `0x70` en écriture et `0x71` en lecture).

## Utilisation de base

Voici un exemple simple d'utilisation dans votre fichier `main.c` :

1.  **Inclure l'en-tête :**

    ```c
    /* USER CODE BEGIN Includes */
    #include <stdio.h>          // Pour printf
    #include "STM32_DHT20_I2C.h" // Inclure la bibliothèque du capteur DHT20
    /* USER CODE END Includes */
    ```

2.  **Déclarer les constantes préprocesseur :**

    ```c 
    /* USER CODE BEGIN PD */
    #define DHT20_I2C_ADDRESS 0x38 // Adresse I2C du DHT20 (7 bits)
    /* USER CODE END PD */
    ```

3.  **Déclarer des variables globales :**

    ```c
    /* USER CODE BEGIN PV */
    DHT20_Handle_t dht20_sensor; // Structure pour contenir les données du capteur DHT20
    float temperature;           // Température lue par le capteur DHT20
    float humidity;              // Humidité lue par le capteur DHT20
    /* USER CODE END PV */
    ```

3.  **Redirection `printf` pour l'affichage :**
    ```c
    /* USER CODE BEGIN 0 */
    // Fonction qui transmet un caractère via UART et le renvoie.Utilisé pour la sortie standard (printf).
    int __io_putchar(int ch) {
        HAL_UART_Transmit(&huart2, (uint8_t*) &ch, 1, 0xFFFF); // Pour Envoyer le caractère via UART
        // ITM_SendChar(ch);                 // Option alternative pour envoyer le caractère via ITM
        return ch;
    }
    /* USER CODE END 0 */
    ```

4.  **Initialiser le capteur :**

    ```c
    /* USER CODE BEGIN 2 */
  HAL_Delay(500);
    printf("--- Initialisation des capteurs ---\r\n");
    
    // Initialisation du capteur DHT20
    printf("Initialisation DHT20 (Adresse 0x%X)...\r\n", DHT20_I2C_ADDRESS);
    DHT20_Status_t dht20_status = DHT20_Init(&dht20_sensor, &hi2c1, DHT20_I2C_ADDRESS);
    
    if (dht20_status == DHT20_OK) {
        printf("DHT20 initialisé avec succès.\r\n");
    } else {
        printf("ERREUR: Initialisation DHT20 échouée ! Code: %d\r\n", dht20_status);
        // Vous pouvez choisir de continuer ou d'arrêter ici avec Error_Handler()
        // Error_Handler();
    }
  /* USER CODE END 2 */
    ```


5.  **Lire les données dans la boucle principale :**
    ```c
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        printf("\r\n--- Lecture DHT20 ---\r\n");
        
        // Lecture des données de température et d'humidité
        dht20_status = DHT20_ReadData(&dht20_sensor, &temperature, &humidity);
        
        if (dht20_status == DHT20_OK) {
            printf("DHT20 Température : %.2f C\r\n", temperature);
            printf("DHT20 Humidité    : %.2f %%RH\r\n", humidity);
        } else {
            printf("DHT20 Erreur lecture Temp/Hum. Code: %d\r\n", dht20_status);
        }

        HAL_Delay(2000); // Attente de 2 secondes avant la prochaine lecture
        /* USER CODE END WHILE */
    ```

## Référence API

### Structures Principales
*   `DHT20_Handle_t`: Structure principale contenant le handle I2C, l'adresse du capteur et potentiellement son état.


### Fonctions Principales

*   `DHT20_Status_t DHT20_Init(DHT20_Handle_t *dev, I2C_HandleTypeDef *hi2c, uint8_t dev_addr)`
    *   Initialise le capteur DHT20. Vérifie la communication et l'état initial du capteur.
    *   Retourne `DHT20_OK` en cas de succès, ou un code d'erreur `DHT20_ERR_xxx`.

*   `DHT20_Status_t DHT20_ReadData(DHT20_Handle_t *dev, float *temperature, float *humidity)`
    *   Déclenche une mesure, lit les données brutes, vérifie le CRC et calcule la température (°C) et l'humidité (%RH).
    *   Retourne `DHT20_OK` en cas de succès, ou un code d'erreur `DHT20_ERR_xxx`.

*   `DHT20_Status_t DHT20_CheckStatus(DHT20_Handle_t *dev)`
    *   Lit le registre d'état du capteur pour vérifier s'il est prêt ou calibré.
    *   Retourne `DHT20_OK` si le statut est correct, ou un code d'erreur `DHT20_ERR_xxx`.


### Codes d'Erreur (`DHT20_Status_t`)

*   `DHT20_OK = 0`
    *   Opération réussie.

*   `DHT20_ERR_I2C_TX = 1`
    *   Erreur de transmission I2C.

*   `DHT20_ERR_I2C_RX = 2`
    *   Erreur de réception I2C.

*   `DHT20_ERR_CHECKSUM = 3`
    *   Erreur de CRC lors de la lecture des données.

*   `DHT20_ERR_TIMEOUT = 4`
    *   Timeout en attendant la réponse ou la fin de mesure du capteur.

*   `DHT20_ERR_NULL_PTR = 5`
    *   Pointeur NULL fourni en argument.

*   `DHT20_ERR_INIT_FAILED = 6`
    *   Échec de l'initialisation (ex: capteur non détecté, état initial incorrect).
