#include "STM32_BMP085.h" // Inclut le fichier d'en-tête pour les fonctions du capteur BMP085
#include "config.h"       // Inclut le fichier de configuration pour les paramètres spécifiques au projet

// Registres de données de calibration
#define BMP085_CAL_AC1 0xAA //!< R   Données de calibration (16 bits)
#define BMP085_CAL_AC2 0xAC //!< R   Données de calibration (16 bits)
#define BMP085_CAL_AC3 0xAE //!< R   Données de calibration (16 bits)
#define BMP085_CAL_AC4 0xB0 //!< R   Données de calibration (16 bits)
#define BMP085_CAL_AC5 0xB2 //!< R   Données de calibration (16 bits)
#define BMP085_CAL_AC6 0xB4 //!< R   Données de calibration (16 bits)
#define BMP085_CAL_B1 0xB6  //!< R   Données de calibration (16 bits)
#define BMP085_CAL_B2 0xB8  //!< R   Données de calibration (16 bits)
#define BMP085_CAL_MB 0xBA  //!< R   Données de calibration (16 bits)
#define BMP085_CAL_MC 0xBC  //!< R   Données de calibration (16 bits)
#define BMP085_CAL_MD 0xBE  //!< R   Données de calibration (16 bits)

// Commandes
#define BMP085_CONTROL 0xF4         //!< Registre de contrôle
#define BMP085_TEMPDATA 0xF6        //!< Registre des données de température
#define BMP085_PRESSUREDATA 0xF6    //!< Registre des données de pression
#define BMP085_READTEMPCMD 0x2E     //!< Commande pour lire la température
#define BMP085_READPRESSURECMD 0x34 //!< Commande pour lire la pression

static const uint8_t BMP085_ADDR = CONFIG_I2C_ADDRESS << 1; // Adresse I2C 8 bits issue de la configuration
I2C_HandleTypeDef *bmpPort; // Pointeur pour gérer l'interface I2C
uint8_t oversampling; // Niveau de suréchantillonnage

// Données de calibration (seront lues depuis le capteur)
int16_t ac1, ac2, ac3, b1, b2, mb, mc, md; // Variables pour les données de calibration signées
uint16_t ac4, ac5, ac6; // Variables pour les données de calibration non signées

// Fonctions de gestion I2C
static uint8_t read8(uint8_t a) // Lit un octet depuis un registre
{
  uint8_t r; // Variable pour stocker la valeur lue
  HAL_I2C_Master_Transmit(bmpPort, BMP085_ADDR, &a, 1, HAL_MAX_DELAY); // Envoie l'adresse du registre
  HAL_I2C_Master_Receive(bmpPort, BMP085_ADDR, &r, 1, HAL_MAX_DELAY); // Reçoit la valeur du registre
  return r; // Retourne la valeur lue
}

static uint16_t read16(uint8_t a) // Lit deux octets depuis un registre
{
  uint8_t retbuf[2]; // Tampon pour stocker les deux octets
  uint16_t r; // Variable pour stocker la valeur combinée
  HAL_I2C_Master_Transmit(bmpPort, BMP085_ADDR, &a, 1, HAL_MAX_DELAY); // Envoie l'adresse du registre
  HAL_I2C_Master_Receive(bmpPort, BMP085_ADDR, retbuf, 2, HAL_MAX_DELAY); // Reçoit les deux octets
  r = retbuf[1] | (retbuf[0] << 8); // Combine les octets en un entier 16 bits
  return r; // Retourne la valeur combinée
}

static void write8(uint8_t a, uint8_t d) // Écrit un octet dans un registre
{
  uint8_t tBuf[2]; // Tampon pour l'adresse et la donnée
  tBuf[0] = a; // Adresse du registre
  tBuf[1] = d; // Donnée à écrire
  HAL_I2C_Master_Transmit(bmpPort, BMP085_ADDR, tBuf, 2, HAL_MAX_DELAY); // Envoie les données
}

uint8_t BMP085_Init(uint8_t mode, I2C_HandleTypeDef *i2cdev) // Initialise le capteur BMP085
{
  bmpPort = i2cdev; // Associe le périphérique I2C

  if (mode > BMP085_ULTRAHIGHRES) // Vérifie si le mode est valide
    mode = BMP085_ULTRAHIGHRES; // Définit le mode au maximum si invalide
  oversampling = mode; // Définit le niveau de suréchantillonnage

  if (read8(0xD0) != 0x55) // Vérifie l'identifiant du capteur
    return 0; // Retourne 0 si le capteur n'est pas détecté

  /* Lit les données de calibration */
  ac1 = read16(BMP085_CAL_AC1);
  ac2 = read16(BMP085_CAL_AC2);
  ac3 = read16(BMP085_CAL_AC3);
  ac4 = read16(BMP085_CAL_AC4);
  ac5 = read16(BMP085_CAL_AC5);
  ac6 = read16(BMP085_CAL_AC6);

  b1 = read16(BMP085_CAL_B1);
  b2 = read16(BMP085_CAL_B2);

  mb = read16(BMP085_CAL_MB);
  mc = read16(BMP085_CAL_MC);
  md = read16(BMP085_CAL_MD);

  return 1; // Retourne 1 si l'initialisation est réussie
}

// Fonctions de lecture du capteur
static int32_t computeB5(int32_t UT) // Calcule la valeur B5 pour la température
{
  int32_t X1 = (UT - (int32_t)ac6) * ((int32_t)ac5) >> 15; // Calcul intermédiaire X1
  int32_t X2 = ((int32_t)mc << 11) / (X1 + (int32_t)md); // Calcul intermédiaire X2
  return X1 + X2; // Retourne la somme de X1 et X2
}

static uint16_t readRawTemperature(void) // Lit la température brute
{
  write8(BMP085_CONTROL, BMP085_READTEMPCMD); // Envoie la commande pour lire la température
  HAL_Delay(5); // Attend 5 ms pour la conversion
  return read16(BMP085_TEMPDATA); // Lit les données de température
}

static uint32_t readRawPressure(void) // Lit la pression brute
{
  uint32_t raw; // Variable pour stocker la pression brute

  write8(BMP085_CONTROL, BMP085_READPRESSURECMD + (oversampling << 6)); // Envoie la commande pour lire la pression

  if (oversampling == BMP085_ULTRALOWPOWER) // Vérifie le niveau de suréchantillonnage
    HAL_Delay(5); // Délai pour ULTRALOWPOWER
  else if (oversampling == BMP085_STANDARD)
    HAL_Delay(8); // Délai pour STANDARD
  else if (oversampling == BMP085_HIGHRES)
    HAL_Delay(14); // Délai pour HIGHRES
  else
    HAL_Delay(26); // Délai pour ULTRAHIGHRES

  raw = read16(BMP085_PRESSUREDATA); // Lit les deux premiers octets de la pression

  raw <<= 8; // Décale les bits pour préparer l'ajout du troisième octet
  raw |= read8(BMP085_PRESSUREDATA + 2); // Ajoute le troisième octet
  raw >>= (8 - oversampling); // Ajuste selon le niveau de suréchantillonnage

  return raw; // Retourne la pression brute
}

float BMP085_readTemperature(void) // Calcule et retourne la température en °C
{
  int32_t UT, B5; // Variables pour la température brute et B5
  float temp; // Variable pour la température finale

  UT = readRawTemperature(); // Lit la température brute

  B5 = computeB5(UT); // Calcule B5
  temp = (B5 + 8) >> 4; // Calcule la température en dixièmes de degré
  temp /= 10; // Convertit en degrés Celsius

  return temp; // Retourne la température
}

int32_t BMP085_readPressure(void) // Calcule et retourne la pression en Pa
{
  int32_t UT, UP, B3, B5, B6, X1, X2, X3, p; // Variables intermédiaires
  uint32_t B4, B7; // Variables intermédiaires non signées

  UT = readRawTemperature(); // Lit la température brute
  UP = readRawPressure(); // Lit la pression brute

  B5 = computeB5(UT); // Calcule B5

  // Effectue les calculs de pression
  B6 = B5 - 4000;
  X1 = ((int32_t)b2 * ((B6 * B6) >> 12)) >> 11;
  X2 = ((int32_t)ac2 * B6) >> 11;
  X3 = X1 + X2;
  B3 = ((((int32_t)ac1 * 4 + X3) << oversampling) + 2) / 4;

  X1 = ((int32_t)ac3 * B6) >> 13;
  X2 = ((int32_t)b1 * ((B6 * B6) >> 12)) >> 16;
  X3 = ((X1 + X2) + 2) >> 2;
  B4 = ((uint32_t)ac4 * (uint32_t)(X3 + 32768)) >> 15;
  B7 = ((uint32_t)UP - B3) * (uint32_t)(50000UL >> oversampling);

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

  return p; // Retourne la pression
}

float BMP085_readAltitude(float sealevelPressure) // Calcule et retourne l'altitude en mètres
{
  float pressure = BMP085_readPressure(); // Lit la pression actuelle
  float altitude = 44330 * (1.0 - pow(pressure / sealevelPressure, 0.1903)); // Calcule l'altitude
  return altitude; // Retourne l'altitude
}

int32_t BMP085_readSealevelPressure(float altitude) // Calcule la pression au niveau de la mer
{
  float pressure = BMP085_readPressure(); // Lit la pression actuelle
  return (int32_t)(pressure / pow(1.0 - altitude / 44330, 5.255)); // Calcule et retourne la pression au niveau de la mer
}
