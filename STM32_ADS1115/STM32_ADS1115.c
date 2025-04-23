/*
 * STM32_ADS1115.c
 *      Auteur : Adapté de la bibliothèque Adafruit ADS1X15 pour STM32 HAL
 */

#include <string.h> // Pour memset
#include <STM32_ADS1115.h>
 
 // --- Variables statiques globales ---
 static I2C_HandleTypeDef *ads_hi2c = NULL; // Pointeur vers le gestionnaire du périphérique I2C
 static ADS1115_Module ads_modules[MAX_ADS1115_MODULES]; // Tableau des modules
 static uint8_t ads_moduleCount = 0;                    // Nombre de modules ajoutés
 static int8_t ads_selectedModule = -1;                 // Index du module actuellement sélectionné (-1 si aucun)
 
 // --- Prototypes des fonctions statiques internes ---
 static int16_t readADC(uint16_t mux_config);
 static uint16_t ADS1115_getDataRateSPS(uint16_t dr_value);
 static bool isSelectedModuleValid(void);
 
 // --- Implémentations des fonctions ---
 
 /**
  * @brief Vérifie si un module valide est actuellement sélectionné.
  */
 static bool isSelectedModuleValid(void) {
     return (ads_selectedModule >= 0 && ads_selectedModule < ads_moduleCount && ads_modules[ads_selectedModule].initialized);
 }
 
 /**
  * @brief Initialise le pilote ADS1115 globalement.
  */
 void ADS1115_init(I2C_HandleTypeDef *hi2c_dev) {
     ads_hi2c = hi2c_dev;
     ads_moduleCount = 0;
     ads_selectedModule = -1;
     // Initialiser le tableau des modules (memset à 0 initialise bool à false)
     memset(ads_modules, 0, sizeof(ads_modules));
     // La boucle for n'est plus nécessaire après memset à 0
     // for (int i = 0; i < MAX_ADS1115_MODULES; ++i) {
     //     ads_modules[i].initialized = false;
     // }
 }
 
 /**
  * @brief Ajoute un module ADS1115 avec l'adresse spécifiée.
  */
 int ADS1115_addModule(uint8_t address) {
     if (ads_hi2c == NULL) {
         return -1; // Driver non initialisé avec ADS1115_init
     }
     if (ads_moduleCount >= MAX_ADS1115_MODULES) {
         return -1; // Trop de modules
     }
 
     uint8_t index = ads_moduleCount;
     ads_modules[index].address = (address << 1); // Adresse 7 bits décalée pour HAL
     ads_modules[index].gain = GAIN_TWOTHIRDS;    // Gain par défaut
     ads_modules[index].dataRate = ADS1115_REG_CONFIG_DR_128SPS; // Taux par défaut
     ads_modules[index].initialized = true;
     ads_moduleCount++;
 
     // Sélectionne automatiquement le premier module ajouté
     if (ads_selectedModule == -1) {
         ADS1115_selectModule(index);
     }
     return index;
 }
 
 /**
  * @brief Sélectionne un module ADS1115 par son index.
  */
 bool ADS1115_selectModule(uint8_t moduleIndex) {
     if (moduleIndex >= ads_moduleCount || !ads_modules[moduleIndex].initialized) {
         ads_selectedModule = -1; // Désélectionner si index invalide
         return false; // Index invalide ou module non initialisé
     }
     ads_selectedModule = moduleIndex;
     return true;
 }
 
 /**
  * @brief Obtient l'adresse du module ADS1115 actuellement sélectionné.
  */
 uint8_t ADS1115_getSelectedModuleAddress(void) {
     if (!isSelectedModuleValid()) {
         return 0; // Retourne 0 si aucun module valide n'est sélectionné
     }
     return ads_modules[ads_selectedModule].address;
 }
 
 
 /**
  * @brief Vérifie si le module ADS1115 *actuellement sélectionné* est connecté et réactif.
  */
 bool ADS1115_begin(void) {
     if (!isSelectedModuleValid()) {
         return false; // Aucun module valide sélectionné
     }
 
     // Essayer de lire le registre de configuration pour vérifier la communication I2C.
     uint16_t config_value;
     HAL_StatusTypeDef status = ADS1115_readRegister(ADS1115_REG_POINTER_CONFIG, &config_value);
 
     return (status == HAL_OK); // Communication réussie si la lecture I2C a fonctionné
 }
 
 /**
  * @brief Définit le gain pour le module *actuellement sélectionné*.
  */
 void ADS1115_setGain(adsGain_t gain) {
     if (isSelectedModuleValid()) {
         ads_modules[ads_selectedModule].gain = gain;
     }
     // Note: Le gain n'est appliqué au registre que lors de la prochaine écriture de config.
 }
 
 /**
  * @brief Récupère le réglage actuel du gain pour le module *actuellement sélectionné*.
  */
 adsGain_t ADS1115_getGain(void) {
      if (isSelectedModuleValid()) {
         return ads_modules[ads_selectedModule].gain;
     }
     return GAIN_TWOTHIRDS; // Valeur par défaut ou d'erreur
 }
 
 /**
  * @brief Définit le taux d'échantillonnage pour le module *actuellement sélectionné*.
  */
 void ADS1115_setDataRate(uint16_t rate) {
     if (isSelectedModuleValid()) {
         // Vérifier si le taux est valide (masquer les bits non pertinents)
         uint16_t valid_rate = rate & ADS1115_REG_CONFIG_DR_MASK;
         // On pourrait ajouter une vérification plus stricte ici si nécessaire
         ads_modules[ads_selectedModule].dataRate = valid_rate;
     }
     // Note: Le taux n'est appliqué au registre que lors de la prochaine écriture de config.
 }
 
 /**
  * @brief Récupère le réglage actuel du taux d'échantillonnage pour le module *actuellement sélectionné*.
  */
 uint16_t ADS1115_getDataRate(void) {
     if (isSelectedModuleValid()) {
         return ads_modules[ads_selectedModule].dataRate;
     }
     return ADS1115_REG_CONFIG_DR_128SPS; // Valeur par défaut ou d'erreur
 }
 
 /**
  * @brief Obtient une lecture ADC en mode simple à partir du canal spécifié.
  */
 int16_t ADS1115_readADC_SingleEnded(uint8_t channel) {
     if (channel > 3 || !isSelectedModuleValid()) {
         return INT16_MIN; // Canal invalide ou module non sélectionné
     }
 
     // Définir les réglages du multiplexeur pour les canaux en mode simple
     uint16_t mux_config;
     switch (channel) {
         case 0: mux_config = ADS1115_REG_CONFIG_MUX_SINGLE_0; break;
         case 1: mux_config = ADS1115_REG_CONFIG_MUX_SINGLE_1; break;
         case 2: mux_config = ADS1115_REG_CONFIG_MUX_SINGLE_2; break;
         case 3: mux_config = ADS1115_REG_CONFIG_MUX_SINGLE_3; break;
         default: return INT16_MIN; // Ne devrait pas arriver
     }
 
     return readADC(mux_config); // Appelle la fonction interne qui gère la lecture single-shot
 }
 
 /**
  * @brief Lit la différence de tension entre AIN0 et AIN1.
  */
 int16_t ADS1115_readADC_Differential_0_1(void) {
     if (!isSelectedModuleValid()) return INT16_MIN;
     return readADC(ADS1115_REG_CONFIG_MUX_DIFF_0_1);
 }
 
 /**
  * @brief Lit la différence de tension entre AIN0 et AIN3.
  */
 int16_t ADS1115_readADC_Differential_0_3(void) {
      if (!isSelectedModuleValid()) return INT16_MIN;
     return readADC(ADS1115_REG_CONFIG_MUX_DIFF_0_3);
 }
 
 /**
  * @brief Lit la différence de tension entre AIN1 et AIN3.
  */
 int16_t ADS1115_readADC_Differential_1_3(void) {
      if (!isSelectedModuleValid()) return INT16_MIN;
     return readADC(ADS1115_REG_CONFIG_MUX_DIFF_1_3);
 }
 
 /**
  * @brief Lit la différence de tension entre AIN2 et AIN3.
  */
 int16_t ADS1115_readADC_Differential_2_3(void) {
      if (!isSelectedModuleValid()) return INT16_MIN;
     return readADC(ADS1115_REG_CONFIG_MUX_DIFF_2_3);
 }
 
 /**
  * @brief Démarre le mode comparateur avec configuration étendue.
  */
 bool ADS1115_startComparator_SingleEnded(uint8_t channel, int16_t lowThreshold, int16_t highThreshold,
                                          uint16_t compMode, uint16_t compPol, uint16_t compLat, uint16_t compQue) {
     if (channel > 3 || !isSelectedModuleValid()) {
         return false; // Canal invalide ou module non sélectionné
     }
 
     // Vérifier la validité des paramètres du comparateur (masquer les bits non pertinents)
     compMode &= ADS1115_REG_CONFIG_CMODE_MASK;
     compPol  &= ADS1115_REG_CONFIG_CPOL_MASK;
     compLat  &= ADS1115_REG_CONFIG_CLAT_MASK;
     compQue  &= ADS1115_REG_CONFIG_CQUE_MASK;
 
     // Récupérer les paramètres du module sélectionné
     adsGain_t current_gain = ads_modules[ads_selectedModule].gain;
     uint16_t current_dataRate = ads_modules[ads_selectedModule].dataRate;
 
     // Définir les réglages du multiplexeur
     uint16_t mux_config;
     switch (channel) {
         case 0: mux_config = ADS1115_REG_CONFIG_MUX_SINGLE_0; break;
         case 1: mux_config = ADS1115_REG_CONFIG_MUX_SINGLE_1; break;
         case 2: mux_config = ADS1115_REG_CONFIG_MUX_SINGLE_2; break;
         case 3: mux_config = ADS1115_REG_CONFIG_MUX_SINGLE_3; break;
         default: return false; // Ne devrait pas arriver
     }
 
     // Construire la configuration du comparateur
     uint16_t config = compQue | compLat | compPol | compMode |
                       ADS1115_REG_CONFIG_MODE_CONTIN; // Le comparateur fonctionne en mode continu
 
     config |= current_gain;     // Utiliser le gain du module
     config |= current_dataRate; // Utiliser le data rate du module
     config |= mux_config;       // Définir le canal
 
     HAL_StatusTypeDef status;
 
     // Écrire les registres de seuil (vérifier chaque écriture)
     status = ADS1115_writeRegister(ADS1115_REG_POINTER_LOWTHRESH, (uint16_t)lowThreshold);
     if (status != HAL_OK) return false;
 
     status = ADS1115_writeRegister(ADS1115_REG_POINTER_HITHRESH, (uint16_t)highThreshold);
     if (status != HAL_OK) return false;
 
     // Écrire le registre de configuration pour démarrer le mode comparateur
     // Note: Le bit OS n'est pas explicitement mis à 1 ici, car le mode continu
     // démarre les conversions automatiquement. Si on voulait forcer une première
     // conversion immédiate, on pourrait ajouter `| ADS1115_REG_CONFIG_OS_SINGLE`
     // mais ce n'est généralement pas nécessaire pour le comparateur.
     status = ADS1115_writeRegister(ADS1115_REG_POINTER_CONFIG, config);
     if (status != HAL_OK) return false;
 
     return true; // Succès
 }
 
 /**
  * @brief Lit les derniers résultats de conversion.
  */
 bool ADS1115_getLastConversionResults(int16_t *value) {
     if (!isSelectedModuleValid() || value == NULL) {
         if(value) *value = INT16_MIN; // Indiquer une erreur dans la valeur
         return false; // Module non sélectionné ou pointeur nul
     }
 
     uint16_t res_uint;
     HAL_StatusTypeDef status = ADS1115_readRegister(ADS1115_REG_POINTER_CONVERT, &res_uint);
 
     if (status == HAL_OK) {
         *value = (int16_t)res_uint;
         return true;
     } else {
         *value = INT16_MIN; // Indiquer une erreur dans la valeur
         return false; // Erreur I2C
     }
 }
 
 /**
  * @brief Lit la dernière valeur convertie lorsque l'ADC est en mode continu.
  */
 int16_t ADS1115_readContinuous(void) {
     int16_t result;
     if (ADS1115_getLastConversionResults(&result)) {
         return result;
     } else {
         // getLastConversionResults met déjà result à INT16_MIN en cas d'erreur
         return INT16_MIN;
     }
 }
 
 /**
  * @brief Calcule la tension en volts.
  */
 float ADS1115_computeVolts(int16_t counts) {
     if (!isSelectedModuleValid()) {
         return 0.0f; // Pas de module sélectionné
     }
     // Si counts est INT16_MIN, cela indique une erreur de lecture précédente.
     if (counts == INT16_MIN) {
         return 0.0f; // Ou retourner NAN si disponible/souhaité
     }
 
     float fsRange;
     adsGain_t current_gain = ads_modules[ads_selectedModule].gain; // Lire le gain du module
 
     switch (current_gain) { // Utiliser le gain du module
         case GAIN_TWOTHIRDS: fsRange = 6.144f; break;
         case GAIN_ONE:       fsRange = 4.096f; break;
         case GAIN_TWO:       fsRange = 2.048f; break;
         case GAIN_FOUR:      fsRange = 1.024f; break;
         case GAIN_EIGHT:     fsRange = 0.512f; break;
         case GAIN_SIXTEEN:   fsRange = 0.256f; break;
         default:             fsRange = 6.144f; break; // Valeur par défaut ou d'erreur
     }
     // L'ADS1115 est sur 16 bits, donc la plage est de -32768 à 32767
     // La plage complète (pleine échelle positive) correspond à 32767 comptes (pas 32768)
     // Donc V = counts * (fsRange / 32767.0)
     // Note: Adafruit utilise 32767.0 dans sa bibliothèque.
     return (float)counts * (fsRange / 32767.0f);
 }
 
 /**
  * @brief Calcule la tension en millivolts.
  */
 float ADS1115_computeMilliVolts(int16_t counts) {
     // Calcule d'abord en volts, gère les erreurs potentielles via computeVolts
     float volts = ADS1115_computeVolts(counts);
     // Si computeVolts retourne 0.0f à cause d'une erreur ou d'une lecture nulle, mV sera aussi 0.0f
     return volts * 1000.0f;
 }
 
 /**
  * @brief Démarre une conversion ADC (mode unique ou continu).
  */
 bool ADS1115_startADCReading(uint16_t mux, bool continuous) {
      if (!isSelectedModuleValid()) {
         return false; // Module non sélectionné
     }
 
     // Récupérer les paramètres du module sélectionné
     adsGain_t current_gain = ads_modules[ads_selectedModule].gain;
     uint16_t current_dataRate = ads_modules[ads_selectedModule].dataRate;
 
     // Commencer avec les bits de configuration par défaut pour le mode non-comparateur
      uint16_t config = ADS1115_REG_CONFIG_CQUE_NONE |    // Désactiver la fonction de comparateur
                       ADS1115_REG_CONFIG_CLAT_NONLAT |  // Non verrouillé
                       ADS1115_REG_CONFIG_CPOL_ACTVLOW | // Alerte/Rdy actif bas (non pertinent si CQUE_NONE)
                       ADS1115_REG_CONFIG_CMODE_TRAD;    // Mode traditionnel (non pertinent si CQUE_NONE)
 
     if (continuous) {
         config |= ADS1115_REG_CONFIG_MODE_CONTIN; // Mode continu
     } else {
         config |= ADS1115_REG_CONFIG_MODE_SINGLE; // Mode unique
     }
 
     config |= current_gain;     // Utiliser le gain du module
     config |= current_dataRate; // Utiliser le data rate du module
     config |= mux;              // Définir la sélection du canal
 
     // Définir le bit OS pour démarrer la conversion (nécessaire pour les deux modes)
     config |= ADS1115_REG_CONFIG_OS_SINGLE;
 
     // Écrire le registre de configuration dans l'ADC
     HAL_StatusTypeDef status = ADS1115_writeRegister(ADS1115_REG_POINTER_CONFIG, config);
 
     return (status == HAL_OK); // Retourne true si l'écriture a réussi
 }
 
 /**
  * @brief Vérifie si une conversion ADC est terminée.
  */
 int8_t ADS1115_conversionComplete(void) {
     if (!isSelectedModuleValid()) {
         return -1; // Module non sélectionné -> Erreur
     }
 
     uint16_t config_value;
     HAL_StatusTypeDef status = ADS1115_readRegister(ADS1115_REG_POINTER_CONFIG, &config_value);
 
     if (status != HAL_OK) {
         return -1; // Erreur I2C
     }
 
     // Vérifier le bit OS (bit 15)
     // OS == 1 signifie que la conversion est terminée/périphérique non occupé
     if ((config_value & ADS1115_REG_CONFIG_OS_MASK) != 0) { // Compare avec OS_NOTBUSY
         return 1; // Conversion terminée
     } else {
         return 0; // Conversion en cours
     }
 }
 
 /**
  * @brief Écrit 16 bits dans le registre de destination spécifié.
  */
 HAL_StatusTypeDef ADS1115_writeRegister(uint8_t reg, uint16_t value) {
     if (!isSelectedModuleValid()) {
         return HAL_ERROR; // Ou un autre code d'erreur approprié
     }
 
     uint8_t data[3];
     data[0] = reg;                     // Octet du registre pointeur
     data[1] = (uint8_t)(value >> 8);   // MSB
     data[2] = (uint8_t)(value & 0xFF); // LSB
 
     return HAL_I2C_Master_Transmit(ads_hi2c, ADS1115_getSelectedModuleAddress(), data, 3, ADS1115_I2C_TIMEOUT_MS);
 }
 
 /**
  * @brief Lit 16 bits depuis le registre de destination spécifié.
  */
 HAL_StatusTypeDef ADS1115_readRegister(uint8_t reg, uint16_t *value) {
     if (!isSelectedModuleValid() || value == NULL) {
          if(value) *value = 0; // Mettre une valeur par défaut en cas d'erreur
         return HAL_ERROR; // Module non sélectionné ou pointeur nul
     }
 
     uint8_t reg_ptr = reg;
     uint8_t data[2] = {0, 0};
     HAL_StatusTypeDef status;
 
     // Étape 1: Envoyer l'adresse du registre à lire
     status = HAL_I2C_Master_Transmit(ads_hi2c, ADS1115_getSelectedModuleAddress(), &reg_ptr, 1, ADS1115_I2C_TIMEOUT_MS);
     if (status != HAL_OK) {
         *value = 0; // Mettre une valeur par défaut en cas d'erreur
         return status; // Retourner l'erreur de transmission
     }
 
     // Étape 2: Lire les 2 octets du registre
     status = HAL_I2C_Master_Receive(ads_hi2c, ADS1115_getSelectedModuleAddress(), data, 2, ADS1115_I2C_TIMEOUT_MS);
     if (status == HAL_OK) {
         *value = ((uint16_t)data[0] << 8) | data[1]; // Combiner les octets si la réception réussit
     } else {
         *value = 0; // Mettre une valeur par défaut en cas d'erreur
     }
 
     return status; // Retourner le statut de la réception
 }
 
 // --- Fonctions utilitaires internes ---
 
 /**
  * @brief Fonction interne pour démarrer, attendre et lire la valeur ADC pour un réglage de multiplexeur donné.
  *        Utilise IMPERATIVEMENT le mode unique (single-shot). Gère les erreurs I2C et timeout.
  *        Utilisée par les fonctions readADC_SingleEnded et readADC_Differential_*.
  */
 static int16_t readADC(uint16_t mux_config) {
     // Démarrer une conversion unique pour le canal donné
     if (!ADS1115_startADCReading(mux_config, false)) { // false = mode unique
         return INT16_MIN; // Erreur I2C lors du démarrage
     }
 
     // Récupérer le data rate actuel pour calculer le timeout
     uint16_t current_dataRate = ads_modules[ads_selectedModule].dataRate;
     uint16_t sps = ADS1115_getDataRateSPS(current_dataRate);
     // Calculer le délai de conversion en ms, ajouter une marge (+2ms) et s'assurer qu'il n'est pas trop court
     uint32_t conversion_delay_ms = 2 + (1000 / sps);
     if (conversion_delay_ms < ADS1115_MIN_CONVERSIONDELAY) {
         conversion_delay_ms = ADS1115_MIN_CONVERSIONDELAY;
     }
     // Définir un timeout légèrement plus long que le temps de conversion attendu (ex: 2x ou +10ms)
     uint32_t timeout_ms = conversion_delay_ms + 10; // Ajout d'une marge fixe
 
 
     // Attendre que la conversion soit terminée en interrogeant le bit OS
     uint32_t start_time = HAL_GetTick();
     int8_t conversion_status;
     while (true) {
         conversion_status = ADS1115_conversionComplete();
 
         if (conversion_status == 1) { // Conversion terminée
             break;
         }
         if (conversion_status == -1) { // Erreur I2C pendant la vérification
              return INT16_MIN;
         }
         // Vérifier le timeout
         uint32_t current_time = HAL_GetTick();
         // Gérer le débordement du compteur HAL_GetTick()
         uint32_t elapsed_time = (current_time >= start_time) ? (current_time - start_time) : (UINT32_MAX - start_time + current_time + 1);
 
         if (elapsed_time > timeout_ms) {
             // Gérer l'erreur de timeout
             // printf("Timeout attente conversion!\r\n"); // Debug
             return INT16_MIN; // Indiquer une erreur de timeout
         }
         // Petit délai optionnel pour ne pas surcharger le CPU/Bus, surtout si le timeout est long
         // HAL_Delay(1); // Peut être nécessaire pour les data rates très lents, mais augmente la latence
     }
 
     // Lire les résultats de la conversion
     int16_t result;
     if (ADS1115_getLastConversionResults(&result)) {
         return result;
     } else {
         return INT16_MIN; // Erreur I2C lors de la lecture du résultat
     }
 }
 
 /**
  * @brief Convertit la valeur du registre Data Rate en Samples Per Second (SPS).
  */
 static uint16_t ADS1115_getDataRateSPS(uint16_t dr_value) {
     // Masquer les bits non pertinents pour être sûr
     dr_value &= ADS1115_REG_CONFIG_DR_MASK;
     switch (dr_value) {
         case ADS1115_REG_CONFIG_DR_8SPS:   return 8;
         case ADS1115_REG_CONFIG_DR_16SPS:  return 16;
         case ADS1115_REG_CONFIG_DR_32SPS:  return 32;
         case ADS1115_REG_CONFIG_DR_64SPS:  return 64;
         case ADS1115_REG_CONFIG_DR_128SPS: return 128;
         case ADS1115_REG_CONFIG_DR_250SPS: return 250;
         case ADS1115_REG_CONFIG_DR_475SPS: return 475;
         case ADS1115_REG_CONFIG_DR_860SPS: return 860;
         default: return 128; // Valeur par défaut si inconnue
     }
 }
 
