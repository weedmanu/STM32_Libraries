# Bibliothèque STM32 HAL pour Écran LCD Alphanumérique (I2C)

Cette bibliothèque permet de contrôler des écrans LCD alphanumériques compatibles HD44780 via un expandeur I2C (PCF8574) sur des microcontrôleurs STM32 en utilisant la couche d'abstraction matérielle (HAL).

## Fonctionnalités

* Initialisation de l'écran LCD via I2C.
* Écriture de chaînes de caractères et de caractères ASCII.
* Gestion du curseur : positionnement, affichage, clignotement.
* Effacement de l'écran et retour à la position d'origine.
* Création et affichage de caractères personnalisés.
* Défilement de texte vers la gauche ou la droite.
* Contrôle du rétroéclairage.

## Connexions Matérielles

Connectez le module LCD à votre microcontrôleur STM32 via un expandeur I2C (PCF8574) :

* **VCC** -> 3.3V ou 5V (selon votre module).
* **GND** -> GND.
* **SCL** -> Broche SCL I2C du STM32 (ex: PB6, PB8, PB10 selon le MCU et la configuration I2C).
* **SDA** -> Broche SDA I2C du STM32 (ex: PB7, PB9, PB11 selon le MCU et la configuration I2C).

Assurez-vous d'activer le périphérique I2C approprié dans votre configuration STM32CubeMX ou manuellement. Des résistances de pull-up (généralement 4.7kΩ) sont nécessaires sur les lignes SCL et SDA si elles ne sont pas déjà présentes sur le module ou la carte STM32.

## Utilisation de base

Voici un exemple simple d'utilisation dans votre fichier `main.c` :

### 1. Inclure l'en-tête

```c
/* USER CODE BEGIN Includes */
#include <stdio.h>              // Pour utiliser printf pour le débogage
#include <string.h>             // Pour manipuler des chaînes de caractères
#include "STM32_I2C_LCD.h"      // Librairie pour contrôler l'écran LCD via I2C
/* USER CODE END Includes */
```

### 2. Déclarer les constantes préprocesseur

```c
/* USER CODE BEGIN PD */
#define COLUMNS     20       // Nombre de colonnes du LCD
#define ROWS        4        // Nombre de lignes du LCD
#define I2C_ADDRESS 0x3F     // Adresse I2C du LCD
/* USER CODE END PD */
```

### 3 Redirection `printf` pour l'affichage et fonction de démonstration pour l'écran

```c
/* USER CODE BEGIN 0 */
/**
 * @brief  Cette fonction est utilisée pour rediriger la sortie standard (printf) vers l'UART.
 * @param  ch: Caractère à transmettre.
 * @retval Le caractère transmis.
 */
int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart2, (uint8_t*) &ch, 1, 0xFFFF); // Envoi du caractère via UART
    return ch;
}

/**
 * @brief  Fonction de démonstration pour le LCD.
 * @note   Cette fonction teste diverses fonctionnalités du LCD, y compris l'effacement,
 *         l'écriture de texte, le contrôle du rétroéclairage, et plus encore.
 */
void lcd_demo(void) {
    printf("--- Starting LCD Demonstration ---\r\n");
    printf("LCD Config: %dx%d @ I2C Address 0x%02X\r\n", COLUMNS, ROWS, I2C_ADDRESS);
    char line_buffer[COLUMNS + 1];      // Buffer pour construire les lignes
    lcd_clear();                        // Efface l'écran LCD
    snprintf(line_buffer, sizeof(line_buffer), "LCD Config: %dx%d", COLUMNS, ROWS); // Construit la configuration
    lcd_write_string(line_buffer);     // Écrit la configuration sur la première ligne du LCD
    lcd_set_cursor(1, 0);              // Positionne le curseur sur la deuxième ligne
    snprintf(line_buffer, sizeof(line_buffer), "Address 0x%02X", I2C_ADDRESS);
    lcd_write_string(line_buffer);     // Écrit l'adresse I2C sur la deuxième ligne
    HAL_Delay(2000);                   // Pause de 2 secondes

    // 1. Effacer l'écran
    printf("Demo: lcd_clear()...\r\n");
    lcd_clear();                        // Efface l'écran
    lcd_write_string("Demo: lcd_clear"); // Écrit un message de démonstration
    HAL_Delay(2000);                    // Pause de 2 secondes
    lcd_clear();                        // Efface à nouveau l'écran
    HAL_Delay(2000);                    // Pause de 2 secondes

    // 2. Test d'écriture sur plusieurs lignes
    printf("Demo: Multi-line text writing...\r\n");
    lcd_clear();                        // Efface l'écran
    lcd_write_string("Demo: Lines");    // Écrit un message sur la première ligne
    HAL_Delay(2000);                    // Pause de 2 secondes

    // Ligne 0
    lcd_clear();                        // Efface l'écran et positionne le curseur sur la première ligne
    snprintf(line_buffer, sizeof(line_buffer), "L0 Demo: Lines"); // Construit le message
    lcd_write_string(line_buffer);     // Écrit un message sur la première ligne
    HAL_Delay(1000);                   // Pause de 1 seconde

    // Ligne 1
    if (ROWS > 1) {                    // Vérifie si l'écran a au moins 2 lignes
        lcd_set_cursor(1, 0);          // Positionne le curseur sur la deuxième ligne
        snprintf(line_buffer, sizeof(line_buffer), "L1"); // Construit le message
        lcd_write_string(line_buffer); // Écrit "L1" sur la deuxième ligne
        HAL_Delay(1000);               // Pause de 1 seconde
    }

    // Ligne 2
    if (ROWS > 2) {                    // Vérifie si l'écran a au moins 3 lignes
        lcd_set_cursor(2, 0);          // Positionne le curseur sur la troisième ligne
        snprintf(line_buffer, sizeof(line_buffer), "L2"); // Construit le message
        lcd_write_string(line_buffer); // Écrit "L2" sur la troisième ligne
        HAL_Delay(1000);               // Pause de 1 seconde
    }

    // Ligne 3
    if (ROWS > 3) {                    // Vérifie si l'écran a au moins 4 lignes
        lcd_set_cursor(3, 0);          // Positionne le curseur sur la quatrième ligne
        snprintf(line_buffer, sizeof(line_buffer), "L3"); // Construit le message
        lcd_write_string(line_buffer); // Écrit "L3" sur la quatrième ligne
        HAL_Delay(1000);               // Pause de 1 seconde
    }
    HAL_Delay(2000);                   // Pause de 2 secondes

    // 3. Test du rétroéclairage
    printf("Demo: lcd_backlight()...\r\n");
    lcd_clear();                        // Efface l'écran
    lcd_write_string("Demo: Backlight"); // Écrit un message de démonstration
    HAL_Delay(2000);                    // Pause de 2 secondes
    lcd_backlight(0);                   // Éteint le rétroéclairage
    HAL_Delay(1000);                    // Pause de 1 seconde
    lcd_backlight(1);                   // Allume le rétroéclairage
    HAL_Delay(1000);                    // Pause de 1 seconde

    // 4. Test Display On/Off
    printf("Demo: lcd_display_off() / lcd_display_on()...\r\n");
    lcd_clear();                        // Efface l'écran
    lcd_write_string("display on/off"); // Écrit un message de démonstration
    HAL_Delay(2000);                    // Pause de 2 secondes
    lcd_display_off();                  // Éteint l'affichage
    HAL_Delay(1000);                    // Pause de 1 seconde
    lcd_display_on();                   // Rallume l'affichage
    HAL_Delay(1000);                    // Pause de 1 seconde

    // 5. Test du curseur
    printf("Demo: lcd_cursor_on() / lcd_cursor_off()...\r\n");
    lcd_clear();                        // Efface l'écran
    lcd_write_string("cursor on/off");  // Écrit un message de démonstration
    lcd_set_cursor(1, 5);               // Positionne le curseur sur la deuxième ligne, colonne 5
    HAL_Delay(2000);                    // Pause de 2 secondes
    lcd_cursor_on();                    // Active le curseur
    HAL_Delay(1500);                    // Pause de 1,5 seconde
    lcd_cursor_off();                   // Désactive le curseur
    HAL_Delay(500);                     // Pause de 0,5 seconde

    // 6. Test des caractères personnalisés
    printf("Demo: Custom Characters...\r\n");
    lcd_clear();                        // Efface l'écran
    lcd_write_string("Custom Chars");   // Écrit un message de démonstration
    uint8_t smiley[8] = {0x00, 0x0A, 0x00, 0x04, 0x11, 0x0E, 0x00, 0x00}; // Définition d'un smiley
    lcd_create_char(0, smiley);         // Crée un caractère personnalisé à l'emplacement 0
    lcd_set_cursor(1, 5);               // Positionne le curseur pour afficher le smiley
    lcd_put_custom_char(0);             // Affiche le smiley
    HAL_Delay(2000);                    // Pause de 2 secondes

    // 7. Test de défilement à gauche
    printf("Demo: Scrolling...\r\n");
    lcd_clear();                        // Efface l'écran
    lcd_write_string("Scroll Left <<<");// Écrit un message pour le défilement
    for (int i = 0; i < COLUMNS; i++) { // Boucle pour défilement à gauche
        lcd_scroll_display_left();      // Défile l'affichage vers la gauche
        HAL_Delay(500);                 // Pause de 0,5 seconde
    }
    HAL_Delay(2000);                    // Pause de 2 secondes

    // 8. Test de défilement à droite
    printf("Demo: Scrolling...\r\n");
    lcd_clear();                        // Efface l'écran
    lcd_write_string("Scroll right >>>");// Écrit un message pour le défilement
    for (int i = 0; i < COLUMNS; i++) { // Boucle pour défilement à droite
        lcd_scroll_display_right();     // Défile l'affichage vers la droite
        HAL_Delay(500);                 // Pause de 0,5 seconde
    }
    HAL_Delay(2000);                    // Pause de 2 secondes

    // Message final
    printf("--- LCD Demonstration Finished ---\r\n");
    lcd_clear();                        // Efface l'écran
    lcd_write_string("Demo Finished!"); // Écrit un message final
    HAL_Delay(3000);                    // Pause de 3 secondes
}
/* USER CODE END 0 */
```

### 4. Initialiser l'écran LCD et lancer la fonction de demonstration

```c
  /* USER CODE BEGIN 2 */
	HAL_Delay(250);                         // Délai pour les initialisations
	printf("Test LIB LCD\n\r");                       // Écriture dans la console série

	// Initialisation de l'écran LCD
	HAL_StatusTypeDef lcd_status = lcd_init(&hi2c1, COLUMNS, ROWS, I2C_ADDRESS); // Initialisation de l'écran

	if (lcd_status != HAL_OK) {
		printf("LCD Init Failed! Status: %d\r\n", lcd_status);
		Error_Handler(); // Bloquer si l'initialisation du LCD échoue
	} else {
		printf("LCD Init OK!\r\n");
		lcd_backlight(1); // Allumer le rétroéclairage (ne retourne pas de statut, s'applique à la prochaine commande)

		// Exécuter la séquence de démonstration simplifiée
		lcd_demo(); // Appel de la fonction de démonstration LCD
	}
  /* USER CODE END 2 */
```


## Référence API

### Structures Principales

* `LCD_Config`: Structure interne contenant la configuration de l'écran LCD (adresse I2C, colonnes, lignes).

### Fonctions Principales

* `HAL_StatusTypeDef lcd_init(I2C_HandleTypeDef *hi2c, uint8_t columns, uint8_t rows, uint8_t i2c_address)`
    * Initialise l'écran LCD avec les paramètres spécifiés.
    * Retourne `HAL_OK` en cas de succès, ou un code d'erreur HAL.

* `HAL_StatusTypeDef lcd_write_string(char *str)`
    * Écrit une chaîne de caractères sur l'écran LCD à partir de la position actuelle du curseur.

* `HAL_StatusTypeDef lcd_write_char(char ascii_char)`
    * Écrit un caractère ASCII sur l'écran LCD à la position actuelle du curseur.

* `HAL_StatusTypeDef lcd_set_cursor(uint8_t row, uint8_t column)`
    * Positionne le curseur sur l'écran LCD.

* `HAL_StatusTypeDef lcd_clear(void)`
    * Efface l'affichage du LCD et replace le curseur en (0,0).

* `HAL_StatusTypeDef lcd_home(void)`
    * Replace le curseur en position (0,0) sans effacer l'écran.

* `void lcd_backlight(uint8_t state)`
    * Contrôle l'état du rétroéclairage (1 pour allumé, 0 pour éteint).

* `HAL_StatusTypeDef lcd_create_char(uint8_t location, uint8_t charmap[8])`
    * Crée un caractère personnalisé dans la CGRAM.

* `HAL_StatusTypeDef lcd_put_custom_char(uint8_t location)`
    * Affiche un caractère personnalisé préalablement créé.

* `HAL_StatusTypeDef scroll_text_left(char *text, uint8_t row, uint8_t columns, uint16_t delay_ms)`
    * Fait défiler un texte vers la gauche sur une ligne spécifiée.

* `HAL_StatusTypeDef scroll_text_right(char *text, uint8_t row, uint8_t columns, uint16_t delay_ms)`
    * Fait défiler un texte vers la droite sur une ligne spécifiée.

* `HAL_StatusTypeDef lcd_scroll_display_left(void)`
    * Décale tout l'affichage d'une colonne vers la gauche (commande native).

* `HAL_StatusTypeDef lcd_scroll_display_right(void)`
    * Décale tout l'affichage d'une colonne vers la droite (commande native).


### Codes d'Erreur (HAL_StatusTypeDef)

* **HAL_OK** : Opération réussie.
* **HAL_ERROR** : Erreur générale.
* **HAL_BUSY** : Le périphérique I2C est occupé.
* **HAL_TIMEOUT** : Timeout lors de la communication I2C.


