# Programme STM32 pour le capteur HTS221

## Description
Ce programme est conçu pour fonctionner avec une carte STM32 et le capteur d'humidité et de température **HTS221** monté sur le shield **X-NUCLEO-IKS01A3**. Il initialise les périphériques nécessaires, configure le capteur, lit les données de température et d'humidité, et les affiche via l'interface UART.

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
   - **Parité** : None
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
  - I2C (I2C1)
- **Configuration du capteur** :
  - Activation via le registre `CTRL_REG1`.
  - Lecture des coefficients de calibration pour les calculs de température et d'humidité.

### 2. Boucle principale
- Lecture des données de température et d'humidité.
- Affichage des données via UART toutes les secondes.

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
#define HTS221_I2C_ADDRESS    0x5F << 1  // I2C address for the HTS221
#define HTS221_WHO_AM_I       0x0F       // WHO_AM_I register
#define HTS221_CTRL_REG1      0x20       // Control register 1
#define HTS221_HUMIDITY_OUT_L 0x28       // Humidity LSB register
#define HTS221_HUMIDITY_OUT_H 0x29       // Humidity MSB register
#define HTS221_TEMP_OUT_L     0x2A       // Temperature LSB register
#define HTS221_TEMP_OUT_H     0x2B       // Temperature MSB register
/* USER CODE END PD */
```

3. Déclaration des variables globales

```c
/* USER CODE BEGIN PV */
/* HTS221 calibration data */
float T0_degC = 0.0;    // T0 calibration temperature in °C
float T1_degC = 0.0;    // T1 calibration temperature in °C
int16_t T0_OUT = 0;     // T0 calibration raw value
int16_t T1_OUT = 0;     // T1 calibration raw value
float H0_rh = 0.0;      // H0 calibration humidity in %RH
float H1_rh = 0.0;      // H1 calibration humidity in %RH
int16_t H0_T0_OUT = 0;  // H0 calibration raw value
int16_t H1_T0_OUT = 0;  // H1 calibration raw value
/* USER CODE END PV */
```

4. Déclaration du prototype de la fonction HTS221_ReadCalibration

```c
/* USER CODE BEGIN PFP */
void HTS221_ReadCalibration(void);
/* USER CODE END PFP */
```

5. Fonctions pour rediriger printf vers UART2, scanner le bus I2C et pour utiliser le capteur HTS221

```c
/* USER CODE BEGIN 0 */
/**
 * @brief  Redirect printf output to UART2
 * @param  ch: Character to send
 * @retval Character sent
 */
int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart2, (uint8_t*) &ch, 1, 0xFFFF); // Send character through UART2
    return ch;
}

/**
 * @brief  Scan I2C bus to find connected devices
 * @param  hi2c: Pointer to I2C handle
 * @retval None
 */
void I2C_Scan(I2C_HandleTypeDef *hi2c)
{
    HAL_Delay(500);                 // Small delay of 500ms
    printf("I2C Scanner\r\n\r\n");  // Display start of I2C scanning process
    char Buffer[8] = {0};           // Buffer to store formatted address
    uint8_t i = 0;                  // Index for loop
    uint8_t ret;                    // Return value from device check

    /*-[ I2C Bus Scanning ]-*/
    printf("Starting I2C scan: \r\n");
    for(i = 1; i < 128; i++)        // Scan all possible I2C addresses (1-127)
    {
        // Check if a device is ready at address 'i' (left-shifted by 1 to match 7-bit address format)
        ret = HAL_I2C_IsDeviceReady(hi2c, (uint16_t)(i << 1), 3, 5);
        if (ret != HAL_OK)          // If device is not ready
        {
            printf(" * ");          // Print asterisk for non-responding address
        }
        else if (ret == HAL_OK)     // If device is ready
        {
            sprintf(Buffer, "0x%X", i);     // Format address as hex string
            printf("%s", (uint8_t *)Buffer); // Print found device address
        }
    }
    printf("\r\nScan complete! \r\n\r\n");
}

/*----------------------------------------------------------------------------------*/
/*                       HTS221 HUMIDITY & TEMPERATURE FUNCTIONS                     */
/*----------------------------------------------------------------------------------*/

/**
 * @brief  Write to HTS221 register
 * @param  reg: Register address
 * @param  value: Value to write
 * @retval None
 */
void HTS221_WriteRegister(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    if (HAL_I2C_Master_Transmit(&hi2c1, HTS221_I2C_ADDRESS, data, 2, HAL_MAX_DELAY) != HAL_OK)
    {
        printf("Error writing to register 0x%02X\r\n", reg);
    }
}

/**
 * @brief  Read from HTS221 register
 * @param  reg: Register address
 * @retval Register value
 */
uint8_t HTS221_ReadRegister(uint8_t reg)
{
    uint8_t value = 0;
    if (HAL_I2C_Master_Transmit(&hi2c1, HTS221_I2C_ADDRESS, &reg, 1, HAL_MAX_DELAY) != HAL_OK)
    {
        printf("Error sending register 0x%02X to address 0x%02X\r\n", reg, HTS221_I2C_ADDRESS >> 1);
        return 0;
    }
    if (HAL_I2C_Master_Receive(&hi2c1, HTS221_I2C_ADDRESS, &value, 1, HAL_MAX_DELAY) != HAL_OK)
    {
        printf("Error reading register 0x%02X from address 0x%02X\r\n", reg, HTS221_I2C_ADDRESS >> 1);
        return 0;
    }
    return value;
}

/**
 * @brief  Initialize HTS221 humidity and temperature sensor
 * @retval None
 */
void HTS221_Init(void)
{
    uint8_t who_am_i = HTS221_ReadRegister(HTS221_WHO_AM_I);
    if (who_am_i != 0xBC)  // Expected value for HTS221
    {
        printf("Error: HTS221 not detected (WHO_AM_I = 0x%02X). Check I2C address or connections.\r\n", who_am_i);
        return;
    }
    printf("HTS221 detected (WHO_AM_I = 0x%02X)\r\n", who_am_i);

    // Configure sensor (active mode, ODR = 1 Hz)
    HTS221_WriteRegister(HTS221_CTRL_REG1, 0x80);  // Activate sensor

    // Read calibration coefficients
    HTS221_ReadCalibration();
}

/**
 * @brief  Read calibration coefficients from HTS221
 * @retval None
 */
void HTS221_ReadCalibration(void)
{
    // Read temperature calibration coefficients
    uint8_t T0_degC_x8 = HTS221_ReadRegister(0x32);
    uint8_t T1_degC_x8 = HTS221_ReadRegister(0x33);
    uint8_t T0_T1_msb = HTS221_ReadRegister(0x35);

    T0_degC = (float)((T0_degC_x8 | ((T0_T1_msb & 0x03) << 8)) / 8.0);
    T1_degC = (float)((T1_degC_x8 | ((T0_T1_msb & 0x0C) << 6)) / 8.0);

    T0_OUT = (int16_t)(HTS221_ReadRegister(0x3C) | (HTS221_ReadRegister(0x3D) << 8));
    T1_OUT = (int16_t)(HTS221_ReadRegister(0x3E) | (HTS221_ReadRegister(0x3F) << 8));

    // Read humidity calibration coefficients
    uint8_t H0_rh_x2 = HTS221_ReadRegister(0x30);
    uint8_t H1_rh_x2 = HTS221_ReadRegister(0x31);

    H0_rh = (float)(H0_rh_x2 / 2.0);
    H1_rh = (float)(H1_rh_x2 / 2.0);

    H0_T0_OUT = (int16_t)(HTS221_ReadRegister(0x36) | (HTS221_ReadRegister(0x37) << 8));
    H1_T0_OUT = (int16_t)(HTS221_ReadRegister(0x3A) | (HTS221_ReadRegister(0x3B) << 8));
}

/**
 * @brief  Read temperature from HTS221 sensor
 * @retval Temperature in degrees Celsius
 */
float HTS221_ReadTemperature(void)
{
    uint8_t temp_l = HTS221_ReadRegister(HTS221_TEMP_OUT_L);
    uint8_t temp_h = HTS221_ReadRegister(HTS221_TEMP_OUT_H);

    int16_t temp_raw = (int16_t)((temp_h << 8) | temp_l);

    // Convert to degrees Celsius using calibration data
    return T0_degC + ((float)(temp_raw - T0_OUT) * (T1_degC - T0_degC) / (T1_OUT - T0_OUT));
}

/**
 * @brief  Read humidity from HTS221 sensor
 * @retval Relative humidity in percent
 */
float HTS221_ReadHumidity(void)
{
    uint8_t hum_l = HTS221_ReadRegister(HTS221_HUMIDITY_OUT_L);
    uint8_t hum_h = HTS221_ReadRegister(HTS221_HUMIDITY_OUT_H);

    int16_t hum_raw = (int16_t)((hum_h << 8) | hum_l);

    // Convert to relative humidity using calibration data
    return H0_rh + ((float)(hum_raw - H0_T0_OUT) * (H1_rh - H0_rh) / (H1_T0_OUT - H0_T0_OUT));
}
/* USER CODE END 0 */
```

5. Scan du bus I2C1 et initialisation du capteur HTS221 (modifier l'i2c de votre choix)


```c
/* USER CODE BEGIN 2 */
printf("=== Scan du bus I2C1 ===\r\n");
I2C_Scan(&hi2c1); // Lance le scan du bus I2C1 pour détecter les périphériques connectés
printf("Initialisation du capteur d'humidité et de température HTS221...\r\n");
HTS221_Init();  // Initialise le capteur de température et d'humidité HTS221
HAL_Delay(100); // Pause pour permettre au capteur de s'initialiser correctement
HTS221_ReadCalibration(); // Lit les coefficients de calibration du capteur HTS221
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
    /* USER CODE END WHILE */
```