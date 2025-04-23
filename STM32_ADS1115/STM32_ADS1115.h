/*
 * STM32_ADS1115.h
 *      Author: Adapted from Adafruit ADS1X15 library for STM32 HAL
 */

 #ifndef INC_STM32_ADS1115_H_
 #define INC_STM32_ADS1115_H_
 
 #include "main.h"         // Pour utiliser Error_Handler() si nécessaire, ou gérer les erreurs localement
 #include <stdint.h>       // For uint8_t, uint16_t, int16_t
 #include <stdbool.h>      // For bool type (requires C99 or later)
 #include <limits.h>       // For INT16_MIN
 
 /*=========================================================================
     I2C TIMEOUT (in mS)
     -----------------------------------------------------------------------*/
 #define ADS1115_I2C_TIMEOUT_MS (100) // Timeout pour les opérations I2C
 /*=========================================================================*/
 
 /*=========================================================================
     CONVERSION DELAY (in mS) - Sert de délai minimum
     -----------------------------------------------------------------------*/
 #define ADS1115_MIN_CONVERSIONDELAY (2) // Délai minimum absolu (plus rapide que 860 SPS)
 /*=========================================================================*/
 
 /*=========================================================================
     POINTER REGISTER
     -----------------------------------------------------------------------*/
 #define ADS1115_REG_POINTER_MASK (0x03)      ///< Point mask
 #define ADS1115_REG_POINTER_CONVERT (0x00) ///< Conversion register
 #define ADS1115_REG_POINTER_CONFIG (0x01)  ///< Configuration register
 #define ADS1115_REG_POINTER_LOWTHRESH (0x02) ///< Lo-thresh register
 #define ADS1115_REG_POINTER_HITHRESH (0x03) ///< Hi-thresh register
 /*=========================================================================*/
 
 /*=========================================================================
     CONFIG REGISTER (Les defines restent inchangés)
     -----------------------------------------------------------------------*/
 #define ADS1115_REG_CONFIG_OS_MASK (0x8000) ///< OS Mask
 #define ADS1115_REG_CONFIG_OS_SINGLE (0x8000) ///< Write: Set to start a single-conversion
 #define ADS1115_REG_CONFIG_OS_BUSY (0x0000) ///< Read: Bit = 0 when conversion is in progress
 #define ADS1115_REG_CONFIG_OS_NOTBUSY (0x8000) ///< Read: Bit = 1 when device is not performing a conversion
 #define ADS1115_REG_CONFIG_MUX_MASK (0x7000) ///< Mux Mask
 #define ADS1115_REG_CONFIG_MUX_DIFF_0_1 (0x0000) ///< Differential P = AIN0, N = AIN1 (default)
 #define ADS1115_REG_CONFIG_MUX_DIFF_0_3 (0x1000) ///< Differential P = AIN0, N = AIN3
 #define ADS1115_REG_CONFIG_MUX_DIFF_1_3 (0x2000) ///< Differential P = AIN1, N = AIN3
 #define ADS1115_REG_CONFIG_MUX_DIFF_2_3 (0x3000) ///< Differential P = AIN2, N = AIN3
 #define ADS1115_REG_CONFIG_MUX_SINGLE_0 (0x4000) ///< Single-ended AIN0
 #define ADS1115_REG_CONFIG_MUX_SINGLE_1 (0x5000) ///< Single-ended AIN1
 #define ADS1115_REG_CONFIG_MUX_SINGLE_2 (0x6000) ///< Single-ended AIN2
 #define ADS1115_REG_CONFIG_MUX_SINGLE_3 (0x7000) ///< Single-ended AIN3
 
 #define ADS1115_REG_CONFIG_PGA_MASK (0x0E00)   ///< PGA Mask
 #define ADS1115_REG_CONFIG_PGA_6_144V (0x0000) ///< +/-6.144V range = Gain 2/3
 #define ADS1115_REG_CONFIG_PGA_4_096V (0x0200) ///< +/-4.096V range = Gain 1
 #define ADS1115_REG_CONFIG_PGA_2_048V (0x0400) ///< +/-2.048V range = Gain 2 (default)
 #define ADS1115_REG_CONFIG_PGA_1_024V (0x0600) ///< +/-1.024V range = Gain 4
 #define ADS1115_REG_CONFIG_PGA_0_512V (0x0800) ///< +/-0.512V range = Gain 8
 #define ADS1115_REG_CONFIG_PGA_0_256V (0x0A00) ///< +/-0.256V range = Gain 16
 
 #define ADS1115_REG_CONFIG_MODE_MASK (0x0100)   ///< Mode Mask
 #define ADS1115_REG_CONFIG_MODE_CONTIN (0x0000) ///< Continuous conversion mode
 #define ADS1115_REG_CONFIG_MODE_SINGLE (0x0100) ///< Power-down single-shot mode (default)
 
 #define ADS1115_REG_CONFIG_DR_MASK (0x00E0)   ///< Data Rate Mask
 #define ADS1115_REG_CONFIG_DR_8SPS (0x0000)   ///< 8 samples per second
 #define ADS1115_REG_CONFIG_DR_16SPS (0x0020)  ///< 16 samples per second
 #define ADS1115_REG_CONFIG_DR_32SPS (0x0040)  ///< 32 samples per second
 #define ADS1115_REG_CONFIG_DR_64SPS (0x0060)  ///< 64 samples per second
 #define ADS1115_REG_CONFIG_DR_128SPS (0x0080) ///< 128 samples per second (default)
 #define ADS1115_REG_CONFIG_DR_250SPS (0x00A0) ///< 250 samples per second
 #define ADS1115_REG_CONFIG_DR_475SPS (0x00C0) ///< 475 samples per second
 #define ADS1115_REG_CONFIG_DR_860SPS (0x00E0) ///< 860 samples per second
 
 #define ADS1115_REG_CONFIG_CMODE_MASK (0x0010) ///< CMode Mask
 #define ADS1115_REG_CONFIG_CMODE_TRAD (0x0000) ///< Traditional comparator with hysteresis (default)
 #define ADS1115_REG_CONFIG_CMODE_WINDOW (0x0010) ///< Window comparator
 
 #define ADS1115_REG_CONFIG_CPOL_MASK (0x0008) ///< CPol Mask
 #define ADS1115_REG_CONFIG_CPOL_ACTVLOW (0x0000) ///< ALERT/RDY pin is low when active (default)
 #define ADS1115_REG_CONFIG_CPOL_ACTVHI (0x0008) ///< ALERT/RDY pin is high when active
 
 #define ADS1115_REG_CONFIG_CLAT_MASK (0x0004) ///< CLat Mask
 #define ADS1115_REG_CONFIG_CLAT_NONLAT (0x0000) ///< Non-latching comparator (default)
 #define ADS1115_REG_CONFIG_CLAT_LATCH (0x0004) ///< Latching comparator
 
 #define ADS1115_REG_CONFIG_CQUE_MASK (0x0003) ///< CQue Mask
 #define ADS1115_REG_CONFIG_CQUE_1CONV (0x0000) ///< Assert ALERT/RDY after one conversions
 #define ADS1115_REG_CONFIG_CQUE_2CONV (0x0001) ///< Assert ALERT/RDY after two conversions
 #define ADS1115_REG_CONFIG_CQUE_4CONV (0x0002) ///< Assert ALERT/RDY after four conversions
 #define ADS1115_REG_CONFIG_CQUE_NONE (0x0003) ///< Disable the comparator and put ALERT/RDY in high state (default)
 /*=========================================================================*/
 
 /* Gain settings */
 typedef enum {
   GAIN_TWOTHIRDS = ADS1115_REG_CONFIG_PGA_6_144V, ///< +/- 6.144V range -> 1 bit = 0.1875 mV (limited to VDD +0.3V max!)
   GAIN_ONE = ADS1115_REG_CONFIG_PGA_4_096V,       ///< +/- 4.096V range -> 1 bit = 0.125 mV
   GAIN_TWO = ADS1115_REG_CONFIG_PGA_2_048V,       ///< +/- 2.048V range -> 1 bit = 0.0625 mV
   GAIN_FOUR = ADS1115_REG_CONFIG_PGA_1_024V,      ///< +/- 1.024V range -> 1 bit = 0.03125 mV
   GAIN_EIGHT = ADS1115_REG_CONFIG_PGA_0_512V,     ///< +/- 0.512V range -> 1 bit = 0.016625 mV
   GAIN_SIXTEEN = ADS1115_REG_CONFIG_PGA_0_256V    ///< +/- 0.256V range -> 1 bit = 0.0078125 mV
 } adsGain_t;
 
 #define MAX_ADS1115_MODULES 4 // Nombre maximum de modules pris en charge
 
 // Structure pour stocker l'état de chaque module
 typedef struct {
     uint8_t address;               // Adresse I2C du module (déjà décalée pour HAL)
     adsGain_t gain;                // Gain actuel pour ce module
     uint16_t dataRate;             // Taux d'échantillonnage actuel pour ce module
     bool initialized;              // Indicateur si le module a été ajouté
 } ADS1115_Module;
 
 /**
  * @brief Initialise le pilote ADS1115 globalement (stocke le handle I2C).
  * @param hi2c_dev Pointeur vers la structure HAL I2C handle.
  */
 void ADS1115_init(I2C_HandleTypeDef *hi2c_dev);
 
 /**
  * @brief Ajoute un module ADS1115 avec une adresse spécifique et l'initialise avec des valeurs par défaut.
  * @param address Adresse I2C 7 bits du module (ex: 0x48).
  * @return L'index du module ajouté (0 à MAX_ADS1115_MODULES - 1) ou -1 si le maximum est atteint ou driver non initialisé.
  */
 int ADS1115_addModule(uint8_t address);
 
 /**
  * @brief Sélectionne un module ADS1115 pour les opérations suivantes.
  * @param moduleIndex Index du module (0 à MAX_ADS1115_MODULES - 1).
  * @return true si le module est sélectionné avec succès, false si l'index est invalide ou le module non initialisé.
  */
 bool ADS1115_selectModule(uint8_t moduleIndex);
 
 /**
  * @brief Récupère l'adresse I2C (décalée pour HAL) du module actuellement sélectionné.
  * @return Adresse I2C 8 bits du module sélectionné, ou 0 si aucun module valide n'est sélectionné.
  */
 uint8_t ADS1115_getSelectedModuleAddress(void);
 
 /**
  * @brief Vérifie si le module ADS1115 *actuellement sélectionné* est connecté et réactif.
  * @note Ne configure pas le module. Appeler après ADS1115_selectModule().
  * @return true si la communication est réussie, false en cas d'échec I2C ou si aucun module n'est sélectionné.
  */
 bool ADS1115_begin(void);
 
 /**
  * @brief Définit le gain (et la plage de tension d'entrée) pour le module *actuellement sélectionné*.
  * @note Cette fonction met à jour la configuration en mémoire. Le gain sera appliqué lors de la prochaine écriture de configuration (lecture ADC, etc.).
  * @param gain Réglage du gain à utiliser (depuis l'enum adsGain_t).
  */
 void ADS1115_setGain(adsGain_t gain);
 
 /**
  * @brief Récupère le réglage actuel du gain pour le module *actuellement sélectionné*.
  * @return Le réglage de gain actuel (depuis l'enum adsGain_t), ou GAIN_TWOTHIRDS si aucun module valide n'est sélectionné.
  */
 adsGain_t ADS1115_getGain(void);
 
 /**
  * @brief Définit le taux d'échantillonnage (échantillons par seconde) pour le module *actuellement sélectionné*.
  * @note Cette fonction met à jour la configuration en mémoire. Le taux sera appliqué lors de la prochaine écriture de configuration.
  * @param rate Le taux d'échantillonnage à utiliser (utiliser les constantes ADS1115_REG_CONFIG_DR_*).
  */
 void ADS1115_setDataRate(uint16_t rate);
 
 /**
  * @brief Récupère le réglage actuel du taux d'échantillonnage pour le module *actuellement sélectionné*.
  * @return Le réglage actuel du taux d'échantillonnage (constante ADS1115_REG_CONFIG_DR_*), ou ADS1115_REG_CONFIG_DR_128SPS si aucun module valide n'est sélectionné.
  */
 uint16_t ADS1115_getDataRate(void);
 
 /**
  * @brief Obtient une lecture ADC en mode unique (single-shot) à partir du canal spécifié pour le module *actuellement sélectionné*.
  *        Cette fonction démarre une conversion, attend la fin, et lit le résultat.
  * @param channel Canal ADC à lire (0-3).
  * @return La lecture ADC sous forme d'entier signé 16 bits. Retourne INT16_MIN si le canal est invalide, en cas d'erreur I2C ou de timeout de conversion.
  */
 int16_t ADS1115_readADC_SingleEnded(uint8_t channel);
 
 /**
  * @brief Lit la différence de tension entre AIN0 et AIN1 en mode unique (single-shot) pour le module *actuellement sélectionné*.
  * @return La lecture ADC sous forme d'entier signé 16 bits. Retourne INT16_MIN en cas d'erreur I2C ou de timeout.
  */
 int16_t ADS1115_readADC_Differential_0_1(void);
 
 /**
  * @brief Lit la différence de tension entre AIN0 et AIN3 en mode unique (single-shot) pour le module *actuellement sélectionné*.
  * @return La lecture ADC sous forme d'entier signé 16 bits. Retourne INT16_MIN en cas d'erreur I2C ou de timeout.
  */
 int16_t ADS1115_readADC_Differential_0_3(void);
 
 /**
  * @brief Lit la différence de tension entre AIN1 et AIN3 en mode unique (single-shot) pour le module *actuellement sélectionné*.
  * @return La lecture ADC sous forme d'entier signé 16 bits. Retourne INT16_MIN en cas d'erreur I2C ou de timeout.
  */
 int16_t ADS1115_readADC_Differential_1_3(void);
 
 /**
  * @brief Lit la différence de tension entre AIN2 et AIN3 en mode unique (single-shot) pour le module *actuellement sélectionné*.
  * @return La lecture ADC sous forme d'entier signé 16 bits. Retourne INT16_MIN en cas d'erreur I2C ou de timeout.
  */
 int16_t ADS1115_readADC_Differential_2_3(void);
 
 /**
  * @brief Démarre le mode comparateur pour le module *actuellement sélectionné*.
  *        Place l'ADC en mode de conversion continue. La broche ALERT/RDY s'affirmera selon la configuration.
  * @param channel Canal ADC à utiliser (0-3).
  * @param lowThreshold Seuil bas du comparateur (valeur signée 16 bits).
  * @param highThreshold Seuil haut du comparateur (valeur signée 16 bits).
  * @param compMode Mode du comparateur (ADS1115_REG_CONFIG_CMODE_TRAD ou ADS1115_REG_CONFIG_CMODE_WINDOW).
  * @param compPol Polarité de la broche ALERT/RDY (ADS1115_REG_CONFIG_CPOL_ACTVLOW ou ADS1115_REG_CONFIG_CPOL_ACTVHI).
  * @param compLat Verrouillage du comparateur (ADS1115_REG_CONFIG_CLAT_NONLAT ou ADS1115_REG_CONFIG_CLAT_LATCH).
  * @param compQue File d'attente du comparateur (ADS1115_REG_CONFIG_CQUE_1CONV, _2CONV, _4CONV, ou _NONE pour désactiver).
  * @return true si la configuration a réussi, false en cas d'erreur I2C, de canal invalide ou de paramètres invalides.
  */
 bool ADS1115_startComparator_SingleEnded(uint8_t channel, int16_t lowThreshold, int16_t highThreshold,
                                          uint16_t compMode, uint16_t compPol, uint16_t compLat, uint16_t compQue);
 
 /**
  * @brief Lit les derniers résultats de conversion du module *actuellement sélectionné*.
  *        Utile en mode continu ou pour effacer l'alerte du comparateur en mode verrouillé.
  * @param value Pointeur vers un int16_t où stocker la valeur lue.
  * @return true si la lecture a réussi, false en cas d'erreur I2C ou si aucun module n'est sélectionné.
  *         La valeur pointée par `value` est mise à INT16_MIN en cas d'erreur.
  */
 bool ADS1115_getLastConversionResults(int16_t *value);
 
 /**
  * @brief Lit la dernière valeur convertie lorsque l'ADC est en mode continu.
  * @note Cette fonction suppose que le mode continu a déjà été démarré via `ADS1115_startADCReading(mux, true)`.
  *       Elle ne démarre PAS de nouvelle conversion.
  * @return La dernière valeur ADC lue (entier signé 16 bits). Retourne INT16_MIN si aucun module n'est sélectionné ou en cas d'erreur I2C.
  */
 int16_t ADS1115_readContinuous(void);
 
 /**
  * @brief Calcule la tension en volts en fonction des comptes ADC bruts et du réglage de gain du module *actuellement sélectionné*.
  * @param counts La lecture ADC brute (entier signé 16 bits). Si counts == INT16_MIN (erreur de lecture), retourne 0.0f.
  * @return La tension calculée en volts (float). Retourne 0.0f si aucun module valide n'est sélectionné ou si counts indique une erreur.
  */
 float ADS1115_computeVolts(int16_t counts);
 
 /**
  * @brief Calcule la tension en millivolts en fonction des comptes ADC bruts et du réglage de gain du module *actuellement sélectionné*.
  * @param counts La lecture ADC brute (entier signé 16 bits). Si counts == INT16_MIN (erreur de lecture), retourne 0.0f.
  * @return La tension calculée en millivolts (float). Retourne 0.0f si aucun module valide n'est sélectionné ou si counts indique une erreur.
  */
 float ADS1115_computeMilliVolts(int16_t counts);
 
 /**
  * @brief Démarre une conversion ADC (mode unique ou continu) pour le module *actuellement sélectionné*.
  *        En mode unique, utiliser les fonctions `readADC_...` qui gèrent l'attente.
  *        En mode continu, appeler cette fonction une fois, puis utiliser `ADS1115_readContinuous()` ou
  *        `ADS1115_getLastConversionResults()` pour lire les valeurs.
  *        On peut utiliser `ADS1115_conversionComplete()` ou la broche ALERT/RDY pour savoir quand une nouvelle valeur est prête.
  * @param mux Réglage MUX (utiliser les constantes ADS1115_REG_CONFIG_MUX_*).
  * @param continuous Mettre à true pour le mode continu, false pour le mode unique.
  * @return true si le démarrage a réussi, false en cas d'erreur I2C ou si aucun module n'est sélectionné.
  */
 bool ADS1115_startADCReading(uint16_t mux, bool continuous);
 
 /**
  * @brief Vérifie si une conversion ADC est terminée pour le module *actuellement sélectionné*.
  *        Lit le bit OS du registre de configuration.
  * @return 1 si la conversion est terminée (ou si l'ADC est inactif), 0 si elle est en cours, -1 en cas d'erreur de lecture I2C ou si aucun module n'est sélectionné.
  */
 int8_t ADS1115_conversionComplete(void);
 
 // --- Fonctions I2C de bas niveau (utilisées en interne et potentiellement externe) ---
 
 /**
  * @brief Écrit 16 bits dans le registre de destination spécifié du module *actuellement sélectionné*.
  * @param reg Pointeur de registre (ADS1115_REG_POINTER_*).
  * @param value Valeur à écrire.
  * @return HAL_StatusTypeDef indiquant le résultat de l'opération I2C (HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT).
  */
 HAL_StatusTypeDef ADS1115_writeRegister(uint8_t reg, uint16_t value);
 
 /**
  * @brief Lit 16 bits depuis le registre de destination spécifié du module *actuellement sélectionné*.
  * @param reg Pointeur de registre (ADS1115_REG_POINTER_*).
  * @param value Pointeur vers un uint16_t où stocker la valeur lue.
  * @return HAL_StatusTypeDef indiquant le résultat de l'opération I2C. La valeur pointée par `value` n'est valide que si le retour est HAL_OK.
  *         En cas d'erreur, `*value` est mis à 0.
  */
 HAL_StatusTypeDef ADS1115_readRegister(uint8_t reg, uint16_t *value);
 
 
 #endif /* INC_STM32_ADS1115_H_ */
 
