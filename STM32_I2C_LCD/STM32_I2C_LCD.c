#include "STM32_I2C_LCD.h"        // Inclut le fichier d'en-tête pour la gestion de l'écran LCD via I2C
#include <string.h>               // Inclut la bibliothèque de manipulation de chaînes de caractères

// Timeout pour les transmissions I2C (ms)
#define LCD_I2C_TIMEOUT_MS      100

/* Définitions des broches du LCD (via l'expandeur I2C PCF8574) */
#define RS_BIT   0      // Bit pour le Register Select (P0)
#define RW_BIT   1      // Bit pour le Read/Write (P1) - Généralement non utilisé en écriture seule, mis à 0
#define EN_BIT   2      // Bit pour le Enable (P2)
#define BL_BIT   3      // Bit pour le rétroéclairage (BackLight) (P3)
#define D4_BIT   4      // Bit pour le Data 4 (P4)
#define D5_BIT   5      // Bit pour le Data 5 (P5)
#define D6_BIT   6      // Bit pour le Data 6 (P6)
#define D7_BIT   7      // Bit pour le Data 7 (P7)


// --- Constantes pour la lisibilité ---
// Commandes LCD
#define LCD_CMD_CLEAR_DISPLAY   0x01 // Efface l'affichage
#define LCD_CMD_RETURN_HOME     0x02 // Retourne le curseur à la position initiale
#define LCD_CMD_ENTRY_MODE_SET  0x06 // Mode d'entrée: Incrémenter curseur, pas de décalage écran (0x04 pour décrémenter)
#define LCD_CMD_DISPLAY_CONTROL 0x0C // Contrôle affichage: Écran ON, Curseur OFF, Clignotement OFF
#define LCD_CMD_FUNCTION_SET_4BIT 0x28 // Configuration: Mode 4 bits, nb lignes (détecté), police 5x8
#define LCD_CMD_SET_DDRAM_ADDR  0x80 // Commande de base pour définir l'adresse DDRAM (position curseur)
#define LCD_CMD_SET_CGRAM_ADDR  0x40 // Commande de base pour définir l'adresse CGRAM (caractères perso)
#define LCD_CMD_SCROLL_LEFT     0x18 // Commande native pour décaler l'affichage vers la gauche
#define LCD_CMD_SCROLL_RIGHT    0x1C // Commande native pour décaler l'affichage vers la droite

// Bits pour la commande Display Control (base 0x08)
#define LCD_DISPLAY_ON_BIT  2 // D: Display ON/OFF
#define LCD_CURSOR_ON_BIT   1 // C: Cursor ON/OFF
#define LCD_BLINK_ON_BIT    0 // B: Blink ON/OFF

// Délais (ms)
#define LCD_DELAY_POWER_ON      50 // Délai après mise sous tension
#define LCD_DELAY_INIT_CMD      5  // Délai après certaines commandes d'initialisation
#define LCD_DELAY_CLEAR_HOME    2  // Délai après les commandes Clear Display ou Return Home

// --- Structure de configuration interne ---
typedef struct {
	uint8_t rows;           	// Nombre de lignes de l'écran LCD
	uint8_t columns;        	// Nombre de colonnes de l'écran LCD
	uint8_t line_addresses[4]; 	// Adresses de début de chaque ligne pour le LCD
	uint8_t i2c_address;    	// Adresse I2C de l'écran LCD (format 7 bits)
} LCD_Config;

// --- Variables globales statiques ---
static I2C_HandleTypeDef *lcd_hi2c;         // Pointeur vers la structure I2C HAL
static uint8_t backlight_state = 1;         // État du rétroéclairage (1 = ON, 0 = OFF)
static uint8_t display_control_state = 0x0C; // État actuel de Display Control (Initialisé à Display ON, Cursor OFF, Blink OFF)
static LCD_Config lcd_config;               // Configuration de l'écran LCD

// --- Prototypes des fonctions privées ---
static HAL_StatusTypeDef lcd_write_nibble(uint8_t nibble, uint8_t rs);   // Écrit un demi-octet (4 bits) sur le LCD
static HAL_StatusTypeDef lcd_send_cmd(uint8_t cmd);                      // Envoie une commande au LCD
static HAL_StatusTypeDef lcd_send_data_internal(uint8_t data);           // Envoie des données (caractère) au LCD (interne)

// ============================================================================
// Fonctions Publiques (définies dans STM32_I2C_LCD.h)
// ============================================================================

/**
 * @brief Initialise le LCD.
 * @param hi2c Pointeur vers la structure I2C_HandleTypeDef.
 * @param columns Nombre de colonnes du LCD.
 * @param rows Nombre de lignes du LCD.
 * @param i2c_address Adresse I2C du LCD (format 7 bits, ex: 0x27).
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_init(I2C_HandleTypeDef *hi2c, uint8_t columns, uint8_t rows, uint8_t i2c_address) {
	DEBUG_PRINT("lcd_init: Initializing LCD %dx%d at I2C addr 0x%02X\r\n", columns, rows, i2c_address);
	lcd_hi2c = hi2c;
	lcd_config.rows = rows;
	lcd_config.columns = columns;
	lcd_config.i2c_address = i2c_address; // Stocke l'adresse 7 bits

	// Définition des adresses de début de ligne (communes pour la plupart des LCD)
	lcd_config.line_addresses[0] = 0x00;
	lcd_config.line_addresses[1] = 0x40;
	if (rows == 4 && columns == 20) { // Adresses spécifiques pour 20x4
		lcd_config.line_addresses[2] = 0x14;
		lcd_config.line_addresses[3] = 0x54;
	} else { // Adresses pour 16x2 (ou autres tailles non 20x4)
		lcd_config.line_addresses[2] = 0x00 + columns; // Souvent 0x10 pour 16x2, mais calculé pour flexibilité
		lcd_config.line_addresses[3] = 0x40 + columns; // Souvent 0x50 pour 16x2
	}

	HAL_StatusTypeDef status = HAL_OK;

	// --- Séquence d'initialisation en mode 4 bits ---
	HAL_Delay(LCD_DELAY_POWER_ON); // Attente après mise sous tension

	// Étapes pour passer de 8 bits (par défaut au démarrage) à 4 bits
	// La séquence suivante (envoyer 3 fois 0x3) est la procédure standard
	// pour s'assurer que le LCD est en mode 8 bits avant de passer en mode 4 bits,
	// quel que soit l'état dans lequel il se trouvait auparavant.
	status = lcd_write_nibble(0x03, 0); // Envoi 0x30 (mode 8 bits)
	if(status != HAL_OK) {
		DEBUG_PRINT("lcd_init: Error step 1 (write 0x03), Status: %d\r\n", status);
		return status;
	}
	HAL_Delay(LCD_DELAY_INIT_CMD);

	status = lcd_write_nibble(0x03, 0); // Envoi 0x30 (mode 8 bits)
	if(status != HAL_OK) {
		DEBUG_PRINT("lcd_init: Error step 2 (write 0x03), Status: %d\r\n", status);
		return status;
	}
	HAL_Delay(1); // Délai plus court (1ms > 100us requis)

	status = lcd_write_nibble(0x03, 0); // Envoi 0x30 (mode 8 bits)
	if(status != HAL_OK) {
		DEBUG_PRINT("lcd_init: Error step 3 (write 0x03), Status: %d\r\n", status);
		return status;
	}
	HAL_Delay(1);

	// Passage final en mode 4 bits
	status = lcd_write_nibble(0x02, 0); // Envoi 0x20 (commande Function Set pour 4 bits)
	if(status != HAL_OK) {
		DEBUG_PRINT("lcd_init: Error step 4 (write 0x02 - Set 4-bit mode), Status: %d\r\n", status);
		return status;
	}
	HAL_Delay(1); // Petit délai après le changement de mode crucial

	// --- Configuration en mode 4 bits ---
	status = lcd_send_cmd(LCD_CMD_FUNCTION_SET_4BIT); // Fonction: 4 bits, nb lignes (auto), police 5x8
	if(status != HAL_OK) {
		DEBUG_PRINT("lcd_init: Error sending FUNCTION_SET_4BIT (0x%02X), Status: %d\r\n", LCD_CMD_FUNCTION_SET_4BIT, status);
		return status;
	}

	// Initialise l'état de contrôle (Display ON, Cursor OFF, Blink OFF par défaut) et l'envoie
	display_control_state = (1 << LCD_DISPLAY_ON_BIT) | (0 << LCD_CURSOR_ON_BIT) | (0 << LCD_BLINK_ON_BIT);
	status = lcd_send_cmd(0x08 | display_control_state); // 0x08 est la base de la commande Display Control
	if(status != HAL_OK) {
		DEBUG_PRINT("lcd_init: Error sending DISPLAY_CONTROL (0x%02X), Status: %d\r\n", (0x08 | display_control_state), status);
		return status;
	}

	status = lcd_send_cmd(LCD_CMD_CLEAR_DISPLAY); // Effacer l'écran
	if(status != HAL_OK) return status;
	// Le délai est géré dans lcd_send_cmd pour Clear/Home

	status = lcd_send_cmd(LCD_CMD_ENTRY_MODE_SET); // Mode d'entrée: Incrémenter curseur, pas de décalage écran
	if(status != HAL_OK) return status;
	// Note: lcd_send_cmd already prints errors if DEBUG_ON is active

	// Initialisation terminée
	DEBUG_PRINT("lcd_init: Initialization successful.\r\n");
	return HAL_OK;


}

/**
 * @brief Positionne le curseur sur le LCD.
 * @param row Ligne du curseur (0-based).
 * @param column Colonne du curseur (0-based).
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_set_cursor(uint8_t row, uint8_t column) {
	DEBUG_PRINT("lcd_set_cursor: Setting cursor to Row %d, Col %d\r\n", row, column);
	if (row >= lcd_config.rows || column >= lcd_config.columns) {
		DEBUG_PRINT("lcd_set_cursor: Error - Invalid position (%d, %d). Max (%d, %d)\r\n", row, column, lcd_config.rows - 1, lcd_config.columns - 1);
		return HAL_ERROR; // Position invalide
	}
	uint8_t address = lcd_config.line_addresses[row] + column;
	DEBUG_PRINT("lcd_set_cursor: Calculated DDRAM address: 0x%02X\r\n", address);
	return lcd_send_cmd(LCD_CMD_SET_DDRAM_ADDR | address);
}

/**
 * @brief Écrit un seul caractère sur le LCD à la position actuelle du curseur.
 * @param ascii_char Caractère ASCII à écrire.
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_write_char(char ascii_char) {
	DEBUG_PRINT("lcd_write_char: Writing char '%c' (0x%02X)\r\n", ascii_char, ascii_char);
	return lcd_send_data_internal(ascii_char); // Utilise la fonction interne qui retourne un statut
}

/**
 * @brief Écrit une chaîne de caractères sur le LCD à partir de la position actuelle du curseur.
 * @param str Chaîne de caractères à écrire.
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_write_string(char *str) {
	DEBUG_PRINT("lcd_write_string: Writing string \"%s\"\r\n", str ? str : "NULL");
	if (str == NULL) {
		DEBUG_PRINT("lcd_write_string: Error - NULL pointer received.\r\n");
		return HAL_ERROR;
	}

	HAL_StatusTypeDef status = HAL_OK;
	while (*str) {
		status = lcd_send_data_internal(*str++); // Utilise la fonction interne
		if(status != HAL_OK) return status; // Arrêter et retourner l'erreur, lcd_send_data_internal already prints error
	}
	DEBUG_PRINT("lcd_write_string: String write finished.\r\n");
	return HAL_OK;
}

/**
 * @brief Efface l'affichage du LCD et replace le curseur en (0,0).
 * @note Cette fonction introduit un délai bloquant (LCD_DELAY_CLEAR_HOME).
 */
HAL_StatusTypeDef lcd_clear(void) {
	DEBUG_PRINT("lcd_clear: Clearing display.\r\n");
	// lcd_send_cmd gère l'erreur en interne mais ne la propage pas ici car void
	// Le délai est aussi géré dans lcd_send_cmd
	return lcd_send_cmd(LCD_CMD_CLEAR_DISPLAY);
}

/**
 * @brief Replace le curseur en position (0,0) sans effacer l'écran.
 * @note Cette fonction introduit un délai bloquant (LCD_DELAY_CLEAR_HOME).
 */
HAL_StatusTypeDef lcd_home(void) {
	DEBUG_PRINT("lcd_home: Returning cursor home.\r\n");
	// Le délai est aussi géré dans lcd_send_cmd
	return lcd_send_cmd(LCD_CMD_RETURN_HOME);
}

/**
 * @brief Contrôle l'état du rétroéclairage.
 * @param state État du rétroéclairage (1 pour allumé, 0 pour éteint).
 * @note L'état prend effet lors de la prochaine transmission I2C. Ne fait pas d'écriture I2C elle-même.
 */
void lcd_backlight(uint8_t state) {
	DEBUG_PRINT("lcd_backlight: Setting backlight state to %d.\r\n", state ? 1 : 0);
	backlight_state = state ? 1 : 0; // Met à jour la variable statique
}

/**
 * @brief Crée un caractère personnalisé dans la CGRAM.
 * @param location Emplacement dans la CGRAM (0-7).
 * @param charmap Tableau de 8 octets définissant le caractère (5x8 pixels).
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_create_char(uint8_t location, uint8_t charmap[8]) {
	DEBUG_PRINT("lcd_create_char: Creating custom char at location %d.\r\n", location & 0x07);
	HAL_StatusTypeDef status = HAL_OK;
	location &= 0x07; // S'assurer que l'emplacement est entre 0 et 7
	status = lcd_send_cmd(LCD_CMD_SET_CGRAM_ADDR | (location << 3)); // Définir l'adresse CGRAM
	if(status != HAL_OK) {
		DEBUG_PRINT("lcd_create_char: Error setting CGRAM address for location %d, Status: %d\r\n", location, status);
		return status;
	}
	for (int i = 0; i < 8; i++) { // Écrire les 8 octets du motif
		DEBUG_PRINT("lcd_create_char: Writing byte %d (0x%02X) for char %d\r\n", i, charmap[i], location);
		status = lcd_send_data_internal(charmap[i]);
		if(status != HAL_OK) {
			DEBUG_PRINT("lcd_create_char: Error writing byte %d for char %d, Status: %d\r\n", i, location, status);
			return status;
		}
	}
	// Il est recommandé de remettre le curseur en DDRAM après avoir écrit en CGRAM
	// status = lcd_send_cmd(LCD_CMD_SET_DDRAM_ADDR); // Retour à l'adresse DDRAM 0x00 par défaut // Removed as per original logic
	return status;
}

/**
 * @brief Affiche un caractère personnalisé préalablement créé.
 * @param location Emplacement du caractère dans la CGRAM (0-7).
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_put_custom_char(uint8_t location) {
	DEBUG_PRINT("lcd_put_custom_char: Writing custom char from location %d.\r\n", location);
	if (location > 7) {
		DEBUG_PRINT("lcd_put_custom_char: Error - Invalid location %d.\r\n", location);
		return HAL_ERROR; // Emplacement invalide
	}
	return lcd_send_data_internal(location); // Écrire l'octet correspondant à l'emplacement
}

/**
 * @brief Envoie des données (un caractère à afficher) au LCD. (Fonction publique)
 * @param data Données (octet du caractère) à envoyer.
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_send_data(uint8_t data) {
	DEBUG_PRINT("lcd_send_data: Sending data byte 0x%02X\r\n", data);
	return lcd_send_data_internal(data); // Appelle la fonction interne et retourne son statut
}

/**
 * @brief Active l'affichage du LCD (si éteint).
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_display_on(void) {
	DEBUG_PRINT("lcd_display_on: Turning display ON.\r\n");
	display_control_state |= (1 << LCD_DISPLAY_ON_BIT);
	return lcd_send_cmd(0x08 | display_control_state);
}

/**
 * @brief Désactive l'affichage du LCD (le contenu reste en mémoire).
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_display_off(void) {
	DEBUG_PRINT("lcd_display_off: Turning display OFF.\r\n");
	display_control_state &= ~(1 << LCD_DISPLAY_ON_BIT);
	return lcd_send_cmd(0x08 | display_control_state);
}

/**
 * @brief Affiche le curseur (souligné).
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_cursor_on(void) {
	DEBUG_PRINT("lcd_cursor_on: Turning cursor ON.\r\n");
	display_control_state |= (1 << LCD_CURSOR_ON_BIT);
	return lcd_send_cmd(0x08 | display_control_state);
}

/**
 * @brief Masque le curseur.
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_cursor_off(void) {
	DEBUG_PRINT("lcd_cursor_off: Turning cursor OFF.\r\n");
	display_control_state &= ~(1 << LCD_CURSOR_ON_BIT);
	return lcd_send_cmd(0x08 | display_control_state);
}

/**
 * @brief Active le clignotement du curseur (bloc).
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_blink_on(void) {
	DEBUG_PRINT("lcd_blink_on: Turning blink ON.\r\n");
    display_control_state |= (1 << LCD_BLINK_ON_BIT);
    return lcd_send_cmd(0x08 | display_control_state);
}

/**
 * @brief Désactive le clignotement du curseur.
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_blink_off(void) {
	DEBUG_PRINT("lcd_blink_off: Turning blink OFF.\r\n");
    display_control_state &= ~(1 << LCD_BLINK_ON_BIT);
    return lcd_send_cmd(0x08 | display_control_state);
}
// etc.

/**
 * @brief Décale tout l'affichage d'une colonne vers la gauche (commande native).
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_scroll_display_left(void) {
	DEBUG_PRINT("lcd_scroll_display_left: Scrolling display left.\r\n");
	return lcd_send_cmd(LCD_CMD_SCROLL_LEFT);
}

/**
 * @brief Décale tout l'affichage d'une colonne vers la droite (commande native).
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
HAL_StatusTypeDef lcd_scroll_display_right(void) {
	DEBUG_PRINT("lcd_scroll_display_right: Scrolling display right.\r\n");
	return lcd_send_cmd(LCD_CMD_SCROLL_RIGHT);
}

// ============================================================================
// Fonctions Privées
// ============================================================================

/**
 * @brief Écrit un demi-octet (4 bits) sur le bus I2C vers le LCD.
 * @param nibble Demi-octet (4 bits supérieurs des données/commandes).
 * @param rs Register Select (0 pour commande, 1 pour donnée).
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
static HAL_StatusTypeDef lcd_write_nibble(uint8_t nibble, uint8_t rs) {
	// DEBUG_PRINT("lcd_write_nibble: Writing nibble 0x%X with RS=%d\r\n", nibble, rs); // Can be very verbose
	HAL_StatusTypeDef status = HAL_OK;
	uint8_t i2c_data;

	// Préparer l'octet I2C: Données sur P4-P7, RS sur P0, E sur P2, BL sur P3
	// RW (P1) est supposé être à la masse (mode écriture seule)
	i2c_data = (nibble << D4_BIT) | (rs << RS_BIT) | (backlight_state << BL_BIT);

	// Générer l'impulsion Enable (E)
	// 1. Mettre E à 1 (les autres bits sont déjà positionnés)
	i2c_data |= (1 << EN_BIT);
	status = HAL_I2C_Master_Transmit(lcd_hi2c, lcd_config.i2c_address << 1, &i2c_data, 1, LCD_I2C_TIMEOUT_MS); // Adresse décalée pour écriture
	if (status != HAL_OK) {
		// DEBUG_PRINT("lcd_write_nibble: I2C Tx Error (E=1), Byte: 0x%02X, Status: %d\r\n", i2c_data, status);
		DEBUG_PRINT("I2C Tx Error (E=1), Status: %d\r\n", status); // Less verbose error
		return status;
	}
	// HAL_Delay(1); // Très court délai si nécessaire (souvent pas requis)
	// __NOP(); // Alternative si délai trop long

	// 2. Mettre E à 0 (le LCD lit les données sur le front descendant de E)
	// Note: On ne modifie que le bit E, les autres bits (RS, BL, Data) restent identiques à l'étape 1
	i2c_data &= ~(1 << EN_BIT);
	status = HAL_I2C_Master_Transmit(lcd_hi2c, lcd_config.i2c_address << 1, &i2c_data, 1, LCD_I2C_TIMEOUT_MS);
	if (status != HAL_OK) {
		// DEBUG_PRINT("lcd_write_nibble: I2C Tx Error (E=0), Byte: 0x%02X, Status: %d\r\n", i2c_data, status);
		DEBUG_PRINT("I2C Tx Error (E=0), Status: %d\r\n", status); // Less verbose error
	}

	// Ajouter un petit délai après l'impulsion E pour laisser le temps au LCD de traiter
	// Ce délai dépend du contrôleur LCD, mais 50us est généralement suffisant.
	// HAL_Delay(1) est trop long ici. Utiliser un délai plus fin si possible.
	// Pour STM32, on peut utiliser DWT_Delay_us() si configuré. Sinon, boucles NOP.
	// Pour simplifier, on peut omettre ce délai fin s'il n'y a pas de problème.

	return status;
}

/**
 * @brief Envoie une commande complète (8 bits) au LCD en mode 4 bits.
 * @param cmd Commande à envoyer.
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
static HAL_StatusTypeDef lcd_send_cmd(uint8_t cmd) {
	DEBUG_PRINT("lcd_send_cmd: Sending command 0x%02X\r\n", cmd);
	HAL_StatusTypeDef status = HAL_OK;
	uint8_t upper_nibble = (cmd >> 4) & 0x0F;
	uint8_t lower_nibble = cmd & 0x0F;

	status = lcd_write_nibble(upper_nibble, 0); // Envoyer nibble haut (RS=0)
	if(status != HAL_OK) {
		DEBUG_PRINT("lcd_send_cmd: Error sending upper nibble (0x%X) for cmd 0x%02X, Status: %d\r\n", upper_nibble, cmd, status);
		return status;
	}

	status = lcd_write_nibble(lower_nibble, 0); // Envoyer nibble bas (RS=0)
	if(status != HAL_OK) {
		DEBUG_PRINT("lcd_send_cmd: Error sending lower nibble (0x%X) for cmd 0x%02X, Status: %d\r\n", lower_nibble, cmd, status);
		return status;
	}

	// Certaines commandes nécessitent un délai plus long
	if (cmd == LCD_CMD_CLEAR_DISPLAY || cmd == LCD_CMD_RETURN_HOME) {
		HAL_Delay(LCD_DELAY_CLEAR_HOME);
	}
	// Un délai court après chaque commande peut améliorer la stabilité sur certains LCDs
	// HAL_Delay(1); // A tester si besoin

	DEBUG_PRINT("lcd_send_cmd: Command 0x%02X sent successfully.\r\n", cmd);
	return status;
}

/**
 * @brief Envoie des données complètes (8 bits) au LCD en mode 4 bits (fonction interne).
 * @param data Données (caractère) à envoyer.
 * @retval HAL_StatusTypeDef Statut de l'opération HAL.
 */
static HAL_StatusTypeDef lcd_send_data_internal(uint8_t data) {
	// DEBUG_PRINT("lcd_send_data_internal: Sending data 0x%02X ('%c')\r\n", data, data); // Can be verbose
	HAL_StatusTypeDef status = HAL_OK;
	uint8_t upper_nibble = (data >> 4) & 0x0F;
	uint8_t lower_nibble = data & 0x0F;

	status = lcd_write_nibble(upper_nibble, 1); // Envoyer nibble haut (RS=1)
	if(status != HAL_OK) {
		DEBUG_PRINT("lcd_send_data_internal: Error sending upper nibble (0x%X) for data 0x%02X, Status: %d\r\n", upper_nibble, data, status);
		return status;
	}

	status = lcd_write_nibble(lower_nibble, 1); // Envoyer nibble bas (RS=1)
	if(status != HAL_OK) {
		DEBUG_PRINT("lcd_send_data_internal: Error sending lower nibble (0x%X) for data 0x%02X, Status: %d\r\n", lower_nibble, data, status);
		// No return here, let the caller handle the final status
	}
	// HAL_Delay(1); // A tester si besoin

	return status;
}
