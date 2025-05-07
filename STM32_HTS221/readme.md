# Programme STM32 pour le capteur HTS221

## Description
Ce programme est conçu pour fonctionner avec une carte STM32 et le capteur d'humidité et de température **HTS221**. Il initialise capteur, configure le capteur, lit les données de température et d'humidité, et les affiche via l'interface UART.

---

## Fonctionnalités principales
1. **Scan du bus I2C** : Identifie les périphériques connectés au bus I2C.
2. **Initialisation du capteur HTS221** :
   - Vérifie la présence du capteur via le registre `WHO_AM_I`.
   - Configure le capteur en mode actif.
   - Lit les coefficients de calibration nécessaires pour les calculs.
3. **Lecture des données** :
   - Température en degrés Celsius.
   - Humidité relative en pourcentage.
4. **Affichage des données** : Les données sont affichées via l'interface UART (USART2).

---

## Configuration matérielle
- **Carte STM32** : Compatible avec STM32CubeIDE.
- **Capteur HTS221** : Connecté via I2C.
- **Interface UART** : Utilisée pour afficher les données sur un terminal série.

---

## Instructions pour l'exécution
1. Connectez le capteur HTS221 au bus I2C de la carte STM32.
2. Configurez un terminal série (ex. PuTTY) pour lire les données via UART :
   - **Baudrate** : 115200
   - **Parité** : Aucune
   - **Bits de données** : 8
   - **Bits d'arrêt** : 1
3. Compilez et téléchargez le programme sur la carte STM32 via STM32CubeIDE.
4. Observez les données affichées sur le terminal série.

---

## Structure du programme

### 1. Initialisation
- **Périphériques configurés** :
  - GPIO
  - UART (USART2)
  - I2C (I2C2)
- **Configuration du capteur** :
  - Activation via le registre `CTRL_REG1`.
  - Lecture des coefficients de calibration pour les calculs de température et d'humidité.

### 2. Boucle principale
- Lecture des données de température et d'humidité.
- Affichage des données via UART toutes les secondes.

---

## Modifications importantes
- **Choix du bus I2C** : Le programme permet désormais de spécifier dynamiquement le bus I2C à utiliser lors de l'initialisation du capteur HTS221. Cela est fait en passant un pointeur vers la structure `I2C_HandleTypeDef` lors de l'appel à `HTS221_Init`.

---

## Programme main.c

1. Includes nécessaires

```c
/* USER CODE BEGIN Includes */
#include <stdio.h>          // pour printf
/* USER CODE END Includes */
```

2. Déclaration des defines 

```c
/* USER CODE BEGIN PD */
#define HTS221_I2C_ADDRESS    0x5F << 1  // Adresse I2C pour le HTS221
#define HTS221_WHO_AM_I       0x0F       // Registre WHO_AM_I
#define HTS221_CTRL_REG1      0x20       // Registre de contrôle 1
#define HTS221_HUMIDITY_OUT_L 0x28       // Registre LSB de l'humidité
#define HTS221_HUMIDITY_OUT_H 0x29       // Registre MSB de l'humidité
#define HTS221_TEMP_OUT_L     0x2A       // Registre LSB de la température
#define HTS221_TEMP_OUT_H     0x2B       // Registre MSB de la température
/* USER CODE END PD */
```

3. Déclaration des variables globales

```c
/* USER CODE BEGIN PV */
/* Données de calibration du HTS221 */
float T0_degC = 0.0;    // Température de calibration T0 en °C
float T1_degC = 0.0;    // Température de calibration T1 en °C
int16_t T0_OUT = 0;     // Valeur brute de calibration T0
int16_t T1_OUT = 0;     // Valeur brute de calibration T1
float H0_rh = 0.0;      // Humidité de calibration H0 en %RH
float H1_rh = 0.0;      // Humidité de calibration H1 en %RH
int16_t H0_T0_OUT = 0;  // Valeur brute de calibration H0
int16_t H1_T0_OUT = 0;  // Valeur brute de calibration H1
/* USER CODE END PV */
```

4. Déclaration du prototype de la fonction HTS221_ReadCalibration

```c
/* USER CODE BEGIN PFP */
void HTS221_ReadCalibration(I2C_HandleTypeDef *hi2c); // Prototype de la fonction de lecture des coefficients de calibration
/* USER CODE END PFP */
```

5. Fonctions pour rediriger printf vers UART2, scanner le bus I2C et pour utiliser le capteur HTS221

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

/**
 * @brief  Scanne le bus I2C pour trouver les périphériques connectés
 * @param  hi2c: Pointeur vers la structure de gestion du bus I2C
 * @retval Aucun
 */
void I2C_Scan(I2C_HandleTypeDef *hi2c)
{
    HAL_Delay(500);                 // Petite pause de 500 ms
    printf("Scanner I2C\r\n\r\n");  // Affiche le début du processus de scan I2C
    char Buffer[8] = {0};           // Buffer pour stocker les adresses formatées
    uint8_t i = 0;                  // Index pour la boucle
    uint8_t ret;                    // Valeur de retour pour vérifier les périphériques

    /*-[ Scan du bus I2C ]-*/
    printf("Début du scan I2C : \r\n"); // Indique le début du scan
    for(i = 1; i < 128; i++)        // Parcourt toutes les adresses I2C possibles (1-127)
    {
        // Vérifie si un périphérique est prêt à l'adresse 'i' (décalée à gauche pour le format 7 bits)
        ret = HAL_I2C_IsDeviceReady(hi2c, (uint16_t)(i << 1), 3, 5);
        if (ret != HAL_OK)          // Si le périphérique n'est pas prêt
        {
            printf(" * ");          // Affiche une étoile pour les adresses non répondantes
        }
        else if (ret == HAL_OK)     // Si le périphérique est prêt
        {
            sprintf(Buffer, "0x%X", i);     // Formate l'adresse en chaîne hexadécimale
            printf("%s", (uint8_t *)Buffer); // Affiche l'adresse du périphérique trouvé
        }
    }
    printf("\r\nScan terminé ! \r\n\r\n"); // Indique la fin du scan
}

/*----------------------------------------------------------------------------------*/
/*                       FONCTIONS POUR LE CAPTEUR HTS221                           */
/*----------------------------------------------------------------------------------*/

/**
 * @brief  Écrit dans un registre du HTS221
 * @param  hi2c: Pointeur vers la structure I2C
 * @param  reg: Adresse du registre
 * @param  value: Valeur à écrire
 * @retval Aucun
 */
void HTS221_WriteRegister(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    if (HAL_I2C_Master_Transmit(hi2c, HTS221_I2C_ADDRESS, data, 2, HAL_MAX_DELAY) != HAL_OK)
    {
        printf("Erreur lors de l'écriture dans le registre 0x%02X\r\n", reg);
    }
}

/**
 * @brief  Lit un registre du HTS221
 * @param  hi2c: Pointeur vers la structure I2C
 * @param  reg: Adresse du registre
 * @retval Valeur du registre
 */
uint8_t HTS221_ReadRegister(I2C_HandleTypeDef *hi2c, uint8_t reg)
{
    uint8_t value = 0;
    if (HAL_I2C_Master_Transmit(hi2c, HTS221_I2C_ADDRESS, &reg, 1, HAL_MAX_DELAY) != HAL_OK)
    {
        printf("Erreur lors de l'envoi du registre 0x%02X à l'adresse 0x%02X\r\n", reg, HTS221_I2C_ADDRESS >> 1);
        return 0;
    }
    if (HAL_I2C_Master_Receive(hi2c, HTS221_I2C_ADDRESS, &value, 1, HAL_MAX_DELAY) != HAL_OK)
    {
        printf("Erreur lors de la lecture du registre 0x%02X à l'adresse 0x%02X\r\n", reg, HTS221_I2C_ADDRESS >> 1);
        return 0;
    }
    return value;
}

/**
 * @brief  Initialise le capteur HTS221
 * @retval Aucun
 */
void HTS221_Init(I2C_HandleTypeDef *hi2c)
{
    uint8_t who_am_i = HTS221_ReadRegister(hi2c, HTS221_WHO_AM_I);
    if (who_am_i != 0xBC)  // Valeur attendue pour le HTS221
    {
        printf("Erreur : HTS221 non détecté (WHO_AM_I = 0x%02X). Vérifiez l'adresse I2C ou les connexions.\r\n", who_am_i);
        return;
    }
    printf("HTS221 détecté (WHO_AM_I = 0x%02X)\r\n", who_am_i);

    // Configure le capteur (mode actif, ODR = 1 Hz)
    HTS221_WriteRegister(hi2c, HTS221_CTRL_REG1, 0x80);  // Active le capteur

    // Lit les coefficients de calibration
    HTS221_ReadCalibration(hi2c);
}

/**
 * @brief  Lit les coefficients de calibration du HTS221
 * @retval Aucun
 */
void HTS221_ReadCalibration(I2C_HandleTypeDef *hi2c)
{
    // Lecture des coefficients de calibration pour la température
    uint8_t T0_degC_x8 = HTS221_ReadRegister(hi2c, 0x32);
    uint8_t T1_degC_x8 = HTS221_ReadRegister(hi2c, 0x33);
    uint8_t T0_T1_msb = HTS221_ReadRegister(hi2c, 0x35);

    T0_degC = (float)((T0_degC_x8 | ((T0_T1_msb & 0x03) << 8)) / 8.0);
    T1_degC = (float)((T1_degC_x8 | ((T0_T1_msb & 0x0C) << 6)) / 8.0);

    T0_OUT = (int16_t)(HTS221_ReadRegister(hi2c, 0x3C) | (HTS221_ReadRegister(hi2c, 0x3D) << 8));
    T1_OUT = (int16_t)(HTS221_ReadRegister(hi2c, 0x3E) | (HTS221_ReadRegister(hi2c, 0x3F) << 8));

    // Lecture des coefficients de calibration pour l'humidité
    uint8_t H0_rh_x2 = HTS221_ReadRegister(hi2c, 0x30);
    uint8_t H1_rh_x2 = HTS221_ReadRegister(hi2c, 0x31);

    H0_rh = (float)(H0_rh_x2 / 2.0);
    H1_rh = (float)(H1_rh_x2 / 2.0);

    H0_T0_OUT = (int16_t)(HTS221_ReadRegister(hi2c, 0x36) | (HTS221_ReadRegister(hi2c, 0x37) << 8));
    H1_T0_OUT = (int16_t)(HTS221_ReadRegister(hi2c, 0x3A) | (HTS221_ReadRegister(hi2c, 0x3B) << 8));
}

/**
 * @brief  Lit la température depuis le capteur HTS221
 * @retval Température en degrés Celsius
 */
float HTS221_ReadTemperature(void)
{
    uint8_t temp_l = HTS221_ReadRegister(&hi2c1, HTS221_TEMP_OUT_L);
    uint8_t temp_h = HTS221_ReadRegister(&hi2c1, HTS221_TEMP_OUT_H);

    int16_t temp_raw = (int16_t)((temp_h << 8) | temp_l);

    // Conversion en degrés Celsius en utilisant les données de calibration
    return T0_degC + ((float)(temp_raw - T0_OUT) * (T1_degC - T0_degC) / (T1_OUT - T0_OUT));
}

/**
 * @brief  Lit l'humidité depuis le capteur HTS221
 * @retval Humidité relative en pourcentage
 */
float HTS221_ReadHumidity(void)
{
    uint8_t hum_l = HTS221_ReadRegister(&hi2c1, HTS221_HUMIDITY_OUT_L);
    uint8_t hum_h = HTS221_ReadRegister(&hi2c1, HTS221_HUMIDITY_OUT_H);

    int16_t hum_raw = (int16_t)((hum_h << 8) | hum_l);

    // Conversion en humidité relative en utilisant les données de calibration
    return H0_rh + ((float)(hum_raw - H0_T0_OUT) * (H1_rh - H0_rh) / (H1_T0_OUT - H0_T0_OUT));
}
/* USER CODE END 0 */
```

5. Scan du bus I2C2 et initialisation du capteur HTS221 (modifier l'i2c de votre choix)


```c
/* USER CODE BEGIN 2 */
printf("=== Scan du bus I2C2 ===\r\n");
I2C_Scan(&hi2c2); // Lance le scan du bus I2C2 pour détecter les périphériques connectés
printf("Initialisation du capteur d'humidité et de température HTS221...\r\n");
HTS221_Init(&hi2c2);  // Initialise le capteur de température et d'humidité HTS221
HAL_Delay(100); // Pause pour permettre au capteur de s'initialiser correctement
HTS221_ReadCalibration(&hi2c2); // Lit les coefficients de calibration du capteur HTS221
HAL_Delay(100); // Pause pour garantir la stabilité après la lecture des données de calibration
/* USER CODE END 2 */
```

6. Boucle principale

```c
/* USER CODE BEGIN WHILE */
while (1) {    
    printf("\r\n=== LECTURES DES CAPTEURS ===\r\n"); // Afficher un en-tête pour les lectures des capteurs   
    float temperature_hts = HTS221_ReadTemperature(); // Lire la température en °C
    float humidity = HTS221_ReadHumidity();           // Lire l'humidité relative en %
    printf("HTS221 | Température : %.2f °C | Humidité : %.2f %%\r\n", temperature_hts, humidity);  // Afficher les lectures
    HAL_Delay(1000); // Attendre 1 seconde avant la prochaine lecture
}
/* USER CODE END WHILE */
