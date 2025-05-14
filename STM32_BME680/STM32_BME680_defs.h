#ifndef STM32_BME680_DEFS_H_ // Si STM32_BME680_DEFS_H_ n'est pas défini
#define STM32_BME680_DEFS_H_ // Définir STM32_BME680_DEFS_H_

#include <stdint.h> // Inclure le fichier d'en-tête pour les types entiers standard
#include <stddef.h> // Inclure le fichier d'en-tête pour les définitions standard

#define BME680_POLL_PERIOD_MS UINT8_C(10) // Période de sondage du capteur en millisecondes

#define BME680_I2C_ADDR_PRIMARY UINT8_C(0x76)	// Adresse I2C principale du capteur BME680
#define BME680_I2C_ADDR_SECONDARY UINT8_C(0x77) // Adresse I2C secondaire du capteur BME680

#define BME680_CHIP_ID UINT8_C(0x61) // Identifiant du capteur BME680

#define BME680_COEFF_SIZE UINT8_C(41)	   // Taille des coefficients de calibration
#define BME680_COEFF_ADDR1_LEN UINT8_C(25) // Longueur de la première adresse des coefficients
#define BME680_COEFF_ADDR2_LEN UINT8_C(16) // Longueur de la deuxième adresse des coefficients

#define BME680_FIELD_LENGTH UINT8_C(15)		 // Longueur du champ de données
#define BME680_FIELD_ADDR_OFFSET UINT8_C(17) // Décalage de l'adresse du champ

#define BME680_SOFT_RESET_CMD UINT8_C(0xb6) // Commande de réinitialisation logicielle

#define BME680_OK INT8_C(0)				   // Code de retour pour une opération réussie
#define BME680_E_NULL_PTR INT8_C(-1)	   // Code de retour pour un pointeur nul
#define BME680_E_COM_FAIL INT8_C(-2)	   // Code de retour pour une erreur de communication
#define BME680_E_DEV_NOT_FOUND INT8_C(-3)  // Code de retour pour un périphérique non trouvé
#define BME680_E_INVALID_LENGTH INT8_C(-4) // Code de retour pour une longueur invalide

#define BME680_W_DEFINE_PWR_MODE INT8_C(1) // Avertissement pour définir le mode de puissance
#define BME680_W_NO_NEW_DATA INT8_C(2)	   // Avertissement pour aucune nouvelle donnée

#define BME680_I_MIN_CORRECTION UINT8_C(1) // Correction minimale pour l'humidité
#define BME680_I_MAX_CORRECTION UINT8_C(2) // Correction maximale pour l'humidité

#define BME680_ADDR_RES_HEAT_VAL_ADDR UINT8_C(0x00)	  // Adresse de la valeur de résistance de chauffage
#define BME680_ADDR_RES_HEAT_RANGE_ADDR UINT8_C(0x02) // Adresse de la plage de résistance de chauffage
#define BME680_ADDR_RANGE_SW_ERR_ADDR UINT8_C(0x04)	  // Adresse de l'erreur de plage logicielle
#define BME680_ADDR_SENS_CONF_START UINT8_C(0x5A)	  // Adresse de début de la configuration du capteur
#define BME680_ADDR_GAS_CONF_START UINT8_C(0x64)	  // Adresse de début de la configuration du gaz

#define BME680_FIELD0_ADDR UINT8_C(0x1d) // Adresse du champ 0

#define BME680_RES_HEAT0_ADDR UINT8_C(0x5a) // Adresse de la résistance de chauffage 0
#define BME680_GAS_WAIT0_ADDR UINT8_C(0x64) // Adresse de l'attente du gaz 0

#define BME680_CONF_HEAT_CTRL_ADDR UINT8_C(0x70)	   // Adresse de contrôle de la résistance de chauffage
#define BME680_CONF_ODR_RUN_GAS_NBC_ADDR UINT8_C(0x71) // Adresse de configuration ODR, RUN_GAS, NBC
#define BME680_CONF_OS_H_ADDR UINT8_C(0x72)			   // Adresse de configuration OS_H
#define BME680_CONF_T_P_MODE_ADDR UINT8_C(0x74)		   // Adresse de configuration du mode T_P
#define BME680_CONF_ODR_FILT_ADDR UINT8_C(0x75)		   // Adresse de configuration ODR et filtre

#define BME680_COEFF_ADDR1 UINT8_C(0x89) // Première adresse des coefficients de calibration
#define BME680_COEFF_ADDR2 UINT8_C(0xe1) // Deuxième adresse des coefficients de calibration

#define BME680_CHIP_ID_ADDR UINT8_C(0xd0) // Adresse de l'identifiant du capteur

#define BME680_SOFT_RESET_ADDR UINT8_C(0xe0) // Adresse de la commande de réinitialisation logicielle

#define BME680_ENABLE_HEATER UINT8_C(0x00)	// Activer le chauffage
#define BME680_DISABLE_HEATER UINT8_C(0x08) // Désactiver le chauffage

#define BME680_DISABLE_GAS_MEAS UINT8_C(0x00) // Désactiver la mesure de gaz
#define BME680_ENABLE_GAS_MEAS UINT8_C(0x01)  // Activer la mesure de gaz

#define BME680_OS_NONE UINT8_C(0) // Aucun suréchantillonnage
#define BME680_OS_1X UINT8_C(1)	  // Suréchantillonnage 1x
#define BME680_OS_2X UINT8_C(2)	  // Suréchantillonnage 2x
#define BME680_OS_4X UINT8_C(3)	  // Suréchantillonnage 4x
#define BME680_OS_8X UINT8_C(4)	  // Suréchantillonnage 8x
#define BME680_OS_16X UINT8_C(5)  // Suréchantillonnage 16x

#define BME680_FILTER_SIZE_0 UINT8_C(0)	  // Taille de filtre 0
#define BME680_FILTER_SIZE_1 UINT8_C(1)	  // Taille de filtre 1
#define BME680_FILTER_SIZE_3 UINT8_C(2)	  // Taille de filtre 3
#define BME680_FILTER_SIZE_7 UINT8_C(3)	  // Taille de filtre 7
#define BME680_FILTER_SIZE_15 UINT8_C(4)  // Taille de filtre 15
#define BME680_FILTER_SIZE_31 UINT8_C(5)  // Taille de filtre 31
#define BME680_FILTER_SIZE_63 UINT8_C(6)  // Taille de filtre 63
#define BME680_FILTER_SIZE_127 UINT8_C(7) // Taille de filtre 127

#define BME680_SLEEP_MODE UINT8_C(0)  // Mode veille
#define BME680_FORCED_MODE UINT8_C(1) // Mode forcé

#define BME680_RESET_PERIOD UINT32_C(10) // Période de réinitialisation

#define BME680_HUM_REG_SHIFT_VAL UINT8_C(4) // Décalage pour le registre d'humidité

#define BME680_RUN_GAS_DISABLE UINT8_C(0) // Désactiver l'exécution du gaz
#define BME680_RUN_GAS_ENABLE UINT8_C(1)  // Activer l'exécution du gaz

#define BME680_TMP_BUFFER_LENGTH UINT8_C(40)  // Longueur du tampon temporaire
#define BME680_REG_BUFFER_LENGTH UINT8_C(6)	  // Longueur du tampon de registre
#define BME680_FIELD_DATA_LENGTH UINT8_C(3)	  // Longueur des données de champ
#define BME680_GAS_REG_BUF_LENGTH UINT8_C(20) // Longueur du tampon de registre de gaz

#define BME680_OST_SEL UINT16_C(1)															 // Sélecteur de température
#define BME680_OSP_SEL UINT16_C(2)															 // Sélecteur de pression
#define BME680_OSH_SEL UINT16_C(4)															 // Sélecteur d'humidité
#define BME680_GAS_MEAS_SEL UINT16_C(8)														 // Sélecteur de mesure de gaz
#define BME680_FILTER_SEL UINT16_C(16)														 // Sélecteur de filtre
#define BME680_HCNTRL_SEL UINT16_C(32)														 // Sélecteur de contrôle de chauffage
#define BME680_RUN_GAS_SEL UINT16_C(64)														 // Sélecteur d'exécution de gaz
#define BME680_NBCONV_SEL UINT16_C(128)														 // Sélecteur de nombre de conversions
#define BME680_GAS_SENSOR_SEL (BME680_GAS_MEAS_SEL | BME680_RUN_GAS_SEL | BME680_NBCONV_SEL) // Sélecteur de capteur de gaz

#define BME680_NBCONV_MIN UINT8_C(0)  // Nombre minimal de conversions
#define BME680_NBCONV_MAX UINT8_C(10) // Nombre maximal de conversions

#define BME680_GAS_MEAS_MSK UINT8_C(0x30)	 // Masque de mesure de gaz
#define BME680_NBCONV_MSK UINT8_C(0X0F)		 // Masque de nombre de conversions
#define BME680_FILTER_MSK UINT8_C(0X1C)		 // Masque de filtre
#define BME680_OST_MSK UINT8_C(0XE0)		 // Masque de température
#define BME680_OSP_MSK UINT8_C(0X1C)		 // Masque de pression
#define BME680_OSH_MSK UINT8_C(0X07)		 // Masque d'humidité
#define BME680_HCTRL_MSK UINT8_C(0x08)		 // Masque de contrôle de chauffage
#define BME680_RUN_GAS_MSK UINT8_C(0x10)	 // Masque d'exécution de gaz
#define BME680_MODE_MSK UINT8_C(0x03)		 // Masque de mode
#define BME680_RHRANGE_MSK UINT8_C(0x30)	 // Masque de plage de résistance de chauffage
#define BME680_RSERROR_MSK UINT8_C(0xf0)	 // Masque d'erreur de plage de résistance
#define BME680_NEW_DATA_MSK UINT8_C(0x80)	 // Masque de nouvelle donnée
#define BME680_GAS_INDEX_MSK UINT8_C(0x0f)	 // Masque d'index de gaz
#define BME680_GAS_RANGE_MSK UINT8_C(0x0f)	 // Masque de plage de gaz
#define BME680_GASM_VALID_MSK UINT8_C(0x20)	 // Masque de validité de mesure de gaz
#define BME680_HEAT_STAB_MSK UINT8_C(0x10)	 // Masque de stabilité de chauffage
#define BME680_BIT_H1_DATA_MSK UINT8_C(0x0F) // Masque de données H1

#define BME680_GAS_MEAS_POS UINT8_C(4) // Position de mesure de gaz
#define BME680_FILTER_POS UINT8_C(2)   // Position de filtre
#define BME680_OST_POS UINT8_C(5)	   // Position de température
#define BME680_OSP_POS UINT8_C(2)	   // Position de pression
#define BME680_RUN_GAS_POS UINT8_C(4)  // Position d'exécution de gaz

#define BME680_T2_LSB_REG (1)	// Registre LSB de T2
#define BME680_T2_MSB_REG (2)	// Registre MSB de T2
#define BME680_T3_REG (3)		// Registre T3
#define BME680_P1_LSB_REG (5)	// Registre LSB de P1
#define BME680_P1_MSB_REG (6)	// Registre MSB de P1
#define BME680_P2_LSB_REG (7)	// Registre LSB de P2
#define BME680_P2_MSB_REG (8)	// Registre MSB de P2
#define BME680_P3_REG (9)		// Registre P3
#define BME680_P4_LSB_REG (11)	// Registre LSB de P4
#define BME680_P4_MSB_REG (12)	// Registre MSB de P4
#define BME680_P5_LSB_REG (13)	// Registre LSB de P5
#define BME680_P5_MSB_REG (14)	// Registre MSB de P5
#define BME680_P6_REG (16)		// Registre P6
#define BME680_P7_REG (15)		// Registre P7
#define BME680_P8_LSB_REG (19)	// Registre LSB de P8
#define BME680_P8_MSB_REG (20)	// Registre MSB de P8
#define BME680_P9_LSB_REG (21)	// Registre LSB de P9
#define BME680_P9_MSB_REG (22)	// Registre MSB de P9
#define BME680_P10_REG (23)		// Registre P10
#define BME680_H2_MSB_REG (25)	// Registre MSB de H2
#define BME680_H2_LSB_REG (26)	// Registre LSB de H2
#define BME680_H1_LSB_REG (26)	// Registre LSB de H1
#define BME680_H1_MSB_REG (27)	// Registre MSB de H1
#define BME680_H3_REG (28)		// Registre H3
#define BME680_H4_REG (29)		// Registre H4
#define BME680_H5_REG (30)		// Registre H5
#define BME680_H6_REG (31)		// Registre H6
#define BME680_H7_REG (32)		// Registre H7
#define BME680_T1_LSB_REG (33)	// Registre LSB de T1
#define BME680_T1_MSB_REG (34)	// Registre MSB de T1
#define BME680_GH2_LSB_REG (35) // Registre LSB de GH2
#define BME680_GH2_MSB_REG (36) // Registre MSB de GH2
#define BME680_GH1_REG (37)		// Registre GH1
#define BME680_GH3_REG (38)		// Registre GH3

#define BME680_REG_FILTER_INDEX UINT8_C(5)	// Index de filtre
#define BME680_REG_TEMP_INDEX UINT8_C(4)	// Index de température
#define BME680_REG_PRES_INDEX UINT8_C(4)	// Index de pression
#define BME680_REG_HUM_INDEX UINT8_C(2)		// Index d'humidité
#define BME680_REG_NBCONV_INDEX UINT8_C(1)	// Index de nombre de conversions
#define BME680_REG_RUN_GAS_INDEX UINT8_C(1) // Index d'exécution de gaz
#define BME680_REG_HCTRL_INDEX UINT8_C(0)	// Index de contrôle de chauffage

#define BME680_MAX_OVERFLOW_VAL INT32_C(0x40000000) // Valeur maximale de débordement

#define BME680_CONCAT_BYTES(msb, lsb) (((uint16_t)msb << 8) | (uint16_t)lsb) // Concaténer les octets MSB et LSB

#define BME680_SET_BITS(reg_data, bitname, data) ((reg_data & ~(bitname##_MSK)) | ((data << bitname##_POS) & bitname##_MSK)) // Définir les bits
#define BME680_GET_BITS(reg_data, bitname) ((reg_data & (bitname##_MSK)) >> (bitname##_POS))								 // Obtenir les bits

#define BME680_SET_BITS_POS_0(reg_data, bitname, data) ((reg_data & ~(bitname##_MSK)) | (data & bitname##_MSK)) // Définir les bits à la position 0
#define BME680_GET_BITS_POS_0(reg_data, bitname) (reg_data & (bitname##_MSK))									// Obtenir les bits à la position 0

typedef int8_t (*bme680_com_fptr_t)(uint8_t dev_id, uint8_t reg_addr, uint8_t *data, uint16_t len); // Pointeur de fonction pour la communication

typedef void (*bme680_delay_fptr_t)(uint32_t period); // Pointeur de fonction pour le délai

enum bme680_intf // Énumération pour l'interface
{
	BME680_I2C_INTF // Interface I2C
};

struct bme680_field_data // Structure pour les données de champ
{
	uint8_t status;		// Statut
	uint8_t gas_index;	// Index de gaz
	uint8_t meas_index; // Index de mesure

	float temperature;	  // Température
	float pressure;		  // Pression
	float humidity;		  // Humidité
	float gas_resistance; // Résistance du gaz
};

struct bme680_calib_data // Structure pour les données de calibration
{
	uint16_t par_h1; // Paramètre H1
	uint16_t par_h2; // Paramètre H2
	int8_t par_h3;	 // Paramètre H3
	int8_t par_h4;	 // Paramètre H4
	int8_t par_h5;	 // Paramètre H5
	uint8_t par_h6;	 // Paramètre H6
	int8_t par_h7;	 // Paramètre H7
	int8_t par_gh1;	 // Paramètre GH1
	int16_t par_gh2; // Paramètre GH2
	int8_t par_gh3;	 // Paramètre GH3
	uint16_t par_t1; // Paramètre T1
	int16_t par_t2;	 // Paramètre T2
	int8_t par_t3;	 // Paramètre T3
	uint16_t par_p1; // Paramètre P1
	int16_t par_p2;	 // Paramètre P2
	int8_t par_p3;	 // Paramètre P3
	int16_t par_p4;	 // Paramètre P4
	int16_t par_p5;	 // Paramètre P5
	int8_t par_p6;	 // Paramètre P6
	int8_t par_p7;	 // Paramètre P7
	int16_t par_p8;	 // Paramètre P8
	int16_t par_p9;	 // Paramètre P9
	uint8_t par_p10; // Paramètre P10

	float t_fine;			// Température fine
	uint8_t res_heat_range; // Plage de résistance de chauffage
	int8_t res_heat_val;	// Valeur de résistance de chauffage
	int8_t range_sw_err;	// Erreur de plage logicielle
};

struct bme680_tph_sett // Structure pour les paramètres de température, pression, humidité
{
	uint8_t os_hum;	 // Suréchantillonnage d'humidité
	uint8_t os_temp; // Suréchantillonnage de température
	uint8_t os_pres; // Suréchantillonnage de pression
	uint8_t filter;	 // Filtre
};

struct bme680_gas_sett // Structure pour les paramètres de gaz
{
	uint8_t nb_conv;	 // Nombre de conversions
	uint8_t heatr_ctrl;	 // Contrôle de chauffage
	uint8_t run_gas;	 // Exécution de gaz
	uint16_t heatr_temp; // Température de chauffage
	uint16_t heatr_dur;	 // Durée de chauffage
};

struct bme680_dev // Structure pour le périphérique BME680
{
	uint8_t chip_id;				 // Identifiant du capteur
	uint8_t dev_id;					 // Identifiant du périphérique
	enum bme680_intf intf;			 // Interface
	uint8_t mem_page;				 // Page mémoire
	int8_t amb_temp;				 // Température ambiante
	struct bme680_calib_data calib;	 // Données de calibration
	struct bme680_tph_sett tph_sett; // Paramètres de température, pression, humidité
	struct bme680_gas_sett gas_sett; // Paramètres de gaz
	uint8_t power_mode;				 // Mode de puissance
	uint8_t new_fields;				 // Nouveaux champs
	uint8_t info_msg;				 // Message d'information
	bme680_com_fptr_t read;			 // Fonction de lecture
	bme680_com_fptr_t write;		 // Fonction d'écriture
	bme680_delay_fptr_t delay_ms;	 // Fonction de délai
	int8_t com_status;				 // Statut de communication
	I2C_HandleTypeDef *hi2c;		 // Pointeur vers le gestionnaire I2C
};

#endif // Fin de la définition de STM32_BME680_DEFS_H_
