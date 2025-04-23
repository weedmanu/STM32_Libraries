# Bibliothèque STM32_ADS1115 pour STM32 HAL

## Introduction

Cette bibliothèque fournit une interface pour le convertisseur Analogique-Numérique (ADC) 16 bits I2C ADS1115 de Texas Instruments, destinée à être utilisée avec les microcontrôleurs STM32 et la couche d'abstraction matérielle (HAL) de STMicroelectronics.

Elle permet de lire les tensions sur les entrées analogiques en mode simple (single-ended) ou différentiel, de configurer le gain et le taux d'échantillonnage, d'utiliser les modes de conversion unique (single-shot) ou continu, et de configurer le comparateur intégré avec sa broche d'alerte. La bibliothèque supporte également la gestion de plusieurs modules ADS1115 sur le même bus I2C.

## Caractéristiques

*   Lecture des 4 canaux en mode Single-Ended (AIN0, AIN1, AIN2, AIN3 vs GND).
*   Lecture des paires différentielles (AIN0-AIN1, AIN0-AIN3, AIN1-AIN3, AIN2-AIN3).
*   Support du mode de conversion unique (Single-Shot).
*   Support du mode de conversion continu.
*   Configuration du gain de l'amplificateur programmable (PGA) de ±0.256V à ±6.144V.
*   Configuration du taux d'échantillonnage (Data Rate) de 8 SPS à 860 SPS.
*   Configuration complète du comparateur :
    *   Seuils haut et bas.
    *   Mode traditionnel (hystérésis) ou fenêtre.
    *   Polarité de la broche ALERT/RDY (Active Low / Active High).
    *   Mode verrouillé (Latching) ou non-verrouillé.
    *   File d'attente du comparateur (nombre de conversions avant alerte).
*   Gestion de plusieurs modules ADS1115 sur un seul bus I2C (jusqu'à `MAX_ADS1115_MODULES`).
*   Fonctions utilitaires pour convertir les lectures brutes en Volts et Millivolts.
*   Basée sur STM32 HAL pour une meilleure portabilité entre les projets STM32.
*   Gestion robuste des erreurs I2C et des timeouts de conversion.

## Connexion Matérielle

Connectez le module ADS1115 à votre STM32 comme suit :

*   **VCC** -> Alimentation (ex: 3.3V ou 5V, selon votre module et STM32)
*   **GND** -> Masse (GND)
*   **SCL** -> Broche SCL de l'I2C du STM32 (ex: PB6 pour I2C1 sur L476RG, voir `stm32l4xx_hal_msp.c`)
*   **SDA** -> Broche SDA de l'I2C du STM32 (ex: PB7 pour I2C1 sur L476RG, voir `stm32l4xx_hal_msp.c`)
*   **ADDR** -> Connectez à l'une des broches suivantes pour sélectionner l'adresse I2C (adresses 7 bits) :
    *   **GND :** `0x48` (Adresse par défaut)
    *   **VCC (VDD) :** `0x49`
    *   **SDA :** `0x4A`
    *   **SCL :** `0x4B`
*   **ALERT/RDY** -> Connectez à une broche GPIO du STM32 configurée en entrée (ex: PA10, voir `main.c` et `MX_GPIO_Init`) **si vous utilisez le mode comparateur ou si vous voulez détecter la fin de conversion matériellement**. Une résistance de pull-up externe ou l'activation du pull-up interne du STM32 est souvent nécessaire si la polarité est Active Low.

## Configuration Logicielle

### 1. Fichiers de la Bibliothèque

*   Copiez `STM32_ADS1115.c` dans le dossier `Core/Src` de votre projet STM32CubeIDE.
*   Copiez `STM32_ADS1115.h` dans le dossier `Core/Inc` de votre projet STM32CubeIDE.


### 2. Configuration STM32CubeMX

*   **I2C :** Activez le périphérique I2C que vous utilisez (ex: I2C1).
    *   Configurez-le en mode `I2C`.
    *   Assurez-vous que la vitesse (Timing) est correctement configurée pour votre fréquence d'horloge (ex: 100kHz ou 400kHz). Le timing `0x10D19CE4` est un exemple pour L4 à 80MHz et I2C à 100kHz, adaptez-le si nécessaire.
*   **GPIO (pour ALERT/RDY) :** Si vous utilisez le mode comparateur :
    *   Configurez la broche GPIO connectée à ALERT/RDY (ex: PA10) en mode `GPIO_Input`.
    *   Configurez le `Pull-up/Pull-down` :
        *   Si `COMPARATOR_POLARITY` est `ADS1115_REG_CONFIG_CPOL_ACTVLOW` (Active Low), activez le `Pull-up`.
        *   Si `COMPARATOR_POLARITY` est `ADS1115_REG_CONFIG_CPOL_ACTVHI` (Active High), configurez en `No pull-up and no pull-down`.
*   **UART (pour les exemples) :** Si vous utilisez `printf` pour le débogage (comme dans les exemples `main.c`), activez un périphérique UART (ex: USART2) en mode Asynchrone.

## Utilisation (API)

Modification du main.c :

### 1. Mode Single-Ended

On importe les librairie nécessaires en copiant ces lignes entre les balises `USER CODE BEGIN Includes` et `USER CODE END Includes` :

```c
/* USER CODE BEGIN Includes */
#include <stdio.h>            // Inclure la bibliothèque standard pour le printf
#include <STM32_ADS1115.h>    // Inclure la bibliothèque STM32_ADS1115
/* USER CODE END Includes */
```

On Définie l'adresse I2C du module ADS1115 en copiant ces lignes entre les balises `USER CODE BEGIN PD` et `USER CODE END PD` : 

```c
/* USER CODE BEGIN PD */
#define ADS1115_ADDRESS 0x48 // Adresse I2C de votre module ADS1115 (pin ADD sur GND par défaut)
/* USER CODE END PD */
```

On définit un index pour la gestion des modules en copiant ces lignes entre les balises `USER CODE BEGIN PV` et `USER CODE END PV` : 

```c
/* USER CODE BEGIN PV */
int module_index = -1; // Index du module ADS1115 ajouté (sera mis à jour après l'ajout)
/* USER CODE END PV */
```

On copie la fonction de redirection de l'UART pour le printf entre les balises `USER CODE BEGIN 0` et `USER CODE END 0` : 

```c
/* USER CODE BEGIN 0 */
// Fonction pour rediriger printf vers UART
int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart2, (uint8_t*) &ch, 1, HAL_MAX_DELAY);
    return ch;
}
/* USER CODE END 0 */
```

On initialise le module ADS1115 en copiant ces lignes entre les balises `USER CODE BEGIN 2` et `USER CODE END 2` :  

```c
/* USER CODE BEGIN 2 */
HAL_Delay(500); // Petit délai pour laisser le temps à tout de se stabiliser
printf("Programme de test ADS1115 - Mode Single-End (AIN0 & AIN1)\r\n");

// 1. Initialiser la bibliothèque ADS1115
ADS1115_init(&hi2c1);                                   // Passer le handle I2C à la bibliothèque
printf("Bibliothèque ADS1115 initialisée.\r\n");     

// 2. Ajouter le module ADS1115
module_index = ADS1115_addModule(ADS1115_ADDRESS);      // Ajouter le module avec l'adresse I2C
if (module_index < 0) {
    printf("Erreur: Impossible d'ajouter le module ADS1115 à l'adresse 0x%02X.\r\n", ADS1115_ADDRESS);
    Error_Handler();                                    // Gérer l'erreur si le module ne peut pas être ajouté
}
printf("Module ADS1115 ajouté avec l'index %d (Adresse 0x%02X).\r\n", module_index, ADS1115_ADDRESS);

// 3. Sélectionner le module (nécessaire même s'il n'y en a qu'un)
if (!ADS1115_selectModule(module_index)) {              // Sélectionner le module pour les opérations suivantes
        printf("Erreur: Impossible de sélectionner le module %d.\r\n", module_index);
        Error_Handler();                                   // Gérer l'erreur si la sélection échoue
}
printf("Module %d sélectionné.\r\n", module_index);

// 4. Vérifier la communication avec le module
if (!ADS1115_begin()) {                                 // Vérifier si le module est réactif
    printf("Erreur: Échec de la communication avec le module %d (adresse 0x%02X).\r\n", module_index, ADS1115_getSelectedModuleAddress() >> 1);
    Error_Handler();                                    // Gérer l'erreur si la communication échoue
} else {
    printf("Communication avec le module %d (adresse 0x%02X) réussie.\r\n", module_index, ADS1115_getSelectedModuleAddress() >> 1);
}

// 5. Configurer le gain (Optionnel, mais recommandé, par défaut GAIN_TWOTHIRDS pour +/- 6.144V)
// Le gain détermine la plage de tension d'entrée. Choisir en fonction de l'application.
// Exemple: GAIN_ONE pour +/- 4.096V, GAIN_TWO pour +/- 2.048V, 
// GAIN_FOUR pour +/- 1.024V, GAIN_EIGHT pour +/- 0.512V, GAIN_SIXTEEN pour +/- 0.256V
ADS1115_setGain(GAIN_ONE); // Exemple: Gain 1x (+/- 4.096V)
printf("Gain configuré à: +/- 4.096V (1x)\r\n");

// 6. Configurer le taux d'échantillonnage (Optionnel)
// Le taux d'échantillonnage détermine la fréquence d'échantillonnage.
// Choisir en fonction de l'application.
// Options SPS (Temps de conversion):
//   8 SPS   (125ms)
//   16 SPS  (62.5ms)
//   32 SPS  (31.25ms)
//   64 SPS  (15.625ms)
//   128 SPS (7.81ms) - Défaut
//   250 SPS (4ms)
//   475 SPS (2.11ms)
//   860 SPS (1.16ms)
// ADS1115_setDataRate(RATE_ADS1115_250SPS); // Configuration du taux à 250 SPS (4ms)
printf("Fréquence d'échantillonnage par défaut (128 SPS) utilisée.\r\n");

printf("-----------------------------------------------------------\n");
printf("Début des lectures en mode Single-Shot (AIN0 & AIN1)...\r\n");
/* USER CODE END 2 */
```

On définie la boucle principale en copiant ces lignes entre les balises `USER CODE BEGIN WHILE` et `USER CODE END WHILE` : 

```c
/* USER CODE BEGIN WHILE */
while (1)
{
    // Déclarer les variables pour AIN0 et AIN1 (raw, volts, millivolts)
    int16_t adc0, adc1;
    float volts0 = 0.0f, volts1 = 0.0f;
    float mVolts0 = 0.0f, mVolts1 = 0.0f; // Ajout des variables pour mV

    // --- Lecture Single-Ended AIN0 ---
    adc0 = ADS1115_readADC_SingleEnded(0);          // Lire AIN0
    if (adc0 != INT16_MIN) {                        // Vérifier si la lecture a réussi
        volts0 = ADS1115_computeVolts(adc0);        // Calculer la tension en volts
        mVolts0 = ADS1115_computeMilliVolts(adc0);  // Calculer la tension en millivolts
        // Afficher les résultats de la lecture AIN0
        printf("AIN0: Raw = %6d | Volts = %8.4f V | mVolts = %8.2f mV\r\n", adc0, volts0, mVolts0);
    } else {
        printf("AIN0: Erreur de lecture (code %d)\r\n", adc0);
    }

    // --- Lecture Single-Ended AIN1 ---
    // Petite pause si nécessaire entre les lectures
    HAL_Delay(10); // Optionnel: délai pour éviter de surcharger le bus trop vite
    adc1 = ADS1115_readADC_SingleEnded(1);          // Lire AIN1
    if (adc1 != INT16_MIN) {                        // Vérifier si la lecture a réussi
        volts1 = ADS1115_computeVolts(adc1);        // Calculer la tension en volts
        mVolts1 = ADS1115_computeMilliVolts(adc1);  // Calculer la tension en millivolts
        // Afficher les résultats de la lecture AIN1
        printf("AIN1: Raw = %6d | Volts = %8.4f V | mVolts = %8.2f mV\r\n", adc1, volts1, mVolts1);
    } else {
        printf("AIN1: Erreur de lecture (code %d)\r\n", adc1);
    }

    printf("-----------------------------------------------------------\n");
    // Attendre avant la prochaine série de lectures
    HAL_Delay(1000); // Attendre 1 seconde

    /* USER CODE END WHILE */

### 2. Mode Continu

### 3. Mode Comparateur

### 4. Mode Différentiel

