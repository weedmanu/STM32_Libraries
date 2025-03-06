#include "STM32_I2C_LCD.h" 		// Inclut le fichier d'en-tête pour la gestion de l'écran LCD via I2C
#include <string.h>         	// Inclut la bibliothèque de manipulation de chaînes de caractères
#include <stdio.h>          	// Inclut la bibliothèque standard d'entrée-sortie pour utiliser printf

// Structure de configuration de l'écran LCD
typedef struct {
    uint8_t rows;           	// Nombre de lignes de l'écran LCD
    uint8_t columns;        	// Nombre de colonnes de l'écran LCD
    uint8_t line_addresses[4]; 	// Adresses de début de chaque ligne pour le LCD
    uint8_t i2c_address;    	// Adresse I2C de l'écran LCD
} LCD_Config;

// Variables globales
static I2C_HandleTypeDef *lcd_hi2c;                         // Pointeur vers la structure I2C
static uint8_t backlight_state = 1;                         // État du rétroéclairage
static LCD_Config lcd_config;                               // Configuration de l'écran LCD

// Prototypes des fonctions privées
static void lcd_write_nibble(uint8_t nibble, uint8_t rs); 	// Fonction pour écrire un demi-octet sur le LCD
static void lcd_send_cmd(uint8_t cmd); 						// Fonction pour envoyer une commande au LCD


/**
 * @brief Initialisation de l'écran LCD
 * @param  hi2c: Pointeur vers la structure I2C
 * @param  columns: Nombre de colonnes de l'écran LCD
 * @param  rows: Nombre de lignes de l'écran LCD
 * @param  i2c_address: Adresse I2C de l'écran LCD
 * @retval None
 */
void lcd_init(I2C_HandleTypeDef *hi2c, uint8_t columns, uint8_t rows, uint8_t i2c_address) {
    lcd_hi2c = hi2c;                                    // Initialisation du pointeur I2C                        
    lcd_config.rows = rows;                             // Initialisation du paramètre de configuration rows
    lcd_config.columns = columns;                       // Initialisation du paramètre de configuration columns
    lcd_config.i2c_address = i2c_address;               // Initialisation du paramètre de configuration i2c_address

    if (rows == 4 && columns == 20) {                   // Initialisation des adresses de début de chaque ligne pour le LCD 20x4   
        lcd_config.line_addresses[0] = 0x00;            // Adresse de début de la ligne 0
        lcd_config.line_addresses[1] = 0x40;            // Adresse de début de la ligne 1
        lcd_config.line_addresses[2] = 0x14;            // Adresse de début de la ligne 2
        lcd_config.line_addresses[3] = 0x54;            // Adresse de début de la ligne 3
    } else {                                            // Initialisation des adresses de début de chaque ligne pour le LCD 16x2
        lcd_config.line_addresses[0] = 0x00;            // Adresse de début de la ligne 0
        lcd_config.line_addresses[1] = 0x40;            // Adresse de début de la ligne 1
        lcd_config.line_addresses[2] = 0x00;            // Adresse de début de la ligne 2
        lcd_config.line_addresses[3] = 0x00;            // Adresse de début de la ligne 3
    }

    // Séquence d'initialisation de l'écran LCD
    HAL_Delay(50);                                      // Délai pour l'initialisation
    lcd_write_nibble(0x03, 0);                          // Envoi de la commande 0x03
    HAL_Delay(5);                                       // Délai pour l'initialisation                         
    lcd_write_nibble(0x03, 0);                          // Envoi de la commande 0x03
    HAL_Delay(1);                                       // Délai pour l'initialisation
    lcd_write_nibble(0x03, 0);                          // Envoi de la commande 0x03
    HAL_Delay(1);                                       // Délai pour l'initialisation
    lcd_write_nibble(0x02, 0);                          // Envoi de la commande 0x02
    lcd_send_cmd(0x28);                                 // Envoi de la commande 0x28
    lcd_send_cmd(0x0C);                                 // Envoi de la commande 0x0C
    lcd_send_cmd(0x06);                                 // Envoi de la commande 0x06
    lcd_send_cmd(0x01);                                 // Envoi de la commande 0x01
    HAL_Delay(2);                                       // Délai pour l'initialisation
}

/**
 * @brief Positionne le curseur de l'écran LCD
 * @param  row: Ligne de l'écran LCD
 * @param  column: Colonne de l'écran LCD
 * @retval None
 */
void lcd_set_cursor(uint8_t row, uint8_t column) {
    if (row < lcd_config.rows && column < lcd_config.columns) {     // Vérifie si la ligne et la colonne sont valides
        uint8_t address = lcd_config.line_addresses[row] + column;  // Calcule l'adresse de la position du curseur
        lcd_send_cmd(0x80 | address);                               // Envoi de la commande pour positionner le curseur
    }
}

/**
 * @brief Écrit un caractère sur l'écran LCD
 * @param  ascii_char: Caractère ASCII à afficher
 * @retval None
 */
void lcd_write_char(char ascii_char) {
    lcd_send_data(ascii_char);               // Envoi du caractère ASCII
}

/**
 * @brief Écrit une chaîne de caractères sur l'écran LCD
 * @param  str: Chaîne de caractères à afficher
 * @retval None
 */
void lcd_write_string(char *str) {
    while (*str) {                          // Parcourt la chaîne de caractères
        lcd_send_data(*str++);              // Envoi du caractère à l'écran LCD
    }
}

/**
 * @brief Efface l'écran LCD
 * @param  None
 * @retval None
 */
void lcd_clear(void) {
    lcd_send_cmd(0x01);                     // Envoi de la commande pour effacer l'écran                             
    HAL_Delay(2);                           // Délai pour l'effacement                
}

/**
 * @brief Allume ou éteint le rétroéclairage de l'écran LCD
 * @param  state: État du rétroéclairage (1: Allumé, 0: Éteint)
 * @retval None
 */
void lcd_backlight(uint8_t state) {
    backlight_state = state ? 1 : 0;        // Mise à jour de l'état du rétroéclairage
}

/**
 * @brief Crée un caractère personnalisé sur l'écran LCD
 * @param  location: Emplacement du caractère personnalisé (0 à 7)
 * @param  charmap: Tableau de 8 octets pour définir le caractère personnalisé
 * @retval None
 */
void lcd_create_char(uint8_t location, uint8_t charmap[8]) {
    location &= 0x07;                           // Masque pour s'assurer que l'emplacement est entre 0 et 7
    lcd_send_cmd(0x40 | (location << 3));       // Envoi de la commande pour créer un caractère personnalisé
    for (int i = 0; i < 8; i++) {               // Parcourt les 8 octets du caractère personnalisé
        lcd_send_data(charmap[i]);              // Envoi de l'octet du caractère personnalisé
    }
}

/**
 * @brief Affiche un caractère personnalisé sur l'écran LCD
 * @param  location: Emplacement du caractère personnalisé (0 à 7)
 * @retval None
 */
void lcd_put_custom_char(uint8_t location) {
    lcd_send_data(location);                    // Envoi de l'emplacement du caractère personnalisé             
}

/**
 * @brief Fait défiler le texte vers la droite sur l'écran LCD
 * @param  text: Chaîne de caractères à faire défiler
 * @param  columns: Nombre de colonnes de l'écran LCD
 * @retval None
 */
static void lcd_write_nibble(uint8_t nibble, uint8_t rs) {
    uint8_t data = (nibble << 4) | (rs << 0) | (backlight_state << 3) | (1 << 2);                   // Crée le demi-octet de données
    if (HAL_I2C_Master_Transmit(lcd_hi2c, lcd_config.i2c_address << 1, &data, 1, 100) != HAL_OK) {  // Envoie le demi-octet de données
        printf("Erreur de transmission I2C\r\n");                                                   // Affiche un message d'erreur en cas d'échec de la transmission I2C
    }
    HAL_Delay(1);                                                                                   // Délai pour l'envoi du demi-octet
    data &= ~(1 << 2);                                                                              // Réinitialise le bit d'horloge
    if (HAL_I2C_Master_Transmit(lcd_hi2c, lcd_config.i2c_address << 1, &data, 1, 100) != HAL_OK) {  // Envoie le demi-octet de données
        printf("Erreur de transmission I2C\r\n");                                                   // Affiche un message d'erreur en cas d'échec de la transmission I2C
    }
}

/**
 * @brief Envoie une commande au LCD
 * @param  cmd: Commande à envoyer
 * @retval None
 */
static void lcd_send_cmd(uint8_t cmd) {
    uint8_t upper_nibble = cmd >> 4;        // Sépare le demi-octet supérieur
    uint8_t lower_nibble = cmd & 0x0F;      // Sépare le demi-octet inférieur
    lcd_write_nibble(upper_nibble, 0);      // Envoie le demi-octet supérieur
    lcd_write_nibble(lower_nibble, 0);      // Envoie le demi-octet inférieur
    if (cmd == 0x01 || cmd == 0x02) {       // Si la commande est 0x01 ou 0x02
        HAL_Delay(2);                       // Délai pour l'exécution de la commande
    }
}

/**
 * @brief Envoie des données au LCD
 * @param  data: Données à envoyer
 * @retval None
 */
void lcd_send_data(uint8_t data) {          // Envoie des données au LCD
    uint8_t upper_nibble = data >> 4;       // Sépare le demi-octet supérieur
    uint8_t lower_nibble = data & 0x0F;     // Sépare le demi-octet inférieur
    lcd_write_nibble(upper_nibble, 1);      // Envoie le demi-octet supérieur
    lcd_write_nibble(lower_nibble, 1);      // Envoie le demi-octet inférieur
}

/**
 * @brief Fait défiler le texte vers la droite sur l'écran LCD
 * @param  text: Chaîne de caractères à faire défiler
 * @param  columns: Nombre de colonnes de l'écran LCD
 * @retval None
 */
void lcd_scroll_right(void) {
    lcd_send_cmd(0x1E);                     // Envoie de la commande pour faire défiler le texte vers la droite
}

/**
 * @brief Fait défiler le texte vers la gauche sur l'écran LCD
 * @param  None
 * @retval None
 */
void lcd_scroll_left(void) {
    lcd_send_cmd(0x18);                     // Envoie de la commande pour faire défiler le texte vers la gauche
}
