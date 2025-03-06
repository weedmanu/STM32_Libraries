#ifndef STM32_I2C_LCD_H
#define STM32_I2C_LCD_H

/***  Paramètres à modifier en fonction de votre carte STM32  ***/

#include "stm32l1xx_hal.h"

/*****************************************************************/

/* Définitions des broches du LCD */
#define RS_BIT   0      // Bit pour le Register Select
#define EN_BIT   2      // Bit pour le Enable
#define BL_BIT   3      // Bit pour le rétroéclairage (BackLight)
#define D4_BIT   4      // Bit pour le Data 4
#define D5_BIT   5      // Bit pour le Data 5
#define D6_BIT   6      // Bit pour le Data 6
#define D7_BIT   7      // Bit pour le Data 7

/* Déclarations des fonctions pour manipuler le LCD */

/**
 * @brief Initialise le LCD.
 * @param hi2c Pointeur vers la structure I2C_HandleTypeDef.
 * @param columns Nombre de colonnes du LCD.
 * @param rows Nombre de lignes du LCD.
 * @param i2c_address Adresse I2C du LCD.
 */
void lcd_init(I2C_HandleTypeDef *hi2c, uint8_t columns, uint8_t rows, uint8_t i2c_address);

/**
 * @brief Écrit une chaîne de caractères sur le LCD.
 * @param str Chaîne de caractères à écrire.
 */
void lcd_write_string(char *str);

/**
 * @brief Écrit un seul caractère sur le LCD.
 * @param ascii_char Caractère ASCII à écrire.
 */
void lcd_write_char(char ascii_char);

/**
 * @brief Positionne le curseur sur le LCD.
 * @param row Ligne du curseur.
 * @param column Colonne du curseur.
 */
void lcd_set_cursor(uint8_t row, uint8_t column);

/**
 * @brief Efface l'affichage du LCD.
 */
void lcd_clear(void);

/**
 * @brief Contrôle l'état du rétroéclairage.
 * @param state État du rétroéclairage (1 pour allumé, 0 pour éteint).
 */
void lcd_backlight(uint8_t state);

/**
 * @brief Crée un caractère personnalisé.
 * @param location Emplacement dans la CGRAM (0-7).
 * @param charmap Tableau de 8 octets définissant le caractère.
 */
void lcd_create_char(uint8_t location, uint8_t charmap[8]);

/**
 * @brief Affiche un caractère personnalisé.
 * @param location Emplacement du caractère dans la CGRAM (0-7).
 */
void lcd_put_custom_char(uint8_t location);

/**
 * @brief Fait défiler le texte vers la gauche.
 */
void lcd_scroll_left(void);

/**
 * @brief Fait défiler le texte vers la droite.
 */
void lcd_scroll_right(void);

/**
 * @brief Envoie des données au LCD.
 * @param data Données à envoyer.
 */
void lcd_send_data(uint8_t data);

#endif  // STM32_I2C_LCD_H
