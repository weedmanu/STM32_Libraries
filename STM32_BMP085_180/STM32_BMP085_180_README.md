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

## Utilisation de base

Voici un exemple simple d'utilisation dans votre fichier `main.c` :

1.  **Inclure l'en-tête :**

    ```c
    /* USER CODE BEGIN Includes */
    #include <stdio.h>      // Pour printf
    #include "BMP085_180.h" // Inclure la bibliothèque du capteur
    /* USER CODE END Includes */
    ```

2.  **Déclarer les constantes préprocesseur :**

    ```c 
    /* USER CODE BEGIN PD */
    #define STD_ATM_PRESS 101325 // Pression atmosphérique standard au niveau de la mer en Pascal (gardé ici car spécifique à l'application)
    #define BMP_I2C_ADRESS  0x77 // Adresse I2C du BMP180 (7 bits)
    /* USER CODE END PD */
    ```


3.  **Déclarer des variables globales :**

    ```c
    /* USER CODE BEGIN PV */
    BMP_Handle_t bmp_sensor;    // Structure pour contenir les données du capteur BMP
    float temperature;          // Température lue par le capteur BMP
    int32_t pressure_pa;        // Pression lue par le capteur BMP (en Pascal)
    float altitude;             // Altitude calculée à partir de la pression lue (en mètres)
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

    // Initialisation du capteur BMP180
    printf("Initialisation BMP180 (Adresse 0x%X)...\r\n", BMP_I2C_ADRESS);
    // --- CHOIX DE L'INITIALISATION ---
    // Option 1: Spécifier le mode explicitement (ex: BMP_ULTRAHIGHRES, BMP_HIGHRES, BMP_STANDARD, BMP_ULTRALOWPOWER)
    BMP_Status_t bmp_status = BMP_Init(&bmp_sensor, BMP_ULTRAHIGHRES, &hi2c1, BMP_I2C_ADRESS);
    // Option 2: Utiliser le mode par défaut (défini comme BMP_STANDARD dans le .h)
    //BMP_Status_t bmp_status = BMP_Init_Default(&bmp_sensor, &hi2c1, BMP_I2C_ADRESS);
    if (bmp_status == BMP_OK) {
        printf("BMP180 initialisé avec succès.\r\n");
    } else {
        printf("ERREUR: Initialisation BMP180 échouée ! Code: %d\r\n", bmp_status);
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
        printf("\r\n--- Lecture BMP085/180 ---\r\n");
        bmp_status = BMP_readAll(&bmp_sensor, &temperature, &pressure_pa); // Lecture optimisée

        if (bmp_status == BMP_OK) {
            printf("BMP Température : %.2f °C\r\n", temperature);
            printf("BMP Pression    : %ld Pa\r\n", pressure_pa);

            // Calcul de l'altitude à partir de la pression lue
            BMP_Status_t alt_status = BMP_calculateAltitude(pressure_pa, (float)STD_ATM_PRESS, &altitude);
            if (alt_status == BMP_OK) {
                // Vérifie si le calcul a réussi (pas besoin de isnan si la fonction retourne OK)
                    printf("BMP Altitude    : %.2f m (estimée vs %d Pa)\r\n", altitude, STD_ATM_PRESS);
            } else {
                // L'erreur est maintenant gérée par le code de retour de BMP_calculateAltitude
                // On pourrait afficher un message plus spécifique basé sur alt_status si nécessaire
                // Par exemple, si alt_status == BMP_ERR_INVALID_PARAM
                printf("BMP Erreur calcul altitude (pression invalide: %ld Pa)\r\n", pressure_pa);
            }
        } else {
            printf("BMP Erreur lecture Temp/Press. Code: %d\r\n", bmp_status);
        }

        HAL_Delay(2000); // Attente de 2 secondes avant la prochaine lecture
        /* USER CODE END WHILE */
    ```

## Référence API

### Structures Principales

*   `BMP_Handle_t`: Structure principale contenant le handle I2C et l'adresse du capteur.

### Fonctions Principales

*   `BMP_Status_t BMP_Init(BMP_Handle_t *dev, BMP_Mode_t mode, I2C_HandleTypeDef *hi2c, uint8_t dev_addr)`
    *   Initialise le capteur BMP180 en fonction du mode spécifié.
    *   Retourne `BMP_OK` en cas de succès, ou un code d'erreur `BMP_ERROR_xxx`.

*   `BMP_Status_t BMP_Init_Default(BMP_Handle_t *dev, I2C_HandleTypeDef *hi2c, uint8_t dev_addr)`
    *   Initialise le capteur BMP180 avec le mode par défaut (BMP_STANDARD).
    *   Retourne `BMP_OK` en cas de succès, ou un code d'erreur `BMP_ERROR_xxx`.

*   `BMP_Status_t BMP_readAll(BMP_Handle_t *dev, float *temperature, int32_t *pressure)`
    *   Lit la température et la pression depuis le capteur BMP180.
    *   Retourne `BMP_OK` en cas de succès, ou un code d'erreur `BMP_ERROR_xxx`.

*   `BMP_Status_t BMP_calculateAltitude(int32_t pressure, float sea_level_pressure, float *altitude)`
    *   Calcule l'altitude approximative en fonction de la pression mesurée et de la pression au niveau de la mer.
    *   Retourne `BMP_OK` en cas de succès, ou un code d'erreur `BMP_ERROR_xxx`.

*   `BMP_Status_t BMP_readTemperature(BMP_Handle_t *dev, float *temperature)`
    *   Lit uniquement la température depuis le capteur BMP180.
    *   Retourne `BMP_OK` en cas de succès, ou un code d'erreur `BMP_ERROR_xxx`.

*   `BMP_Status_t BMP_readPressure(BMP_Handle_t *dev, int32_t *pressure)`
    *   Lit uniquement la pression depuis le capteur BMP180.
    *   Retourne `BMP_OK` en cas de succès, ou un code d'erreur `BMP_ERROR_xxx`.


### Mode de fonctionnement (`mode`)

    Détermine comment le capteur effectue les mesures, en influençant la précision et le temps de conversion :

    - `BMP_ULTRALOWPOWER` : Mode basse consommation, temps de conversion rapide, précision minimale.
    - `BMP_STANDARD` : Mode standard, équilibre entre consommation, temps de conversion et précision.
    - `BMP_HIGHRES` : Mode haute résolution, temps de conversion plus long, meilleure précision.
    - `BMP_ULTRAHIGHRES` : Mode ultra haute résolution, temps de conversion maximal, précision optimale.

    Le mode est spécifié lors de l'initialisation du capteur avec la fonction `BMP_Init`. Par défaut, le mode est défini comme `BMP_STANDARD` si vous utilisez `BMP_Init_Default`.

### Codes d'Erreur (`BMP_Status_t`)

*   `BMP_OK = 0`  
    *   Opération réussie.

*   `BMP_ERR_I2C_TX = 1`  
    *   Erreur de transmission I2C.

*   `BMP_ERR_I2C_RX = 2`  
    *   Erreur de réception I2C.

*   `BMP_ERR_INVALID_ID = 3`  
    *   ID de puce incorrect lu.

*   `BMP_ERR_CAL_READ = 4`  
    *   Erreur lors de la lecture des données de calibration.

*   `BMP_ERR_NULL_PTR = 5`  
    *   Pointeur NULL fourni en argument.

*   `BMP_ERR_INVALID_PARAM = 6`  
    *   Paramètre invalide fourni (ex: mode, pression/altitude négative).

*   `BMP_ERR_MATH = 7`  
    *   Erreur de calcul interne (ex: division par zéro).




