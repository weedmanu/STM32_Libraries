
/* USER CODE BEGIN Includes */
#include <stdio.h>      	 	// Inclut la bibliothèque standard d'entrée-sortie pour utiliser printf
#include <string.h>     	  	// Inclut la bibliothèque de manipulation de chaînes de caractères pour utiliser strlen
#include "STM32_I2C_LCD.h" 		// Inclut le fichier d'en-tête pour la gestion de l'écran LCD via I2C
/* USER CODE END Includes */


/* USER CODE BEGIN PD */
#define COLUMNS		20		// Nombre de colonnes de l'écran LCD
#define ROWS		4     		// Nombre de lignes de l'écran LCD
#define I2C_ADDRESS 0x3f		// Adresse I2C de l'écran LCD
/* USER CODE END PD */


/* USER CODE BEGIN PV */
uint8_t mode = 0; // Variable pour suivre le mode d'affichage actuel
// Définition des caractères personnalisés : https://maxpromer.github.io/LCD-Character-Creator/
uint8_t HeartChar[8] = {0x00, 0x0a, 0x15, 0x11, 0x0a, 0x04, 0x00, 0x00}; 		// Tableau représentant un caractère personnalisé "cœur"
uint8_t DegreeChar[8] = {0x07, 0x05, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00}; 		// Tableau représentant un caractère personnalisé "degré"
uint8_t ArrowDownChar[8] = {0x00, 0x04, 0x04, 0x04, 0x04, 0x1F, 0x0E, 0x04}; 	// Tableau représentant un caractère personnalisé "flèche vers le bas"
uint8_t ArrowUpChar[8] = {0x04, 0x0E, 0x1F, 0x04, 0x04, 0x04, 0x04, 0x00}; 		// Tableau représentant un caractère personnalisé "flèche vers le haut"
uint8_t ManChar[8] = {0x1F, 0x15, 0x1F, 0x11, 0x1F, 0x0A, 0x0A, 0x1B}; 			// Tableau représentant un caractère personnalisé "homme"
/* USER CODE END PV */


/* USER CODE BEGIN 0 */
/**
 * @brief  Fonction pour initialiser les caractères personnalisés
 * @param  None
 * @retval None
 */
void initialize_custom_chars() {
	lcd_create_char(0, HeartChar);      // Créer un caractère personnalisé "cœur"
	lcd_create_char(1, DegreeChar);     // Créer un caractère personnalisé "degré"
	lcd_create_char(2, ArrowDownChar);  // Créer un caractère personnalisé "flèche vers le bas"
	lcd_create_char(3, ArrowUpChar);    // Créer un caractère personnalisé "flèche vers le haut"
	lcd_create_char(4, ManChar);        // Créer un caractère personnalisé "homme"
}

/**
 * @brief  Fonction pour afficher les caractères personnalisés sur l'écran LCD
 * @param  columns: Nombre de colonnes de l'écran LCD
 * @retval None
 */
void display_custom_chars(uint8_t columns) {
    lcd_set_cursor(0, 0); // Placer le curseur à la ligne 0, colonne 0
    lcd_write_string("Custom Chars: "); // Écrire le texte "Custom Chars: "

    if (columns == 16) { 		// Configuration pour un écran 16x2
        lcd_set_cursor(1, 0); 		// Placer le curseur à la ligne 1, colonne 0
        lcd_put_custom_char(0); 	// Cœur

        lcd_set_cursor(1, 2); 		// Placer le curseur à la ligne 1, colonne 2
        lcd_put_custom_char(1); 	// Degré

        lcd_set_cursor(1, 4); 		// Placer le curseur à la ligne 1, colonne 4
        lcd_put_custom_char(2); 	// Flèche vers le bas

        lcd_set_cursor(1, 6); 		// Placer le curseur à la ligne 1, colonne 6
        lcd_put_custom_char(3); 	// Flèche vers le haut

        lcd_set_cursor(1, 8); 		// Placer le curseur à la ligne 1, colonne 8
        lcd_put_custom_char(4); 	// Homme
    } else if (columns == 20) { // Configuration pour un écran 20x4
        lcd_set_cursor(2, 0);		// Placer le curseur à la ligne 2, colonne 0
        lcd_put_custom_char(0);     // Cœur

        lcd_set_cursor(2, 2);       // Placer le curseur à la ligne 2, colonne 2
        lcd_put_custom_char(1);		// Degré

        lcd_set_cursor(2, 4);		// Placer le curseur à la ligne 1, colonne 4
        lcd_put_custom_char(2);		// Flèche vers le bas

        lcd_set_cursor(2, 6);		// Placer le curseur à la ligne 1, colonne 6
        lcd_put_custom_char(3);		// Flèche vers le haut

        lcd_set_cursor(2, 8);		// Placer le curseur à la ligne 1, colonne 8
        lcd_put_custom_char(4);		// Homme
    }
}

/**
 * @brief  Fonction pour afficher les caractères ASCII sur l'écran LCD
 * @param  columns: Nombre de colonnes de l'écran LCD
 * @param  rows: Nombre de lignes de l'écran LCD
 * @retval None
 */
void display_ascii(uint8_t columns, uint8_t rows) {
    char ascii_group[rows][columns]; // Tableau pour stocker les caractères ASCII pour chaque ligne
    uint8_t start_char = 32;         // Commencer à l'espace (caractère 32)
    uint8_t end_char = 126;          // Finir au tilde (caractère 126)
    uint8_t i, j, k;                 // Variables pour les boucles
    uint8_t total_chars = end_char - start_char + 1; // Nombre total de caractères ASCII
    uint8_t chars_per_page = columns * rows; // Nombre de caractères par page
    uint8_t num_pages = (total_chars + chars_per_page - 1) / chars_per_page; // Nombre total de pages

    lcd_set_cursor(0, 0);
    lcd_write_string("Ascii Chars:"); // Écrire le texte "Ascii Chars:"

    for (k = 0; k < num_pages; k++) { // Boucle pour chaque page
        for (i = 0; i < rows; i++) { // Boucle pour chaque ligne
            for (j = 0; j < columns; j++) { // Boucle pour chaque colonne
                uint8_t char_index = k * chars_per_page + i * columns + j;
                if (char_index < total_chars) {
                    ascii_group[i][j] = start_char + char_index; // Stocker le caractère ASCII dans le tableau
                } else {
                    ascii_group[i][j] = ' '; // Remplir avec des espaces si nécessaire
                }
            }
        }

        for (i = 0; i < rows; i++) { // Afficher les caractères sur l'écran LCD
            lcd_set_cursor(i, 0);
            for (j = 0; j < columns; j++) {
                lcd_send_data(ascii_group[i][j]); // Envoyer le caractère ASCII à l'écran LCD
            }
        }

        HAL_Delay(2000); // Attendre 2 secondes avant d'afficher la page suivante
        lcd_clear(); // Effacer l'écran avant d'afficher la page suivante
    }
}

/**
 * @brief  Fonction pour faire défiler le texte vers la droite
 * @param  text: Chaîne de caractères à faire défiler
 * @param  columns: Nombre de colonnes de l'écran LCD
 * @retval None
 */
void scroll_text_right(char* text, uint8_t columns) {
	uint16_t text_len = strlen(text);                       // Longueur du texte
	uint8_t i;                                              // Variable pour la boucle
	for (i = 0; i < text_len; i++) {                        // Afficher le texte caractère par caractère en défilement vers la droite
		lcd_set_cursor(1, 0);                               // Placer le curseur à la ligne 1, colonne 0
		lcd_write_string(&text[i]);                         // Afficher le texte en commençant par le caractère actuel
		for (uint8_t j = i + 1; j < columns; j++) {         // Remplir le reste de la ligne avec des espaces
			lcd_write_char(' ');                            // Afficher un espace
		}
		HAL_Delay(300);                                     // Délai pour le défilement
	}
}

/**
 * @brief  Fonction pour faire défiler le texte vers la gauche
 * @param  text: Chaîne de caractères à faire défiler
 * @param  columns: Nombre de colonnes de l'écran LCD
 * @retval None
 */
void scroll_text_left(char* text, uint8_t columns) {
	uint16_t text_len = strlen(text);                     	// Longueur du texte
	uint8_t i;                                              // Variable pour la boucle
	for (i = 0; i < text_len; i++) {                        // Afficher le texte caractère par caractère en défilement vers la gauche
		lcd_set_cursor(1, 0);				// Placer le curseur à la ligne 1, colonne 0
		lcd_write_string(&text[text_len - i - 1]);      // Afficher le texte en commençant par le dernier caractère
		for (uint8_t j = i + 1; j < columns; j++) {     // Remplir le reste de la ligne avec des espaces
			lcd_write_char(' ');                    // Afficher un espace
		}
		HAL_Delay(300);                                 // Délai pour le défilement
	}
}
/* USER CODE END 0 */


/* USER CODE BEGIN 2 */
HAL_Delay(500);                                   // Délai pour les initialisations
printf("Test LIB LCD\n\r");                       // Écriture dans la console série

// Initialisation de l'écran LCD
lcd_init(&hi2c1, COLUMNS, ROWS, I2C_ADDRESS);     // Initialisation de l'écran
lcd_clear();                                      // Effacer l'écran
lcd_backlight(1);                                 // Allumer le rétroéclairage
initialize_custom_chars();                        // Appel de la fonction pour initialiser les caractères personnalisés

// Affichage de texte sur l'écran
lcd_set_cursor(0, 0);                             // Placer le curseur ligne 0 colonne 0
lcd_write_string("Test LIB LCD");                 // Écrire sur l'écran
lcd_set_cursor(1, 0);                             // Placer le curseur ligne 1 colonne 0
lcd_write_string("Init OK");                      // Écrire sur l'écran
HAL_Delay(3000);                                  // Attendre 3 secondes
/* USER CODE END 2 */

/* USER CODE BEGIN WHILE */
while (1) {
    lcd_clear();                                    // Efface l'écran LCD
    switch (mode) {                                 // Sélectionne le mode d'affichage
    case 0:                                         // Mode 0
        display_ascii(COLUMNS, ROWS);                 // Affiche les caractères ASCII
        break;                                        // Sortie de la boucle
    case 1:                                         // Mode 1
        display_custom_chars(COLUMNS);                // Affiche les caractères personnalisés
        break;                                        // Sortie de la boucle
    case 2:
        lcd_set_cursor(0, 0); // Placer le curseur à la ligne 0, colonne 0
        lcd_write_string("Scroll Right:"); // Écrire le texte "Scroll Right:"
        lcd_set_cursor(1, 0); // Placer le curseur à la ligne 1, colonne 0
        lcd_write_string("Hello !!!"); // Écrire le texte à défiler
        scroll_text_right("Hello !!!", COLUMNS); // Faire défiler le texte vers la droite
        break;
    case 3:
        lcd_set_cursor(0, 0); // Placer le curseur à la ligne 0, colonne 0
        lcd_write_string("Scroll Left: "); // Écrire le texte "Scroll Left: "
        lcd_set_cursor(1, 0); // Placer le curseur à la ligne 1, colonne 0
        lcd_write_string("Hello !!!"); // Écrire le texte à défiler
        scroll_text_left("Hello !!!", COLUMNS); // Faire défiler le texte vers la gauche
        break;
    }
HAL_Delay(3000);                                // Attendre 3 secondes avant de changer de mode
mode = (mode + 1) % 4;                          // Changer de mode à chaque itération
/* USER CODE END WHILE */


/* USER CODE BEGIN 4 */
// Fonction pour envoyer un caractère via UART
int __io_putchar(int ch) {              
	HAL_UART_Transmit(&huart2, (uint8_t*) &ch, 1, 0xFFFF);  // Transmet le caractère via UART
	return ch;                                              // Retourne le caractère transmis
}
/* USER CODE END 4 */

/* USER CODE BEGIN Error_Handler_Debug */
// Affiche le fichier et la ligne où l'erreur s'est produite
printf("Error in file %s at line %d\r\n", __FILE__, __LINE__);
/* User can add his own implementation to report the HAL error return state */
__disable_irq();
while (1)
{

}
/* USER CODE END Error_Handler_Debug */

