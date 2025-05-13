/**
 * @file    STM32_I2C_LCD.h
 * @brief   Fichier d'en-tête pour la librairie de contrôle d'écrans LCD
 *          alphanumériques via I2C (avec expandeur PCF8574) sur STM32.
 * @author  manu
 * @version 1.1
 * @date    2023-10-05
 *
 * @note    Cette librairie utilise les fonctions HAL de STMicroelectronics.
 *          Elle est conçue pour fonctionner avec des écrans LCD compatibles HD44780
 *          connectés via un module I2C basé sur le PCF8574.
 */

#ifndef STM32_I2C_LCD_H
#define STM32_I2C_LCD_H

/****************************************************************************
 * @note Modifier en fonction de votre carte STM32.
 *****************************************************************************/
#include "stm32l0xx_hal.h" // Remplacez stm32l0xx_hal.h si vous utilisez une autre série de carte ex : stm32f4xx_hal.h.

/****************************************************************************
 * @note Dé-commenter la ligne suivante pour activer les messages de débogage via printf.
 *****************************************************************************/
// #define DEBUG_ON // Définit DEBUG_ON pour activer les messages de débogage

#ifdef DEBUG_ON
#include <stdio.h>                                       // Inclure stdio.h pour utiliser printf
#define DEBUG_PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__) // Macro pour l'affichage de débogage
#else
#define DEBUG_PRINT(fmt, ...) ((void)0) // Macro vide si le débogage est désactivé
#endif

/* Déclarations des fonctions pour manipuler le LCD */

/**
 * @brief Initialise le LCD.
 * @param hi2c Pointeur vers la structure I2C_HandleTypeDef.
 * @param columns Nombre de colonnes du LCD.
 * @param rows Nombre de lignes du LCD.
 * @param i2c_address Adresse I2C du LCD.
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_init(I2C_HandleTypeDef *hi2c, uint8_t columns, uint8_t rows, uint8_t i2c_address); // Initialisation de l'écran LCD

/**
 * @brief Écrit une chaîne de caractères sur le LCD.
 * @param str Chaîne de caractères à écrire.
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_write_string(char *str);

/**
 * @brief Écrit un seul caractère sur le LCD.
 * @param ascii_char Caractère ASCII à écrire.
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_write_char(char ascii_char);

/**
 * @brief Positionne le curseur sur le LCD.
 * @param row Ligne du curseur (0-based).
 * @param column Colonne du curseur (0-based).
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_set_cursor(uint8_t row, uint8_t column);

/**
 * @brief Efface l'affichage du LCD.
 * @note Cette fonction introduit un délai bloquant.
 */
HAL_StatusTypeDef lcd_clear(void);

/**
 * @brief Replace le curseur en position (0,0) sans effacer l'écran.
 * @note Cette fonction introduit un délai bloquant.
 */
HAL_StatusTypeDef lcd_home(void);

/**
 * @brief Contrôle l'état du rétroéclairage.
 * @param state État du rétroéclairage (1 pour allumé, 0 pour éteint).
 * @note L'état prend effet lors de la prochaine transmission I2C.
 */
void lcd_backlight(uint8_t state);

/**
 * @brief Crée un caractère personnalisé.
 * @param location Emplacement dans la CGRAM (0-7).
 * @param charmap Tableau de 8 octets définissant le caractère.
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_create_char(uint8_t location, uint8_t charmap[8]);

/**
 * @brief Affiche un caractère personnalisé préalablement créé.
 * @param location Emplacement du caractère dans la CGRAM (0-7).
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_put_custom_char(uint8_t location);

/**
 * @brief Active l'affichage du LCD (si éteint).
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_display_on(void);

/**
 * @brief Désactive l'affichage du LCD (le contenu reste en mémoire).
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_display_off(void);

/**
 * @brief Affiche le curseur (souligné).
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_cursor_on(void);

/**
 * @brief Masque le curseur.
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_cursor_off(void);

/**
 * @brief Active le clignotement du curseur (bloc).
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_blink_on(void);

/**
 * @brief Désactive le clignotement du curseur.
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_blink_off(void);

/**
 * @brief Envoie des données (un caractère à afficher) au LCD.
 * @param data Données (octet du caractère) à envoyer.
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_send_data(uint8_t data);

/**
 * @brief Décale tout l'affichage d'une colonne vers la gauche (commande native).
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_scroll_display_left(void);

/**
 * @brief Décale tout l'affichage d'une colonne vers la droite (commande native).
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_scroll_display_right(void);

#endif // STM32_I2C_LCD_H
