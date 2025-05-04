#include "STM32_BMP085_180.h" // Inclut le fichier d'en-tête pour les fonctions du capteur BMP085

// Cette librairie est conçue pour être compatible avec les capteurs Bosch BMP085 et BMP180.
// Ces deux capteurs partagent le même ID de puce (0x55) et une interface I2C similaire.

#ifdef DEBUG_ON
    #include <stdio.h> // Inclure stdio.h pour utiliser printf
    #define DEBUG_PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__) // Corriger la définition
#else
    #define DEBUG_PRINT(fmt, ...) // Ne rien faire si DEBUG n'est pas défini
#endif

// Registres de données de calibration
#define BMP_CAL_AC1 0xAA //!< R   Données de calibration (16 bits)
#define BMP_CAL_AC2 0xAC //!< R   Données de calibration (16 bits)
#define BMP_CAL_AC3 0xAE //!< R   Données de calibration (16 bits)
#define BMP_CAL_AC4 0xB0 //!< R   Données de calibration (16 bits)
#define BMP_CAL_AC5 0xB2 //!< R   Données de calibration (16 bits)
#define BMP_CAL_AC6 0xB4 //!< R   Données de calibration (16 bits)
#define BMP_CAL_B1 0xB6  //!< R   Données de calibration (16 bits)
#define BMP_CAL_B2 0xB8  //!< R   Données de calibration (16 bits)
#define BMP_CAL_MB 0xBA  //!< R   Données de calibration (16 bits)
#define BMP_CAL_MC 0xBC  //!< R   Données de calibration (16 bits)
#define BMP_CAL_MD 0xBE  //!< R   Données de calibration (16 bits)

// Commandes
#define BMP_CHIP_ID_REG 0xD0     //!< Registre contenant l'ID de la puce (devrait être 0x55)
#define BMP_CONTROL 0xF4         //!< Registre de contrôle
#define BMP_TEMPDATA 0xF6        //!< Registre des données de température
#define BMP_PRESSUREDATA 0xF6    //!< Registre des données de pression
#define BMP_READTEMPCMD 0x2E     //!< Commande pour lire la température
#define BMP_READPRESSURECMD 0x34 //!< Commande pour lire la pression

// Délais de conversion en ms selon le mode
#define BMP_TEMP_CONVERSION_DELAY 5
#define BMP_PRES_CONVERSION_DELAY_ULP 5
#define BMP_PRES_CONVERSION_DELAY_STD 8
#define BMP_PRES_CONVERSION_DELAY_HR 14
#define BMP_PRES_CONVERSION_DELAY_UHR 26
#define BMP_I2C_TIMEOUT 100 // Timeout pour les opérations I2C en ms

// --- Fonctions statiques internes ---

// Fonctions de gestion I2C
// Retourne BMP_OK en cas de succès, ou un code d'erreur BMP_Status_t
static BMP_Status_t readBytes(BMP_Handle_t *bmp, uint8_t regAddr, uint8_t *buffer, uint8_t len)
{
    if (bmp == NULL || bmp->i2cHandle == NULL || buffer == NULL) {
        DEBUG_PRINT("readBytes Error: NULL pointer\r\n");
        return BMP_ERR_NULL_PTR;
    }

    DEBUG_PRINT("readBytes: Reading %d byte(s) from reg 0x%02X...\r\n", len, regAddr);
    if (HAL_I2C_Master_Transmit(bmp->i2cHandle, bmp->i2cAddr8bit, &regAddr, 1, BMP_I2C_TIMEOUT) != HAL_OK) {
        bmp->lastError = BMP_ERR_I2C_TX;
        DEBUG_PRINT("readBytes Error: I2C Transmit failed\r\n");
        return BMP_ERR_I2C_TX;
    }
    if (HAL_I2C_Master_Receive(bmp->i2cHandle, bmp->i2cAddr8bit, buffer, len, BMP_I2C_TIMEOUT) != HAL_OK) {
        bmp->lastError = BMP_ERR_I2C_RX;
        DEBUG_PRINT("readBytes Error: I2C Receive failed\r\n");
        return BMP_ERR_I2C_RX;
    }
    return BMP_OK;
}

// Retourne BMP_OK en cas de succès. Met la valeur lue dans *value
static BMP_Status_t read8(BMP_Handle_t *bmp, uint8_t regAddr, uint8_t *value) // Lit un octet depuis un registre
{
    // Passe maintenant le handle bmp à readBytes
    DEBUG_PRINT("read8: Reading from reg 0x%02X...\r\n", regAddr);
    return readBytes(bmp, regAddr, value, 1);
}

// Retourne BMP_OK en cas de succès. Met la valeur lue dans *value
static BMP_Status_t read16(BMP_Handle_t *bmp, uint8_t regAddr, uint16_t *value) // Lit deux octets depuis un registre
{
    if (bmp == NULL || value == NULL) return BMP_ERR_NULL_PTR;
    DEBUG_PRINT("read16: Reading from reg 0x%02X...\r\n", regAddr);
    uint8_t retbuf[2]; // Tampon pour stocker les deux octets
    BMP_Status_t status = readBytes(bmp, regAddr, retbuf, 2);
    if (status != BMP_OK) {
        DEBUG_PRINT("read16 Error: readBytes failed with status %d\r\n", status);
        return status; // Erreur
    }
    *value = ((uint16_t)retbuf[0] << 8) | retbuf[1]; // Combine les octets en un entier 16 bits (MSB first)
    #ifdef DEBUG_VERBOSE // Optionnel: Afficher la valeur lue si très verbeux
    DEBUG_PRINT("read16: Value read = 0x%04X (%d)\r\n", *value, *value);
    #endif
    return BMP_OK; // Succès
}

// Retourne BMP_OK en cas de succès
static BMP_Status_t write8(BMP_Handle_t *bmp, uint8_t regAddr, uint8_t data) // Écrit un octet dans un registre
{
    uint8_t tBuf[2]; // Tampon pour l'adresse et la donnée
    if (bmp == NULL) return BMP_ERR_NULL_PTR;
    DEBUG_PRINT("write8: Writing 0x%02X to reg 0x%02X...\r\n", data, regAddr);
    tBuf[0] = regAddr; // Adresse du registre
    tBuf[1] = data; // Donnée à écrire
    if (HAL_I2C_Master_Transmit(bmp->i2cHandle, bmp->i2cAddr8bit, tBuf, 2, BMP_I2C_TIMEOUT) != HAL_OK) {
        bmp->lastError = BMP_ERR_I2C_TX;
        return BMP_ERR_I2C_TX; // Erreur
        DEBUG_PRINT("write8 Error: I2C Transmit failed\r\n");
    }
    return BMP_OK; // Succès
}

// --- Fonctions Publiques ---

BMP_Status_t BMP_Init(BMP_Handle_t *bmp, uint8_t mode, I2C_HandleTypeDef *i2cdev, uint8_t i2c_address_7bit)
{
  DEBUG_PRINT("BMP_Init: Starting initialization...\r\n");
  if (bmp == NULL || i2cdev == NULL) {
    // Ne peut pas stocker l'erreur dans bmp->lastError car bmp peut être NULL
    DEBUG_PRINT("BMP_Init Error: NULL pointer provided\r\n");
    return BMP_ERR_NULL_PTR;
  }

  bmp->i2cHandle = i2cdev; // Associe le périphérique I2C
  bmp->i2cAddr8bit = i2c_address_7bit << 1; // Configure l'adresse I2C (8 bits)
  bmp->lastError = BMP_OK; // Initialise le statut d'erreur

  if (mode > BMP_ULTRAHIGHRES) { // Vérifie si le mode est valide
    DEBUG_PRINT("BMP_Init Warning: Invalid mode (%d), using BMP_ULTRAHIGHRES\r\n", mode);
    mode = BMP_ULTRAHIGHRES; // Définit le mode au maximum si invalide
  }
  bmp->oversampling = mode; // Définit le niveau de suréchantillonnage
  DEBUG_PRINT("BMP_Init: Oversampling mode set to %d\r\n", bmp->oversampling);

  uint8_t chipId = 0;
  BMP_Status_t status = read8(bmp, BMP_CHIP_ID_REG, &chipId);
  if (status != BMP_OK || chipId != 0x55) { // Vérifie l'identifiant du capteur (0x55 pour BMP085/BMP180) et l'erreur I2C
    DEBUG_PRINT("BMP_Init Error: Chip ID read failed (status %d) or incorrect ID (0x%02X)\r\n", status, chipId);
    // printf("Erreur : Capteur BMP non détecté ou ID incorrect (ID lu : 0x%X)\r\n", chipId); // Gardé pour l'utilisateur final
    // printf("Assurez-vous qu'un BMP085 ou BMP180 est connecté à l'adresse 0x%X.\r\n", i2c_address_7bit); // Gardé pour l'utilisateur final
    bmp->lastError = (status != BMP_OK) ? status : BMP_ERR_INVALID_ID;
    return bmp->lastError;
  }
  printf("Capteur détecté : BMP085/BMP180 avec ID 0x%X\r\n", chipId);
  DEBUG_PRINT("BMP_Init: Chip ID OK (0x%02X)\r\n", chipId);

  /* Lit les données de calibration */
  // Vérifier chaque lecture de calibration pour les erreurs I2C
  // Correction: read16 prend bmp, regAddr, value*
  #define CHECK_CAL_READ(reg, var_ptr) if ((status = read16(bmp, reg, (uint16_t*)var_ptr)) != BMP_OK) { bmp->lastError = status; return status; }
  CHECK_CAL_READ(BMP_CAL_AC1, &bmp->ac1);
  CHECK_CAL_READ(BMP_CAL_AC2, &bmp->ac2);
  CHECK_CAL_READ(BMP_CAL_AC3, &bmp->ac3);
  CHECK_CAL_READ(BMP_CAL_AC4, &bmp->ac4);
  CHECK_CAL_READ(BMP_CAL_AC5, &bmp->ac5);
  CHECK_CAL_READ(BMP_CAL_AC6, &bmp->ac6);

  CHECK_CAL_READ(BMP_CAL_B1, &bmp->b1);
  // Suppression des anciennes lignes if (read16(...) != 0)
  CHECK_CAL_READ(BMP_CAL_B2, &bmp->b2);

  CHECK_CAL_READ(BMP_CAL_MB, &bmp->mb);
  CHECK_CAL_READ(BMP_CAL_MC, &bmp->mc);
  CHECK_CAL_READ(BMP_CAL_MD, &bmp->md);
  #undef CHECK_CAL_READ

  DEBUG_PRINT("BMP_Init: Calibration data read successfully.\r\n");
  #ifdef DEBUG_VERBOSE // Optionnel: Afficher les coeffs de calibration si très verbeux
  DEBUG_PRINT("  AC1=%d AC2=%d AC3=%d AC4=%u AC5=%u AC6=%u\r\n", bmp->ac1, bmp->ac2, bmp->ac3, bmp->ac4, bmp->ac5, bmp->ac6);
  DEBUG_PRINT("  B1=%d B2=%d MB=%d MC=%d MD=%d\r\n", bmp->b1, bmp->b2, bmp->mb, bmp->mc, bmp->md);
  #endif

  DEBUG_PRINT("BMP_Init: Initialization finished successfully.\r\n");
  return BMP_OK; // Initialisation réussie
}

// Initialise le capteur BMP en utilisant le mode par défaut défini par BMP_DEFAULT_MODE
BMP_Status_t BMP_Init_Default(BMP_Handle_t *bmp, I2C_HandleTypeDef *i2cdev, uint8_t i2c_address_7bit)
{
    DEBUG_PRINT("BMP_Init_Default: Starting initialization with default mode (%d)...\r\n", BMP_DEFAULT_MODE);
    // Vérifie les pointeurs comme dans BMP_Init
    if (bmp == NULL || i2cdev == NULL) {
        DEBUG_PRINT("BMP_Init_Default Error: NULL pointer provided\r\n");
        return BMP_ERR_NULL_PTR;
    }
    // Appelle la fonction d'initialisation principale avec le mode par défaut
    BMP_Status_t status = BMP_Init(bmp, BMP_DEFAULT_MODE, i2cdev, i2c_address_7bit);
    DEBUG_PRINT("BMP_Init_Default: Finished with status %d.\r\n", status);
    return status;
}

// --- Fonctions de calcul internes (utilisent le handle) ---

// Calcule la valeur B5 pour la température. Retourne B5 ou 0 en cas d'erreur.
static int32_t computeB5(BMP_Handle_t *bmp, int32_t UT)
{
  if (bmp == NULL) {
      DEBUG_PRINT("computeB5 Error: NULL pointer\r\n");
      return 0; // Ne peut pas définir lastError
  }

  int32_t X1 = (UT - (int32_t)bmp->ac6) * ((int32_t)bmp->ac5) >> 15;
  int32_t denom = X1 + (int32_t)bmp->md;
  if (denom == 0) {
      bmp->lastError = BMP_ERR_MATH;
      DEBUG_PRINT("computeB5 Error: Division by zero (X1 + md == 0)\r\n");
      return 0; // Erreur de division par zéro potentielle
  }
  int32_t X2 = ((int32_t)bmp->mc << 11) / denom;
  return X1 + X2; // Retourne la somme de X1 et X2
}

// Retourne BMP_OK en cas de succès. Met la valeur lue dans *ut
static BMP_Status_t readRawTemperature(BMP_Handle_t *bmp, int32_t *ut) // Lit la température brute
{
  if (bmp == NULL || ut == NULL) {
      DEBUG_PRINT("readRawTemperature Error: NULL pointer\r\n");
      return BMP_ERR_NULL_PTR;
  }

  DEBUG_PRINT("readRawTemperature: Requesting temperature conversion...\r\n");
  BMP_Status_t status = write8(bmp, BMP_CONTROL, BMP_READTEMPCMD);
  if (status != BMP_OK) return status; // Erreur I2C
  DEBUG_PRINT("readRawTemperature: Waiting %d ms for conversion...\r\n", BMP_TEMP_CONVERSION_DELAY);
  HAL_Delay(BMP_TEMP_CONVERSION_DELAY); // Attend la conversion
  uint16_t raw_temp;
  // Correction: Appel à read16 avec les bons arguments
  DEBUG_PRINT("readRawTemperature: Reading raw temperature value...\r\n");
  status = read16(bmp, BMP_TEMPDATA, &raw_temp);
  if (status != BMP_OK) return status; // Erreur I2C
  *ut = raw_temp;
  DEBUG_PRINT("readRawTemperature: Raw temperature = %ld\r\n", *ut);
  return BMP_OK; // Succès
}

// Retourne BMP_OK en cas de succès. Met la valeur lue dans *up
static BMP_Status_t readRawPressure(BMP_Handle_t *bmp, int32_t *up) // Lit la pression brute
{
  uint16_t raw_pres_h;
  uint8_t  raw_pres_l;
  uint32_t delay_ms;
  // Suppression de l'ancien appel write8 incorrect

  if (bmp == NULL || up == NULL) {
      DEBUG_PRINT("readRawPressure Error: NULL pointer\r\n");
      return BMP_ERR_NULL_PTR;
  }

  DEBUG_PRINT("readRawPressure: Requesting pressure conversion (OSS=%d)...\r\n", bmp->oversampling);
  BMP_Status_t status = write8(bmp, BMP_CONTROL, BMP_READPRESSURECMD + (bmp->oversampling << 6));
  if (status != BMP_OK) return status;

  // Choix du délai en fonction du mode
  if (bmp->oversampling == BMP_ULTRALOWPOWER) // Vérifie le niveau de suréchantillonnage
    delay_ms = BMP_PRES_CONVERSION_DELAY_ULP;
  else if (bmp->oversampling == BMP_STANDARD)
    delay_ms = BMP_PRES_CONVERSION_DELAY_STD;
  // Correction: Utilisation de bmp->oversampling
  else if (bmp->oversampling == BMP_HIGHRES)
    delay_ms = BMP_PRES_CONVERSION_DELAY_HR;
  else
    delay_ms = BMP_PRES_CONVERSION_DELAY_UHR;

  DEBUG_PRINT("readRawPressure: Waiting %ld ms for conversion...\r\n", delay_ms);
  HAL_Delay(delay_ms);

  DEBUG_PRINT("readRawPressure: Reading raw pressure value...\r\n");
  status = read16(bmp, BMP_PRESSUREDATA, &raw_pres_h); // Lit les deux premiers octets de la pression
  // Correction: Appel à read16 avec les bons arguments (déjà correct ici)
  if (status != BMP_OK) return status;
  status = read8(bmp, BMP_PRESSUREDATA + 2, &raw_pres_l); // Lit le troisième octet
  // Correction: Appel à read8 avec les bons arguments (déjà correct ici)
  if (status != BMP_OK) return status;
  DEBUG_PRINT("readRawPressure: Raw values read (MSB=0x%04X, LSB=0x%02X)\r\n", raw_pres_h, raw_pres_l);

  *up = ((uint32_t)raw_pres_h << 8) | raw_pres_l;
  *up >>= (8 - bmp->oversampling); // Ajuste selon le niveau de suréchantillonnage
  DEBUG_PRINT("readRawPressure: Raw pressure (adjusted) = %ld\r\n", *up);

  return BMP_OK; // Succès
}

// --- Fonctions de lecture publiques ---

// Retourne BMP_OK en cas de succès. Met la valeur lue dans *temp_degC
BMP_Status_t BMP_readTemperature(BMP_Handle_t *bmp, float *temp_degC) // Calcule et retourne la température en °C
{
  if (bmp == NULL || temp_degC == NULL) {
      DEBUG_PRINT("BMP_readTemperature Error: NULL pointer\r\n");
      return BMP_ERR_NULL_PTR;
  }

  int32_t UT = 0;
  int32_t B5;
  float temp; // Variable pour la température finale
  BMP_Status_t status;

  status = readRawTemperature(bmp, &UT); // Lit la température brute
  if (status != BMP_OK) {
      DEBUG_PRINT("BMP_readTemperature Error: readRawTemperature failed (%d)\r\n", status);
      return status; // Erreur I2C ou autre
  }

  B5 = computeB5(bmp, UT); // Calcule B5
  if (bmp->lastError != BMP_OK) {
      DEBUG_PRINT("BMP_readTemperature Error: computeB5 failed (%d)\r\n", bmp->lastError);
      return bmp->lastError; // Vérifie erreur dans computeB5
  }
  temp = (B5 + 8) >> 4; // Calcule la température en dixièmes de degré
  temp /= 10; // Convertit en degrés Celsius

  *temp_degC = temp; DEBUG_PRINT("BMP_readTemperature: Calculated Temp = %.2f C\r\n", *temp_degC);
  return BMP_OK; // Succès
}

// Calcule la pression finale à partir de la pression brute (UP) et de B5 (calculé à partir de la température brute)
// Retourne la pression en Pa.
// Note : Cette fonction NE lit PAS les valeurs brutes, elle fait juste le calcul.
// Suppression de la définition dupliquée
static int32_t calculatePressure(BMP_Handle_t *bmp, int32_t UP, int32_t B5) // Garde celle-ci
{
  if (bmp == NULL) {
      DEBUG_PRINT("calculatePressure Error: NULL pointer\r\n");
      return 0; // Ne peut pas stocker l'erreur
  }

  int32_t B3, B6, X1, X2, X3, p; // Variables intermédiaires
  uint32_t B4, B7; // Variables intermédiaires non signées

  // Effectue les calculs de pression
  B6 = B5 - 4000;
  X1 = ((int32_t)bmp->b2 * ((B6 * B6) >> 12)) >> 11;
  X2 = ((int32_t)bmp->ac2 * B6) >> 11;
  X3 = X1 + X2;
  B3 = ((((int32_t)bmp->ac1 * 4 + X3) << bmp->oversampling) + 2) / 4;

  X1 = ((int32_t)bmp->ac3 * B6) >> 13;
  X2 = ((int32_t)bmp->b1 * ((B6 * B6) >> 12)) >> 16;
  X3 = ((X1 + X2) + 2) >> 2;
  B4 = ((uint32_t)bmp->ac4 * (uint32_t)(X3 + 32768)) >> 15;

  if (B4 == 0) { // Vérification division par zéro
      bmp->lastError = BMP_ERR_MATH;
      DEBUG_PRINT("calculatePressure Error: Division by zero (B4 == 0)\r\n");
      return 0;
  }

  B7 = ((uint32_t)UP - B3) * (uint32_t)(50000UL >> bmp->oversampling);

  if (B7 < 0x80000000) // Vérifie si B7 est inférieur à une limite
  {
    p = (B7 * 2) / B4; // Calcule la pression
  }
  else
  {
    p = (B7 / B4) * 2; // Calcule la pression
  }
  X1 = (p >> 8) * (p >> 8);
  X1 = (X1 * 3038) >> 16;
  X2 = (-7357 * p) >> 16;

  p = p + ((X1 + X2 + (int32_t)3791) >> 4); // Ajuste la pression finale

  return p;
}

// Retourne BMP_OK en cas de succès. Met la valeur lue dans *pressure_Pa
// Fonction conservée pour compatibilité ou usage simple, mais moins efficace que BMP_readAll.
BMP_Status_t BMP_readPressure(BMP_Handle_t *bmp, int32_t *pressure_Pa) // Calcule et retourne la pression en Pa
{
  if (bmp == NULL || pressure_Pa == NULL) {
      DEBUG_PRINT("BMP_readPressure Error: NULL pointer\r\n");
      return BMP_ERR_NULL_PTR;
  }

  int32_t UT, UP, B5;
  BMP_Status_t status;

  // Lire la température brute (nécessaire pour le calcul de pression)
  status = readRawTemperature(bmp, &UT);
  if (status != BMP_OK) {
      DEBUG_PRINT("BMP_readPressure Error: readRawTemperature failed (%d)\r\n", status);
      return status;
  }
  // Lire la pression brute
  status = readRawPressure(bmp, &UP);
  if (status != BMP_OK) {
      DEBUG_PRINT("BMP_readPressure Error: readRawPressure failed (%d)\r\n", status);
      return status;
  }

  B5 = computeB5(bmp, UT); // Calcule B5
  if (bmp->lastError != BMP_OK) {
      DEBUG_PRINT("BMP_readPressure Error: computeB5 failed (%d)\r\n", bmp->lastError);
      return bmp->lastError; // Vérifie erreur dans computeB5
  }

  *pressure_Pa = calculatePressure(bmp, UP, B5); // Calcule la pression finale
  if (bmp->lastError != BMP_OK) {
      DEBUG_PRINT("BMP_readPressure Error: calculatePressure failed (%d)\r\n", bmp->lastError);
      return bmp->lastError; // Vérifie erreur dans calculatePressure
  }

  DEBUG_PRINT("BMP_readPressure: Calculated Pressure = %ld Pa\r\n", *pressure_Pa);
  return BMP_OK; // Succès
}

// Lit la température et la pression de manière optimisée.
// Retourne BMP_OK en cas de succès.
BMP_Status_t BMP_readAll(BMP_Handle_t *bmp, float *temp_degC, int32_t *pressure_Pa)
{
    if (bmp == NULL || temp_degC == NULL || pressure_Pa == NULL) {
        DEBUG_PRINT("BMP_readAll Error: NULL pointer\r\n");
        return BMP_ERR_NULL_PTR;
    }

    int32_t UT, UP, B5;
    float temp;
    BMP_Status_t status;

    // 1. Lire la température brute
    status = readRawTemperature(bmp, &UT);
    if (status != BMP_OK) {
        DEBUG_PRINT("BMP_readAll Error: readRawTemperature failed (%d)\r\n", status);
        return status;
    }

    // 2. Lire la pression brute
    status = readRawPressure(bmp, &UP);
    if (status != BMP_OK) {
        DEBUG_PRINT("BMP_readAll Error: readRawPressure failed (%d)\r\n", status);
        return status;
    }

    // 3. Calculer B5 (nécessaire pour les deux)
    // Correction: Appel à computeB5 avec le handle
    B5 = computeB5(bmp, UT);
    if (bmp->lastError != BMP_OK) {
        DEBUG_PRINT("BMP_readAll Error: computeB5 failed (%d)\r\n", bmp->lastError);
        return bmp->lastError; // Vérifie erreur dans computeB5
    }

    // 4. Calculer la température finale
    temp = (B5 + 8) >> 4;
    temp /= 10;
    *temp_degC = temp; DEBUG_PRINT("BMP_readAll: Calculated Temp = %.2f C\r\n", *temp_degC);

    // 5. Calculer la pression finale
    *pressure_Pa = calculatePressure(bmp, UP, B5);
    if (bmp->lastError != BMP_OK) {
        DEBUG_PRINT("BMP_readAll Error: calculatePressure failed (%d)\r\n", bmp->lastError);
        return bmp->lastError; // Vérifie erreur dans calculatePressure
    }
    DEBUG_PRINT("BMP_readAll: Calculated Pressure = %ld Pa\r\n", *pressure_Pa);
    return BMP_OK; // Succès
}

// --- Fonctions de calcul publiques (ne dépendent pas directement du handle) ---

// Calcule l'altitude en mètres. Retourne BMP_OK ou BMP_ERR_INVALID_PARAM.
BMP_Status_t BMP_calculateAltitude(int32_t pressure_Pa, float sealevelPressure_Pa, float *altitude_m)
{
  if (altitude_m == NULL) {
      DEBUG_PRINT("BMP_calculateAltitude Error: NULL pointer for altitude_m\r\n");
      return BMP_ERR_NULL_PTR;
  }

  if (pressure_Pa <= 0 || sealevelPressure_Pa <= 0) { // Vérifie les pressions valides
      DEBUG_PRINT("BMP_calculateAltitude Error: Invalid pressure input (P=%ld Pa, P0=%.1f Pa)\r\n", pressure_Pa, sealevelPressure_Pa);
      *altitude_m = NAN; // Ou une autre valeur d'erreur comme -9999.0f
      return BMP_ERR_INVALID_PARAM;
  }
  // Utilise powf pour les opérations en virgule flottante
  *altitude_m = 44330.0f * (1.0f - powf((float)pressure_Pa / sealevelPressure_Pa, 0.1903f)); // Calcule l'altitude
  DEBUG_PRINT("BMP_calculateAltitude: P=%ld Pa, P0=%.1f Pa -> Alt=%.2f m\r\n", pressure_Pa, sealevelPressure_Pa, *altitude_m);
  return BMP_OK;
}

// Calcule la pression au niveau de la mer (Pa) à partir de la pression mesurée (Pa) et de l'altitude (m)
int32_t BMP_calculateSealevelPressure(int32_t pressure_Pa, float altitude_m) // Cette fonction n'est pas utilisée dans main.c pour l'instant
{
  // Utilise powf pour les opérations en virgule flottante
  // Attention : vérifier que le dénominateur ne devient pas nul ou négatif si altitude_m est très grand
  float factor = powf(1.0f - altitude_m / 44330.0f, 5.255f);
  if (factor <= 0) {
      DEBUG_PRINT("BMP_calculateSealevelPressure Error: Calculation factor <= 0 (Alt=%.1f m)\r\n", altitude_m);
      // Retourner une valeur invalide comme 0
      return 0;
  }
  int32_t sealevel_p = (int32_t)((float)pressure_Pa / factor);
  DEBUG_PRINT("BMP_calculateSealevelPressure: P=%ld Pa, Alt=%.1f m -> P0=%ld Pa\r\n", pressure_Pa, altitude_m, sealevel_p);
  return sealevel_p; // Calcule la pression au niveau de la mer
} // Correction: Ajout de l'accolade fermante manquante
