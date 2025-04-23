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
*   **ADDR** -> Connectez à GND, VCC, SDA ou SCL pour sélectionner l'adresse I2C (voir datasheet ADS1115). L'adresse par défaut est souvent 0x48 (ADDR à GND).
*   **ALERT/RDY** -> Connectez à une broche GPIO du STM32 configurée en entrée (ex: PA10, voir `main.c` et `MX_GPIO_Init`) **si vous utilisez le mode comparateur ou si vous voulez détecter la fin de conversion matériellement**. Une résistance de pull-up externe ou l'activation du pull-up interne du STM32 est souvent nécessaire si la polarité est Active Low.

## Configuration Logicielle

### 1. Fichiers de la Bibliothèque

*   Copiez `STM32_ADS1115.c` dans le dossier `Core/Src` de votre projet STM32CubeIDE.
*   Copiez `STM32_ADS1115.h` dans le dossier `Core/Inc` de votre projet STM32CubeIDE.

### 2. Dépendances

*   Cette bibliothèque nécessite la bibliothèque STM32 HAL. Assurez-vous que les modules HAL suivants sont activés dans votre `stm32l4xx_hal_conf.h` (ou équivalent) :
    *   `HAL_MODULE_ENABLED`
    *   `HAL_I2C_MODULE_ENABLED`
    *   `HAL_GPIO_MODULE_ENABLED`
    *   `HAL_RCC_MODULE_ENABLED`
    *   `HAL_PWR_MODULE_ENABLED`
    *   `HAL_CORTEX_MODULE_ENABLED`
    *   (Optionnel, pour `HAL_Delay`) `HAL_TIM_MODULE_ENABLED` (généralement via SysTick)
    *   (Optionnel, pour `printf`) `HAL_UART_MODULE_ENABLED`

### 3. Configuration STM32CubeMX

*   **I2C :** Activez le périphérique I2C que vous utilisez (ex: I2C1).
    *   Configurez-le en mode `I2C`.
    *   Assurez-vous que la vitesse (Timing) est correctement configurée pour votre fréquence d'horloge (ex: 100kHz ou 400kHz). Le timing `0x10D19CE4` est un exemple pour L4 à 80MHz et I2C à 100kHz, adaptez-le si nécessaire.
*   **GPIO (pour ALERT/RDY) :** Si vous utilisez le mode comparateur :
    *   Configurez la broche GPIO connectée à ALERT/RDY (ex: PA10) en mode `GPIO_Input`.
    *   Configurez le `Pull-up/Pull-down` :
        *   Si `COMPARATOR_POLARITY` est `ADS1115_REG_CONFIG_CPOL_ACTVLOW` (Active Low), activez le `Pull-up`.
        *   Si `COMPARATOR_POLARITY` est `ADS1115_REG_CONFIG_CPOL_ACTVHI` (Active High), configurez en `No pull-up and no pull-down`.
*   **UART (pour les exemples) :** Si vous utilisez `printf` pour le débogage (comme dans les exemples `main.c`), activez un périphérique UART (ex: USART2) en mode Asynchrone.

### 4. Intégration dans `main.c`

*   Incluez l'en-tête : `#include "STM32_ADS1115.h"`
*   Redirigez `printf` vers l'UART si nécessaire (voir exemples `main.c`).

## Utilisation (API)

### 1. Initialisation

```c
#include "STM32_ADS1115.h"
#include <stdio.h> // Pour printf

// ... autres includes et variables globales ...
extern I2C_HandleTypeDef hi2c1; // Handle I2C défini par CubeMX
extern UART_HandleTypeDef huart2; // Handle UART défini par CubeMX (pour printf)

// Fonction pour printf (si utilisée)
int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart2, (uint8_t*)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

int main(void) {
    // ... Initialisation HAL, Clock, Périphériques (généré par CubeMX) ...
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART2_UART_Init(); // Si printf est utilisé

    int module_index = -1;
    uint8_t ads_address = 0x48; // Adresse 7 bits de votre module

    // Initialiser la bibliothèque avec le handle I2C
    ADS1115_init(&hi2c1);
    printf("Bibliothèque ADS1115 initialisée.\r\n");

    // Ajouter un module ADS1115
    module_index = ADS1115_addModule(ads_address);
    if (module_index < 0) {
        printf("Erreur ajout module ADS1115 @ 0x%02X\r\n", ads_address);
        Error_Handler();
    }
    printf("Module ADS1115 ajouté (index %d).\r\n", module_index);

    // Sélectionner le module pour les opérations suivantes (obligatoire)
    if (!ADS1115_selectModule(module_index)) {
         printf("Erreur sélection module %d.\r\n", module_index);
         Error_Handler();
    }
    printf("Module %d sélectionné.\r\n", module_index);

    // Vérifier la communication avec le module sélectionné
    if (!ADS1115_begin()) {
        printf("Erreur communication module %d.\r\n", module_index);
        Error_Handler();
    }
    printf("Communication module %d OK.\r\n", module_index);

    // ... Reste du code ...
}
