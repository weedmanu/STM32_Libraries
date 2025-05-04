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
#include <stdio.h>      	  // Inclut la bibliothèque standard d'entrée-sortie pour utiliser printf
#include <string.h>     	  // Inclut la bibliothèque de manipulation de chaînes de caractères pour utiliser strlen
#include "STM32_I2C_LCD.h" 	// Inclut le fichier d'en-tête pour la gestion de l'écran LCD via I2C
/* USER CODE END Includes */
```

### 2. Déclarer les constantes préprocesseur

```c
/* USER CODE BEGIN PD */
#define COLUMNS     20       // Nombre de colonnes du LCD
#define ROWS        4        // Nombre de lignes du LCD
#define I2C_ADDRESS 0x3F     // Adresse I2C du LCD
// Emplacements caractères personnalisés 
#define CUSTOM_CHAR_HEART     0
#define CUSTOM_CHAR_DEGREE    1
#define CUSTOM_CHAR_ARROWDOWN 2
#define CUSTOM_CHAR_ARROWUP   3
#define CUSTOM_CHAR_MAN       4
/* USER CODE END PD */
```

### 3. Déclarer des variables globales

```c
/* USER CODE BEGIN PV */
uint8_t mode = 0; // Variable pour suivre le mode d'affichage actuel
// Définition des caractères personnalisés : https://maxpromer.github.io/LCD-Character-Creator/
uint8_t HeartChar[8] = {0x00, 0x0a, 0x15, 0x11, 0x0a, 0x04, 0x00, 0x00}; 		// Tableau représentant un caractère personnalisé "cœur"
uint8_t DegreeChar[8] = {0x07, 0x05, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00}; 		// Tableau représentant un caractère personnalisé "degré"
uint8_t ArrowDownChar[8] = {0x00, 0x04, 0x04, 0x04, 0x04, 0x1F, 0x0E, 0x04}; 	// Tableau représentant un caractère personnalisé "flèche vers le bas"
uint8_t ArrowUpChar[8] = {0x04, 0x0E, 0x1F, 0x04, 0x04, 0x04, 0x04, 0x00}; 		// Tableau représentant un caractère personnalisé "flèche vers le haut"
uint8_t ManChar[8] = {0x1F, 0x15, 0x1F, 0x11, 0x1F, 0x0A, 0x0A, 0x1B}; 			// Tableau représentant un caractère personnalisé "homme"
/* USER CODE END PV */
```

### 4. Redirection `printf` pour l'affichage et fonctions de démonstration pour l'écran

```c
/* USER CODE BEGIN 0 */
// Fonction qui transmet un caractère via UART et le renvoie.Utilisé pour la sortie standard (printf).
int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart2, (uint8_t*) &ch, 1, 0xFFFF); // Pour Envoyer le caractère via UART
    // ITM_SendChar(ch);                 // Option alternative pour envoyer le caractère via ITM
    return ch;
}

/**
 * @brief  Fonction pour initialiser les caractères personnalisés
 * @param  None
 * @retval None
 */
void initialize_custom_chars() {
	HAL_StatusTypeDef status;
	status = lcd_create_char(CUSTOM_CHAR_HEART, HeartChar);      // Créer un caractère personnalisé "cœur"
	if(status != HAL_OK) printf("Error creating char 0\r\n");
	status = lcd_create_char(CUSTOM_CHAR_DEGREE, DegreeChar);     // Créer un caractère personnalisé "degré"
	if(status != HAL_OK) printf("Error creating char 1\r\n");
	status = lcd_create_char(CUSTOM_CHAR_ARROWDOWN, ArrowDownChar);  // Créer un caractère personnalisé "flèche vers le bas"
	if(status != HAL_OK) printf("Error creating char 2\r\n");
	status = lcd_create_char(CUSTOM_CHAR_ARROWUP, ArrowUpChar);    // Créer un caractère personnalisé "flèche vers le haut"
	if(status != HAL_OK) printf("Error creating char 3\r\n");
	status = lcd_create_char(CUSTOM_CHAR_MAN, ManChar);        // Créer un caractère personnalisé "homme"
	if(status != HAL_OK) printf("Error creating char 4\r\n");
}

/**
 * @brief  Fonction pour afficher les caractères personnalisés sur l'écran LCD
 * @param  columns: Nombre de colonnes de l'écran LCD
 * @retval None
 */
void display_custom_chars(uint8_t columns) {
	HAL_StatusTypeDef status;

	// Note: lcd_clear() place déjà le curseur en (0,0), donc lcd_set_cursor(0,0) n'est pas nécessaire ici
	// si cette fonction est appelée après un clear. On suppose que c'est le cas.
	status = lcd_write_string("Custom Chars: "); // Écrire le texte "Custom Chars: "
	if(status != HAL_OK) { printf("Error write_string in custom_chars\r\n"); return; }

	if (columns == 16) { 		// Configuration pour un écran 16x2
		status = lcd_set_cursor(1, 0); 		// Placer le curseur à la ligne 1, colonne 0
		if(status == HAL_OK) status = lcd_put_custom_char(CUSTOM_CHAR_MAN); 	// Cœur

		if(status == HAL_OK) status = lcd_set_cursor(1, 2); 		// Placer le curseur à la ligne 1, colonne 2
		if(status == HAL_OK) status = lcd_put_custom_char(CUSTOM_CHAR_DEGREE); 	// Degré

		if(status == HAL_OK) status = lcd_set_cursor(1, 4); 		// Placer le curseur à la ligne 1, colonne 4
		if(status == HAL_OK) status = lcd_put_custom_char(CUSTOM_CHAR_ARROWDOWN); 	// Flèche vers le bas

		if(status == HAL_OK) status = lcd_set_cursor(1, 6); 		// Placer le curseur à la ligne 1, colonne 6
		if(status == HAL_OK) status = lcd_put_custom_char(CUSTOM_CHAR_ARROWUP); 	// Flèche vers le haut

		if(status == HAL_OK) status = lcd_set_cursor(1, 8); 		// Placer le curseur à la ligne 1, colonne 8
		if(status == HAL_OK) status = lcd_put_custom_char(CUSTOM_CHAR_MAN); 	// Homme
	} else if (columns == 20) { // Configuration pour un écran 20x4
		status = lcd_set_cursor(2, 0);		// Placer le curseur à la ligne 2, colonne 0
		if(status == HAL_OK) status = lcd_put_custom_char(CUSTOM_CHAR_MAN);     // Cœur

		if(status == HAL_OK) status = lcd_set_cursor(2, 2);       // Placer le curseur à la ligne 2, colonne 2
		if(status == HAL_OK) status = lcd_put_custom_char(CUSTOM_CHAR_DEGREE);		// Degré

		if(status == HAL_OK) status = lcd_set_cursor(2, 4);		// Placer le curseur à la ligne 2, colonne 4
		if(status == HAL_OK) status = lcd_put_custom_char(CUSTOM_CHAR_ARROWDOWN);		// Flèche vers le bas

		if(status == HAL_OK) status = lcd_set_cursor(2, 6);		// Placer le curseur à la ligne 2, colonne 6
		if(status == HAL_OK) status = lcd_put_custom_char(CUSTOM_CHAR_ARROWUP);		// Flèche vers le haut

		if(status == HAL_OK) status = lcd_set_cursor(2, 8);		// Placer le curseur à la ligne 2, colonne 8
		if(status == HAL_OK) status = lcd_put_custom_char(CUSTOM_CHAR_MAN);		// Homme
	}

	if(status != HAL_OK) {
		printf("Error displaying custom char\r\n");
	}
	HAL_Delay(SHORT_PAUSE_MS); 				// Attendre
}

/**
 * @brief  Fonction pour afficher les caractères ASCII sur l'écran LCD
 * @param  columns: Nombre de colonnes de l'écran LCD
 * @param  rows: Nombre de lignes de l'écran LCD
 * @retval None
 */
void display_ascii(uint8_t columns, uint8_t rows) {
	HAL_StatusTypeDef status;
	uint8_t start_char = 32; // Commencer à l'espace (caractère 32)
	uint8_t end_char = 126;  // Finir au tilde (caractère 126)
	uint8_t current_char = start_char; // Caractère ASCII actuel

	lcd_clear(); // Effacer avant d'afficher les caractères ASCII (lcd_clear ne retourne pas de statut ici)
	// Note: lcd_clear() place déjà le curseur en (0,0), donc lcd_set_cursor(0,0) n'est pas nécessaire ici
	status = lcd_write_string("Ascii Chars:"); // Écrire le texte "Ascii Chars:"
	if(status != HAL_OK) { printf("Error write_string in display_ascii\r\n"); return; }

	HAL_Delay(SHORT_PAUSE_MS); // Attendre avant d'afficher la page suivante

	while(current_char <= end_char) {
		lcd_clear(); // Effacer pour la nouvelle page
		for (uint8_t i = 0; i < rows; i++) { // Boucle pour chaque ligne
			status = lcd_set_cursor(i, 0);
			if(status != HAL_OK) { printf("Error set_cursor(%d,0) in display_ascii\r\n", i); return; }

			for (uint8_t j = 0; j < columns; j++) { // Boucle pour chaque colonne
				if (current_char <= end_char) {
					// Utiliser lcd_write_char qui gère l'envoi de données internes
					status = lcd_write_char(current_char);
					if(status != HAL_OK) { printf("Error write_char(%c) in display_ascii\r\n", current_char); return; }
					current_char++;
				} else {
					// Remplir avec des espaces si on a dépassé le dernier caractère
					status = lcd_write_char(' ');
					if(status != HAL_OK) { printf("Error writing space in display_ascii\r\n"); return; }
				}
			}
		}
		HAL_Delay(SHORT_PAUSE_MS); // Attendre avant d'afficher la page suivante
		if (current_char > end_char) break; // Sortir si tous les caractères ont été affichés (sécurité)
	}
}
/* USER CODE END 0 */
```

### 5. Initialiser l'écran LCD

```c
/* USER CODE BEGIN 2 */
	HAL_Delay(INIT_DELAY_MS);                         // Délai pour les initialisations
	printf("Test LIB LCD\n\r");                       // Écriture dans la console série

	// Initialisation de l'écran LCD
	HAL_StatusTypeDef lcd_status = lcd_init(&hi2c1, COLUMNS, ROWS, I2C_ADDRESS); // Initialisation de l'écran

	if (lcd_status != HAL_OK) {
		printf("LCD Init Failed! Status: %d\r\n", lcd_status);
		Error_Handler(); // Bloquer si l'initialisation échoue
	} else {
		printf("LCD Init OK!\r\n");
		lcd_clear();                                      // Effacer l'écran (ne retourne pas de statut)
		lcd_backlight(1);                                 // Allumer le rétroéclairage (ne retourne pas de statut)

		// Initialisation des caractères personnalisés
		initialize_custom_chars();                        // Appel de la fonction pour initialiser les caractères personnalisés (gère ses propres erreurs)

		// Affichage de texte sur l'écran
		// Note: lcd_clear() place déjà le curseur en (0,0), donc lcd_set_cursor(0,0) n'est pas nécessaire ici
		lcd_status = lcd_write_string("Test LIB LCD"); // Écrire sur l'écran (commence en 0,0 après clear)
		if(lcd_status == HAL_OK) lcd_status = lcd_set_cursor(1, 0);    // Placer le curseur ligne 1 colonne 0
		if(lcd_status == HAL_OK) lcd_status = lcd_write_string("Init OK");      // Écrire sur l'écran

		if(lcd_status != HAL_OK) printf("Error writing initial text\r\n");
		HAL_Delay(SHORT_PAUSE_MS);                       // Attendre
	}
	 uint32_t mode_timer_start = HAL_GetTick();
	/* USER CODE END 2 */
    ```

    ### 6. Boucle principale

    ```c
    /* USER CODE BEGIN WHILE */
	while (1) {
		// Vérifier si le temps pour le mode actuel est écoulé
    if (HAL_GetTick() - mode_timer_start >= LONG_PAUSE_MS) {
      if (mode > 5) { mode = 0; }
      mode_timer_start = HAL_GetTick(); // Redémarrer le timer pour le nouveau mode

      // Effacer l'écran et afficher le titre du nouveau mode (déplacer le début du switch ici)
      lcd_status = lcd_clear(); // Maintenant retourne un statut
      if (lcd_status != HAL_OK) {
          printf("Error clearing LCD at start of mode %d!\r\n", mode);
          // Peut-être gérer l'erreur ici, ex: Error_Handler() ou tentative de réinitialisation
      }
      HAL_Delay(CLEAR_DELAY_MS); // Garder un petit délai après clear si nécessaire
      printf("Mode %d: ...\r\n", mode); // Afficher le titre
	  switch (mode) { // Sélectionne le mode d'affichage
		case 0:
			printf("Mode 0: Display ASCII\r\n");
			display_ascii(COLUMNS, ROWS); // Affiche les caractères ASCII
			break;
		case 1:
			printf("Mode 1: Display Custom Chars\r\n");
			display_custom_chars(COLUMNS); // Affiche les caractères personnalisés
			break;
		case 2:
			printf("Mode 2: Cursor Demo\r\n");
			// Note: lcd_clear() place déjà le curseur en (0,0)
			lcd_status = lcd_write_string("Cursor ON");
			if(lcd_status == HAL_OK) lcd_status = lcd_cursor_on(); // Active le curseur
			if(lcd_status != HAL_OK) printf("Error cursor on\r\n");
			HAL_Delay(SHORT_PAUSE_MS);
			if(lcd_status == HAL_OK) lcd_status = lcd_set_cursor(1, 0);
			if(lcd_status == HAL_OK) lcd_status = lcd_write_string("Cursor OFF");
			if(lcd_status == HAL_OK) lcd_status = lcd_cursor_off(); // Désactive le curseur
			if(lcd_status != HAL_OK) printf("Error cursor off\r\n");
			HAL_Delay(SHORT_PAUSE_MS);
			break;
		case 3:
		    printf("Mode 3: Blink Demo\r\n");
		    // Note: lcd_clear() place déjà le curseur en (0,0)
		    lcd_status = lcd_write_string("Blink ON");
		    if(lcd_status == HAL_OK) lcd_status = lcd_cursor_on();  // Assure que le curseur est visible
		    if(lcd_status == HAL_OK) lcd_status = lcd_blink_on();   // Active le clignotement
		    if(lcd_status != HAL_OK) printf("Error blink on\r\n");
		    HAL_Delay(LONG_PAUSE_MS); // Laisse le temps de voir clignoter
		    if(lcd_status == HAL_OK) lcd_status = lcd_set_cursor(1, 0);
		    if(lcd_status == HAL_OK) lcd_status = lcd_write_string("Blink OFF");
		    if(lcd_status == HAL_OK) lcd_status = lcd_blink_off();  // Désactive le clignotement
		    if(lcd_status != HAL_OK) printf("Error blink off\r\n");
		    HAL_Delay(SHORT_PAUSE_MS);
		    if(lcd_status == HAL_OK) lcd_status = lcd_cursor_off(); // Optionnel: éteint le curseur à la fin
		    break;
		case 4:
			printf("Mode 4: Display ON/OFF Demo\r\n");
			// Note: lcd_clear() place déjà le curseur en (0,0)
			lcd_status = lcd_write_string("Display OFF soon...");
			if(lcd_status != HAL_OK) printf("Error writing display off text\r\n"); // Ajout du printf manquant
			HAL_Delay(SHORT_PAUSE_MS);
			if(lcd_status == HAL_OK) lcd_status = lcd_display_off(); // Éteint l'affichage
			if(lcd_status != HAL_OK) printf("Error display off\r\n");
			HAL_Delay(SHORT_PAUSE_MS);
			// Note: l'écriture suivante se fera "en aveugle" car l'écran est éteint
			if(lcd_status == HAL_OK) lcd_status = lcd_set_cursor(1, 0);
			if(lcd_status == HAL_OK) lcd_status = lcd_write_string("Display ON again!");
			if(lcd_status == HAL_OK) lcd_status = lcd_display_on(); // Ré-allume l'affichage
			if(lcd_status != HAL_OK) printf("Error display on\r\n");
			HAL_Delay(SHORT_PAUSE_MS);
			break;
		case 5:
			printf("Mode 5: Native Scroll Demo\r\n");
			// Note: lcd_clear() place déjà le curseur en (0,0)
			lcd_status = lcd_write_string("Native Scroll Left->");
			if(lcd_status != HAL_OK) printf("Error writing native scroll text\r\n");
			for(int i=0; i<COLUMNS/2; i++) { // Défilement vers la gauche
				if(lcd_status == HAL_OK) lcd_status = lcd_scroll_display_left();
				if(lcd_status != HAL_OK) { printf("Error native scroll left\r\n"); break; }
				HAL_Delay(SCROLL_STEP_DELAY_MS);
			}
			HAL_Delay(SHORT_PAUSE_MS);
			for(int i=0; i<COLUMNS/2; i++) { // Défilement vers la droite pour revenir
				if(lcd_status == HAL_OK) lcd_status = lcd_scroll_display_right();
				if(lcd_status != HAL_OK) { printf("Error native scroll right\r\n"); break; }
				HAL_Delay(SCROLL_STEP_DELAY_MS);
			}
			HAL_Delay(SHORT_PAUSE_MS);
			break;
		}
	  mode++;
      }
	}
	/* USER CODE END WHILE */
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

## Codes d'Erreur (HAL_StatusTypeDef)

HAL_OK: Opération réussie.
HAL_ERROR: Erreur générale.
HAL_BUSY: Le périphérique I2C est occupé.
HAL_TIMEOUT: Timeout lors de la communication I2C.


