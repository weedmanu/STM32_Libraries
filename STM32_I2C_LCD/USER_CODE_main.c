/* USER CODE BEGIN Includes */
#include <stdio.h>      	  // Inclut la bibliothèque standard d'entrée-sortie pour utiliser printf
#include <string.h>     	  // Inclut la bibliothèque de manipulation de chaînes de caractères pour utiliser strlen
#include "STM32_I2C_LCD.h" 	// Inclut le fichier d'en-tête pour la gestion de l'écran LCD via I2C
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
#define COLUMNS		20	 	  // Nombre de colonnes de l'écran LCD
#define ROWS		4     	  // Nombre de lignes de l'écran LCD
#define I2C_ADDRESS 0x3f	  // Adresse I2C de l'écran LCD
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
    HAL_StatusTypeDef status;
    status = lcd_create_char(0, HeartChar);      // Créer un caractère personnalisé "cœur"
    if(status != HAL_OK) printf("Error creating char 0\r\n");
    status = lcd_create_char(1, DegreeChar);     // Créer un caractère personnalisé "degré"
    if(status != HAL_OK) printf("Error creating char 1\r\n");
    status = lcd_create_char(2, ArrowDownChar);  // Créer un caractère personnalisé "flèche vers le bas"
    if(status != HAL_OK) printf("Error creating char 2\r\n");
    status = lcd_create_char(3, ArrowUpChar);    // Créer un caractère personnalisé "flèche vers le haut"
    if(status != HAL_OK) printf("Error creating char 3\r\n");
    status = lcd_create_char(4, ManChar);        // Créer un caractère personnalisé "homme"
    if(status != HAL_OK) printf("Error creating char 4\r\n");
}

/**
 * @brief  Fonction pour afficher les caractères personnalisés sur l'écran LCD
 * @param  columns: Nombre de colonnes de l'écran LCD
 * @retval None
 */
void display_custom_chars(uint8_t columns) {
    HAL_StatusTypeDef status;

    status = lcd_set_cursor(0, 0); // Placer le curseur à la ligne 0, colonne 0
    if(status != HAL_OK) { printf("Error set_cursor(0,0) in custom_chars\r\n"); return; }
    status = lcd_write_string("Custom Chars: "); // Écrire le texte "Custom Chars: "
    if(status != HAL_OK) { printf("Error write_string in custom_chars\r\n"); return; }

    if (columns == 16) { 		// Configuration pour un écran 16x2
        status = lcd_set_cursor(1, 0); 		// Placer le curseur à la ligne 1, colonne 0
        if(status == HAL_OK) status = lcd_put_custom_char(0); 	// Cœur

        if(status == HAL_OK) status = lcd_set_cursor(1, 2); 		// Placer le curseur à la ligne 1, colonne 2
        if(status == HAL_OK) status = lcd_put_custom_char(1); 	// Degré

        if(status == HAL_OK) status = lcd_set_cursor(1, 4); 		// Placer le curseur à la ligne 1, colonne 4
        if(status == HAL_OK) status = lcd_put_custom_char(2); 	// Flèche vers le bas

        if(status == HAL_OK) status = lcd_set_cursor(1, 6); 		// Placer le curseur à la ligne 1, colonne 6
        if(status == HAL_OK) status = lcd_put_custom_char(3); 	// Flèche vers le haut

        if(status == HAL_OK) status = lcd_set_cursor(1, 8); 		// Placer le curseur à la ligne 1, colonne 8
        if(status == HAL_OK) status = lcd_put_custom_char(4); 	// Homme
    } else if (columns == 20) { // Configuration pour un écran 20x4
        status = lcd_set_cursor(2, 0);		// Placer le curseur à la ligne 2, colonne 0
        if(status == HAL_OK) status = lcd_put_custom_char(0);     // Cœur

        if(status == HAL_OK) status = lcd_set_cursor(2, 2);       // Placer le curseur à la ligne 2, colonne 2
        if(status == HAL_OK) status = lcd_put_custom_char(1);		// Degré

        if(status == HAL_OK) status = lcd_set_cursor(2, 4);		// Placer le curseur à la ligne 2, colonne 4
        if(status == HAL_OK) status = lcd_put_custom_char(2);		// Flèche vers le bas

        if(status == HAL_OK) status = lcd_set_cursor(2, 6);		// Placer le curseur à la ligne 2, colonne 6
        if(status == HAL_OK) status = lcd_put_custom_char(3);		// Flèche vers le haut

        if(status == HAL_OK) status = lcd_set_cursor(2, 8);		// Placer le curseur à la ligne 2, colonne 8
        if(status == HAL_OK) status = lcd_put_custom_char(4);		// Homme
    }

    if(status != HAL_OK) {
        printf("Error displaying custom char\r\n");
    }
    HAL_Delay(2000); 				// Attendre 2 secondes
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
    status = lcd_set_cursor(0, 0);
    if(status != HAL_OK) { printf("Error set_cursor(0,0) in display_ascii\r\n"); return; }
    status = lcd_write_string("Ascii Chars:"); // Écrire le texte "Ascii Chars:"
    if(status != HAL_OK) { printf("Error write_string in display_ascii\r\n"); return; }

    HAL_Delay(1000); // Attendre 1 secondes avant d'afficher la page suivante

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
        HAL_Delay(2000); // Attendre 2 secondes avant d'afficher la page suivante
        if (current_char > end_char) break; // Sortir si tous les caractères ont été affichés (sécurité)
    }
}
/* USER CODE END 0 */

/* USER CODE BEGIN 2 */
HAL_Delay(500);                                   // Délai pour les initialisations
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
    lcd_status = lcd_set_cursor(0, 0);                             // Placer le curseur ligne 0 colonne 0
    if(lcd_status == HAL_OK) lcd_status = lcd_write_string("Test LIB LCD"); // Écrire sur l'écran
    if(lcd_status == HAL_OK) lcd_status = lcd_set_cursor(1, 0);    // Placer le curseur ligne 1 colonne 0
    if(lcd_status == HAL_OK) lcd_status = lcd_write_string("Init OK");      // Écrire sur l'écran

    if(lcd_status != HAL_OK) printf("Error writing initial text\r\n");
    HAL_Delay(2000);                                  // Attendre 2 secondes
}
/* USER CODE END 2 */

/* USER CODE BEGIN WHILE */
while (1) {
    switch (mode) { // Sélectionne le mode d'affichage
    case 0:
        display_ascii(COLUMNS, ROWS); // Affiche les caractères ASCII
        break;
    case 1:
        display_custom_chars(COLUMNS); // Affiche les caractères personnalisés
        break;
    case 2:
        lcd_status = lcd_set_cursor(0, 0); // Place le curseur à la ligne 0, colonne 0
        if(lcd_status == HAL_OK) lcd_status = lcd_write_string("Scroll Right:"); // Écrit le texte "Scroll Right:"
        // Appel de scroll_text_right avec la ligne (1) et un délai (300ms)
        if(lcd_status == HAL_OK) lcd_status = scroll_text_right("Hello !!!", 1, COLUMNS, 300);
        if(lcd_status != HAL_OK) printf("Error during scroll right\r\n");
        HAL_Delay(2000); 				// Attendre 2 secondes
        break;
    case 3:
        lcd_status = lcd_set_cursor(0, 0); // Place le curseur à la ligne 0, colonne 0
        if(lcd_status == HAL_OK) lcd_status = lcd_write_string("Scroll Left: "); // Écrit le texte "Scroll Left: "
        // Appel de scroll_text_left avec la ligne (1) et un délai (300ms)
        if(lcd_status == HAL_OK) lcd_status = scroll_text_left("Hello !!!", 1, COLUMNS, 300);
        if(lcd_status != HAL_OK) printf("Error during scroll left\r\n");
        HAL_Delay(2000); 				// Attendre 2 secondes
        break;
    }
    mode++;          			// Changer de mode à chaque itération
    if (mode > 3) {mode = 0;}	// Remise à zero du mode
    lcd_clear();
}
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
