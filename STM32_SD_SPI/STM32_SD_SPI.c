#include "diskio.h"       // Inclusion de l'interface Disk I/O
#include <STM32_SD_SPI.h> // Inclusion du fichier d'en-tête spécifique à la carte SD via SPI
#include "ff.h"           // Inclusion de la bibliothèque FatFs pour MKFS_PARM
#include "ffconf.h"       // Inclusion de la configuration FatFs pour MKFS_PARM

static volatile DSTATUS Stat = STA_NOINIT; // Statut du disque, initialisé à "non initialisé"
static uint8_t CardType;                   // Type de carte SD : 0 pour MMC, 1 pour SDC, 2 pour adressage par bloc
static uint8_t PowerFlag = 0;              // Indicateur d'alimentation, initialisé à 0

/***************************************
 * Fonctions SPI
 ***************************************/

/* Sélection de l'esclave */
static void SELECT(void)
{
  HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET); // Met la broche CS à l'état bas pour sélectionner l'esclave
  HAL_Delay(1);                                             // Attente d'une courte durée pour stabilisation
}

/* Désélection de l'esclave */
static void DESELECT(void)
{
  HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET); // Met la broche CS à l'état haut pour désélectionner l'esclave
  HAL_Delay(1);                                           // Attente d'une courte durée pour stabilisation
}

/* Transmission d'un octet via SPI */
static void SPI_TxByte(uint8_t data)
{
  while (!__HAL_SPI_GET_FLAG(HSPI_SDCARD, SPI_FLAG_TXE))
    ;                                                   // Attente que le tampon de transmission soit vide
  HAL_SPI_Transmit(HSPI_SDCARD, &data, 1, SPI_TIMEOUT); // Transmission de l'octet via SPI
}

/* Transmission d'un buffer via SPI */
static void SPI_TxBuffer(uint8_t *buffer, uint16_t len)
{
  while (!__HAL_SPI_GET_FLAG(HSPI_SDCARD, SPI_FLAG_TXE))
    ;                                                      // Attente que le tampon de transmission soit vide
  HAL_SPI_Transmit(HSPI_SDCARD, buffer, len, SPI_TIMEOUT); // Transmission du buffer via SPI
}

/* Réception d'un octet via SPI */
static uint8_t SPI_RxByte(void)
{
  uint8_t dummy = SPI_DUMMY_BYTE; // Octet vide pour l'envoi
  uint8_t data;                   // Variable pour stocker l'octet reçu

  while (!__HAL_SPI_GET_FLAG(HSPI_SDCARD, SPI_FLAG_TXE))
    ;                                                                  // Attente que le tampon de transmission soit vide
  HAL_SPI_TransmitReceive(HSPI_SDCARD, &dummy, &data, 1, SPI_TIMEOUT); // Envoi et réception simultanés via SPI

  return data; // Retourne l'octet reçu
}

/* Réception d'un octet via pointeur */
static void SPI_RxBytePtr(uint8_t *buff)
{
  *buff = SPI_RxByte(); // Stocke l'octet reçu dans le tampon pointé
}

/***************************************
 * Fonctions SD
 ***************************************/

/* Attendre que la carte SD soit prête */
static uint8_t SD_ReadyWait(void)
{
  uint8_t res;                        // Variable pour stocker la réponse
  uint32_t startTime = HAL_GetTick(); // Temps de départ pour le timeout

  do
  {
    res = SPI_RxByte(); // Lecture d'un octet depuis la carte SD
    if (res == SPI_DUMMY_BYTE)
    {
      return res; // Prêt
    }
  } while ((HAL_GetTick() - startTime) < SD_TIMEOUT_READY); // Boucle jusqu'à ce que la carte soit prête ou que le timeout expire (SD_TIMEOUT_READY en ms)

  return res; // Retourne la dernière réponse (probablement pas SPI_DUMMY_BYTE si timeout)
}

/* Mise sous tension */
static void SD_PowerOn(void)
{
  uint8_t args[6];                // Tableau pour les arguments de commande
  uint32_t cnt = SD_TIMEOUT_INIT; // Compteur pour le timeout

  DESELECT();                  // Désélectionne la carte SD
  for (int i = 0; i < 10; i++) // Envoie 80 cycles d'horloge pour réveiller la carte
  {
    SPI_TxByte(SPI_DUMMY_BYTE); // Envoi d'un octet vide
  }

  SELECT(); // Sélectionne la carte SD

  args[0] = CMD0;     // Commande CMD0 : GO_IDLE_STATE
  args[1] = 0;        // Argument 1
  args[2] = 0;        // Argument 2
  args[3] = 0;        // Argument 3
  args[4] = 0;        // Argument 4
  args[5] = CMD0_CRC; // CRC pour CMD0

  SPI_TxBuffer(args, sizeof(args)); // Transmission des arguments via SPI

  while ((SPI_RxByte() != 0x01) && cnt) // Attente de la réponse de la carte SD ou expiration du timeout
  {
    cnt--; // Décrémentation du compteur
  }

  DESELECT();                 // Désélectionne la carte SD
  SPI_TxByte(SPI_DUMMY_BYTE); // Envoi d'un octet vide pour terminer la communication

  PowerFlag = 1; // Indique que la carte SD est sous tension
}

/* Mise hors tension */
static void SD_PowerOff(void)
{
  PowerFlag = 0; // Réinitialise l'indicateur d'alimentation
}

/* Vérification de l'état d'alimentation */
static uint8_t SD_CheckPower(void)
{
  return PowerFlag; // Retourne l'état d'alimentation
}

/* Réception d'un bloc de données */
static uint8_t SD_RxDataBlock(BYTE *buff, UINT len)
{
  uint8_t token;                      // Variable pour stocker le jeton
  uint32_t startTime = HAL_GetTick(); // Temps de départ pour le timeout

  do
  {
    token = SPI_RxByte(); // Lecture d'un octet depuis la carte SD
    if (token != SPI_DUMMY_BYTE)
    { // Si on reçoit autre chose qu'un octet vide
      break;
    }
  } while ((HAL_GetTick() - startTime) < SD_TIMEOUT_BLOCK); // Boucle jusqu'à réception d'un jeton ou expiration du timeout (SD_TIMEOUT_BLOCK en ms)

  if (token != SD_READY_TOKEN)
  {
    return 0; // Si le jeton est invalide ou timeout, retourne 0
  }

  do
  {
    SPI_RxBytePtr(buff++); // Réception des données dans le tampon
  } while (len--); // Répète jusqu'à ce que toutes les données soient reçues
  SPI_RxByte(); // Ignore le CRC
  SPI_RxByte(); // Ignore le CRC

  return 1; // Retourne 1 pour indiquer le succès
}

/* Transmission d'un bloc de données */
#if _USE_WRITE == 1
static uint8_t SD_TxDataBlock(const uint8_t *buff, BYTE token)
{
  uint8_t resp;
  uint8_t i = 0;

  /* Attendre que la carte SD soit prête */
  if (SD_ReadyWait() != 0xFF)
    return 0;

  /* Transmission du jeton */
  SPI_TxByte(token);

  /* Si ce n'est pas le jeton STOP, transmettre les données */
  if (token != SD_STOP_TRANSMISSION)
  {
    SPI_TxBuffer((uint8_t *)buff, BLOCK_SIZE);

    /* Ignorer le CRC */
    SPI_RxByte();
    SPI_RxByte();

    /* Réception de la réponse */
    while (i <= 64)
    {
      resp = SPI_RxByte();

      /* Transmission acceptée avec 0x05 */
      if ((resp & 0x1F) == SD_ACCEPTED)
        break;
      i++;
    }

    /* Vider le buffer de réception */
    while (SPI_RxByte() == 0)
      ;
  }

  /* Transmission acceptée avec 0x05 */
  if ((resp & 0x1F) == SD_ACCEPTED)
    return 1;

  return 0;
}
#endif /* _USE_WRITE */

/* Transmission d'une commande */
static BYTE SD_SendCmd(BYTE cmd, uint32_t arg)
{
  uint8_t crc, res;

  /* Attendre que la carte SD soit prête */
  if (SD_ReadyWait() != 0xFF)
    return 0xFF;

  /* Transmission de la commande */
  SPI_TxByte(cmd);                  /* Commande */
  SPI_TxByte((uint8_t)(arg >> 24)); /* Argument[31..24] */
  SPI_TxByte((uint8_t)(arg >> 16)); /* Argument[23..16] */
  SPI_TxByte((uint8_t)(arg >> 8));  /* Argument[15..8] */
  SPI_TxByte((uint8_t)arg);         /* Argument[7..0] */

  /* Préparation du CRC */
  if (cmd == CMD0)
    crc = CMD0_CRC; /* CRC pour CMD0(0) */
  else if (cmd == CMD8)
    crc = CMD8_CRC; /* CRC pour CMD8(0x1AA) */
  else
    crc = 1;

  /* Transmission du CRC */
  SPI_TxByte(crc);

  /* Ignorer un octet lors de STOP_TRANSMISSION */
  if (cmd == CMD12)
    SPI_RxByte();

  /* Réception de la réponse */
  uint8_t n = 10;
  do
  {
    res = SPI_RxByte();
  } while ((res & 0x80) && --n);

  return res;
}

/***************************************
 * Fonctions de user_diskio.c
 ***************************************/

/* Initialisation de la carte SD */
DSTATUS SD_disk_initialize(BYTE drv)
{
  uint8_t n, type, ocr[4];

  /* Un seul lecteur, drv doit être 0 */
  if (drv)
    return STA_NOINIT;

  /* Pas de disque */
  if (Stat & STA_NODISK)
    return Stat;

  /* Mise sous tension */
  SD_PowerOn();

  /* Sélection de l'esclave */
  SELECT();

  /* Vérification du type de disque */
  type = 0;

  /* Envoi de la commande GO_IDLE_STATE (CMD0) */
  if (SD_SendCmd(CMD0, 0) == 0x01) // 0x01 = In Idle State
  {
    /* Timeout de 1 seconde */
    uint32_t initStartTime = HAL_GetTick();

    /* SDC V2+ accepte la commande CMD8 */
    if (SD_SendCmd(CMD8, 0x1AA) == 1)
    {
      /* Registre de condition d'opération */
      for (n = 0; n < 4; n++)
      {
        ocr[n] = SPI_RxByte();
      }

      /* Plage de tension 2.7-3.6V */
      if (ocr[2] == 0x01 && ocr[3] == 0xAA)
      {
        /* ACMD41 avec bit HCS */
        do
        {
          // Envoyer CMD55 (APP_CMD) puis ACMD41 (SD_SEND_OP_COND)
          if (SD_SendCmd(CMD55, 0) <= 1 && SD_SendCmd(CMD41, 1UL << 30) == 0)
          { // 0 = Initialisation terminée
            break;
          }
        } while ((HAL_GetTick() - initStartTime) < 1000); // Timeout de 1000 ms

        /* READ_OCR */
        if (((HAL_GetTick() - initStartTime) < 1000) && SD_SendCmd(CMD58, 0) == 0) // Vérifier si le timeout n'est pas écoulé
        {
          /* Vérification du bit CCS */
          for (n = 0; n < 4; n++)
          {
            ocr[n] = SPI_RxByte();
          }

          /* SDv2 (HC ou SC) */
          type = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;
        }
      }
    }
    else
    {
      /* SDC V1 ou MMC */
      type = (SD_SendCmd(CMD55, 0) <= 1 && SD_SendCmd(CMD41, 0) <= 1) ? CT_SD1 : CT_MMC;

      do
      {
        if (type == CT_SD1)
        { // Pour SDv1
          if (SD_SendCmd(CMD55, 0) <= 1 && SD_SendCmd(CMD41, 0) == 0)
          {        // 0 = Initialisation terminée
            break; /* ACMD41 */
          }
        }
        else
        { // Pour MMC
          if (SD_SendCmd(CMD1, 0) == 0)
          {        // 0 = Initialisation terminée
            break; /* CMD1 */
          }
        }
      } while ((HAL_GetTick() - initStartTime) < 1000); // Timeout de 1000 ms

      /* SET_BLOCKLEN */
      if (((HAL_GetTick() - initStartTime) >= 1000) || SD_SendCmd(CMD16, 512) != 0)
      {           // Si timeout ou échec CMD16
        type = 0; // Échec de l'initialisation
      }
    }
  }

  CardType = type;

  /* Inactif */
  DESELECT();
  SPI_RxByte();

  /* Effacement de STA_NOINIT */
  if (type)
  {
    Stat &= ~STA_NOINIT;
  }
  else
  {
    /* Initialisation échouée */
    SD_PowerOff();
  }

  return Stat;
}

/* Retourne le statut du disque */
DSTATUS SD_disk_status(BYTE drv)
{
  if (drv)
    return STA_NOINIT;
  return Stat;
}

/* Lecture d'un secteur */
DRESULT SD_disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
  /* pdrv doit être 0 */
  if (pdrv || !count)
    return RES_PARERR;

  /* Pas de disque */
  if (Stat & STA_NOINIT)
    return RES_NOTRDY;

  /* Conversion en adresse octet */
  if (!(CardType & CT_SD2))
    sector *= 512;

  SELECT();

  if (count == 1)
  {
    /* READ_SINGLE_BLOCK */
    if ((SD_SendCmd(CMD17, sector) == 0) && SD_RxDataBlock(buff, 512))
      count = 0;
  }
  else
  {
    /* READ_MULTIPLE_BLOCK */
    if (SD_SendCmd(CMD18, sector) == 0)
    {
      do
      {
        if (!SD_RxDataBlock(buff, 512))
          break;
        buff += 512;
      } while (--count);

      /* STOP_TRANSMISSION */
      SD_SendCmd(CMD12, 0);
    }
  }

  /* Inactif */
  DESELECT();
  SPI_RxByte();

  return count ? RES_ERROR : RES_OK;
}

/* Écriture d'un secteur */
#if _USE_WRITE == 1
DRESULT SD_disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
  /* pdrv doit être 0 */
  if (pdrv || !count)
    return RES_PARERR;

  /* Pas de disque */
  if (Stat & STA_NOINIT)
    return RES_NOTRDY;

  /* Protection en écriture */
  if (Stat & STA_PROTECT)
    return RES_WRPRT;

  /* Conversion en adresse octet */
  if (!(CardType & CT_SD2))
    sector *= 512;

  SELECT();

  if (count == 1)
  {
    /* WRITE_BLOCK */
    if ((SD_SendCmd(CMD24, sector) == 0) && SD_TxDataBlock(buff, SD_READY_TOKEN))
      count = 0;
  }
  else
  {
    /* WRITE_MULTIPLE_BLOCK */
    if (CardType & CT_SD1)
    {
      SD_SendCmd(CMD55, 0);
      SD_SendCmd(CMD23, count); /* ACMD23 */
    }

    if (SD_SendCmd(CMD25, sector) == 0)
    {
      do
      {
        if (!SD_TxDataBlock(buff, SD_MULTIPLE_WRITE))
          break;
        buff += 512;
      } while (--count);

      /* Jeton STOP_TRAN */
      if (!SD_TxDataBlock(0, SD_STOP_TRANSMISSION))
      {
        count = 1;
      }
    }
  }

  /* Inactif */
  DESELECT();
  SPI_RxByte();

  return count ? RES_ERROR : RES_OK;
}
#endif /* _USE_WRITE */

/* ioctl */
DRESULT SD_disk_ioctl(BYTE drv, BYTE ctrl, void *buff)
{
  DRESULT res;
  uint8_t n, csd[16], *ptr = buff;
  WORD csize;

  /* pdrv doit être 0 */
  if (drv)
    return RES_PARERR;
  res = RES_ERROR;

  if (ctrl == CTRL_POWER)
  {
    switch (*ptr)
    {
    case 0:
      SD_PowerOff(); /* Mise hors tension */
      res = RES_OK;
      break;
    case 1:
      SD_PowerOn(); /* Mise sous tension */
      res = RES_OK;
      break;
    case 2:
      *(ptr + 1) = SD_CheckPower();
      res = RES_OK; /* Vérification de l'alimentation */
      break;
    default:
      res = RES_PARERR;
    }
  }
  else
  {
    /* Pas de disque */
    if (Stat & STA_NOINIT)
      return RES_NOTRDY;

    SELECT();

    switch (ctrl)
    {
    case GET_SECTOR_COUNT:
      /* SEND_CSD */
      if ((SD_SendCmd(CMD9, 0) == 0) && SD_RxDataBlock(csd, 16))
      {
        if ((csd[0] >> 6) == 1)
        {
          /* SDC V2 */
          csize = csd[9] + ((WORD)csd[8] << 8) + 1;
          *(DWORD *)buff = (DWORD)csize << 10;
        }
        else
        {
          /* MMC ou SDC V1 */
          n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
          csize = (csd[8] >> 6) + ((WORD)csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
          *(DWORD *)buff = (DWORD)csize << (n - 9);
        }
        res = RES_OK;
      }
      break;
    case GET_SECTOR_SIZE:
      *(WORD *)buff = 512;
      res = RES_OK;
      break;
    case CTRL_SYNC:
      if (SD_ReadyWait() == 0xFF)
        res = RES_OK;
      break;
    case MMC_GET_CSD:
      /* SEND_CSD */
      if (SD_SendCmd(CMD9, 0) == 0 && SD_RxDataBlock(ptr, 16))
        res = RES_OK;
      break;
    case MMC_GET_CID:
      /* SEND_CID */
      if (SD_SendCmd(CMD10, 0) == 0 && SD_RxDataBlock(ptr, 16))
        res = RES_OK;
      break;
    case MMC_GET_OCR:
      /* READ_OCR */
      if (SD_SendCmd(CMD58, 0) == 0)
      {
        for (n = 0; n < 4; n++)
        {
          *ptr++ = SPI_RxByte();
        }
        res = RES_OK;
      }
    default:
      res = RES_PARERR;
    }

    DESELECT();
    SPI_RxByte();
  }

  return res;
}

/***************************************
 * Fonctions d'utilisations
 ***************************************/

FATFS systemeFichiers; // Gestionnaire du système de fichiers
FIL fichier;           // Gestionnaire de fichier

/**
 * @brief Monte la carte SD.
 * @retval 0 si succès, -1 si échec.
 */
DRESULT SD_Monter(void)
{
  FRESULT resultat = f_mount(&systemeFichiers, "", 1); // Monter la carte SD
  if (resultat != FR_OK)
  {
    DEBUG_PRINTF("Erreur : Impossible de monter la carte SD (%i)\r\n", resultat);
    return -1;
  }
  DEBUG_PRINTF("Carte SD montée avec succès !\r\n");
  return 0;
}

/**
 * @brief Démonte la carte SD.
 */
void SD_Demonter(void)
{
  f_mount(NULL, "", 0); // Démonter la carte SD
  DEBUG_PRINTF("Carte SD démontée avec succès !\r\n");
}

/**
 * @brief Lit l'espace total et libre de la carte SD.
 * @param[out] espaceTotal Espace total en octets.
 * @param[out] espaceLibre Espace libre en octets.
 * @retval 0 si succès, -1 si échec.
 */
DRESULT SD_LireEspace(uint32_t *espaceTotal, uint32_t *espaceLibre)
{
  FATFS *ptrSystemeFichiers;
  DWORD clustersLibres;

  if (f_getfree("", &clustersLibres, &ptrSystemeFichiers) != FR_OK)
  {
    DEBUG_PRINTF("Erreur : Impossible de lire l'espace de la carte SD\r\n");
    return -1;
  }

  *espaceTotal = (uint32_t)((ptrSystemeFichiers->n_fatent - 2) * ptrSystemeFichiers->csize * 0.5);
  *espaceLibre = (uint32_t)(clustersLibres * ptrSystemeFichiers->csize * 0.5);

  DEBUG_PRINTF("Espace total : %lu octets, Espace libre : %lu octets\r\n", *espaceTotal, *espaceLibre);
  return 0;
}

/**
 * @brief Écrit des données dans un fichier sur la carte SD.
 * @param[in] nomFichier Nom du fichier.
 * @param[in] donnees Données à écrire.
 * @retval 0 si succès, -1 si échec.
 */
DRESULT SD_EcrireFichier(const char *nomFichier, const char *donnees)
{
  FRESULT resultat = f_open(&fichier, nomFichier, FA_WRITE | FA_CREATE_ALWAYS);
  if (resultat != FR_OK)
  {
    DEBUG_PRINTF("Erreur : Impossible d'ouvrir le fichier (%i)\r\n", resultat);
    return -1;
  }

  f_puts(donnees, &fichier);
  f_close(&fichier);
  DEBUG_PRINTF("Données écrites dans le fichier : %s\r\n", nomFichier);
  return 0;
}

/**
 * @brief Lit des données depuis un fichier sur la carte SD.
 * @param[in] nomFichier Nom du fichier.
 * @param[out] tampon Tampon pour stocker les données lues.
 * @param[in] tailleTampon Taille du tampon.
 * @retval 0 si succès, -1 si échec.
 */
DRESULT SD_LireFichier(const char *nomFichier, char *tampon, size_t tailleTampon)
{
  FRESULT resultat = f_open(&fichier, nomFichier, FA_READ);
  if (resultat != FR_OK)
  {
    DEBUG_PRINTF("Erreur : Impossible d'ouvrir le fichier (%i)\r\n", resultat);
    return -1;
  }

  f_gets(tampon, tailleTampon, &fichier);
  f_close(&fichier);
  DEBUG_PRINTF("Données lues depuis le fichier : %s\r\n", tampon);
  return 0;
}

/**
 * @brief Liste les fichiers et répertoires dans un répertoire donné.
 * @param[in] chemin Chemin du répertoire à lister (par exemple, "/").
 * @retval 0 si succès, -1 si échec.
 */
DRESULT SD_ListerFichiers(const char *chemin)
{
  DIR repertoire;      // Gestionnaire de répertoire
  FILINFO infoFichier; // Informations sur les fichiers
  FRESULT resultat;

  // Ouvrir le répertoire
  resultat = f_opendir(&repertoire, chemin);
  if (resultat != FR_OK)
  {
    DEBUG_PRINTF("Erreur : Impossible d'ouvrir le répertoire (%i)\r\n", resultat);
    return -1;
  }

  DEBUG_PRINTF("Contenu du répertoire '%s' :\r\n", chemin);

  // Lire les fichiers et répertoires dans le répertoire
  while (1)
  {
    resultat = f_readdir(&repertoire, &infoFichier); // Lire une entrée
    if (resultat != FR_OK || infoFichier.fname[0] == 0)
      break; // Fin du répertoire ou erreur

    if (infoFichier.fattrib & AM_DIR)
    {
      // C'est un répertoire
      DEBUG_PRINTF("[DIR]  %s\r\n", infoFichier.fname);
    }
    else
    {
      // C'est un fichier
      DEBUG_PRINTF("[FILE] %s (%lu octets)\r\n", infoFichier.fname, infoFichier.fsize);
    }
  }

  // Fermer le répertoire
  f_closedir(&repertoire);

  return 0;
}

/**
 * @brief Supprime un fichier sur la carte SD.
 * @param[in] nomFichier Nom du fichier à supprimer.
 * @retval 0 si succès, -1 si échec.
 */
DRESULT SD_SupprimerFichier(const char *nomFichier)
{
  FRESULT resultat = f_unlink(nomFichier); // Supprime le fichier spécifié
  if (resultat != FR_OK)
  {
    DEBUG_PRINTF("Erreur : Impossible de supprimer le fichier (%i)\r\n", resultat);
    return -1;
  }

  DEBUG_PRINTF("Fichier supprimé avec succès : %s\r\n", nomFichier);
  return 0;
}

/**
 * @brief Vérifie si un fichier ou un répertoire existe.
 * @param[in] nomChemin Chemin du fichier ou répertoire.
 * @param[out] pbExiste Pointeur vers un BYTE qui sera mis à 1 si existe, 0 sinon.
 * @retval 0 si la vérification a réussi, -1 en cas d'erreur système.
 */
DRESULT SD_VerifierExistence(const char *nomChemin, BYTE *pbExiste)
{
  FILINFO infoFichier;
  FRESULT resultat = f_stat(nomChemin, &infoFichier);

  if (resultat == FR_OK)
  {
    *pbExiste = 1; // L'élément existe
    DEBUG_PRINTF("L'élément '%s' existe.\r\n", nomChemin);
    return 0; // Succès de la vérification
  }
  else if (resultat == FR_NO_FILE)
  {
    *pbExiste = 0; // L'élément n'existe pas
    DEBUG_PRINTF("L'élément '%s' n'existe pas.\r\n", nomChemin);
    return 0; // Succès de la vérification
  }
  else
  {
    *pbExiste = 0; // État indéterminé en cas d'erreur
    DEBUG_PRINTF("Erreur lors de la vérification de l'existence de '%s' (%i)\r\n", nomChemin, resultat);
    return -1; // Erreur système
  }
}

/**
 * @brief Crée un nouveau répertoire.
 * @param[in] nomRepertoire Nom du répertoire à créer.
 * @retval 0 si succès, -1 si échec.
 */
DRESULT SD_CreerRepertoire(const char *nomRepertoire)
{
  FRESULT resultat = f_mkdir(nomRepertoire);
  if (resultat == FR_OK)
  {
    DEBUG_PRINTF("Répertoire '%s' créé avec succès.\r\n", nomRepertoire);
    return 0;
  }
  else
  {
    DEBUG_PRINTF("Erreur : Impossible de créer le répertoire '%s' (%i)\r\n", nomRepertoire, resultat);
    return -1;
  }
}

/**
 * @brief Renomme ou déplace un fichier ou un répertoire.
 * @param[in] ancienNom Chemin actuel de l'élément.
 * @param[in] nouveauNom Nouveau chemin de l'élément.
 * @retval 0 si succès, -1 si échec.
 */
DRESULT SD_RenommerElement(const char *ancienNom, const char *nouveauNom)
{
  FRESULT resultat = f_rename(ancienNom, nouveauNom);
  if (resultat == FR_OK)
  {
    DEBUG_PRINTF("Élément '%s' renommé/déplacé en '%s' avec succès.\r\n", ancienNom, nouveauNom);
    return 0;
  }
  else
  {
    DEBUG_PRINTF("Erreur : Impossible de renommer/déplacer '%s' en '%s' (%i)\r\n", ancienNom, nouveauNom, resultat);
    return -1;
  }
}

/**
 * @brief Ajoute des données à la fin d'un fichier (le crée s'il n'existe pas).
 * @param[in] nomFichier Nom du fichier.
 * @param[in] donnees Données à ajouter.
 * @retval 0 si succès, -1 si échec.
 */
DRESULT SD_AjouterAuFichier(const char *nomFichier, const char *donnees)
{
  FRESULT resultat = f_open(&fichier, nomFichier, FA_OPEN_APPEND | FA_WRITE | FA_OPEN_ALWAYS);
  if (resultat != FR_OK)
  {
    DEBUG_PRINTF("Erreur : Impossible d'ouvrir/créer le fichier '%s' en mode ajout (%i)\r\n", nomFichier, resultat);
    return -1;
  }

  if (f_puts(donnees, &fichier) < 0)
  {
    DEBUG_PRINTF("Erreur : Impossible d'écrire dans le fichier '%s' en mode ajout.\r\n", nomFichier);
    f_close(&fichier);
    return -1;
  }

  f_close(&fichier);
  DEBUG_PRINTF("Données ajoutées au fichier : %s\r\n", nomFichier);
  return 0;
}

/**
 * @brief Obtient la taille d'un fichier.
 * @param[in] nomFichier Nom du fichier.
 * @param[out] pdwTaille Pointeur vers un DWORD pour stocker la taille du fichier en octets.
 * @retval 0 si succès, -1 si échec (par exemple, si ce n'est pas un fichier ou s'il n'existe pas).
 */
DRESULT SD_TailleFichier(const char *nomFichier, DWORD *pdwTaille)
{
  FILINFO infoFichier;
  FRESULT resultat = f_stat(nomFichier, &infoFichier);

  if (resultat == FR_OK)
  {
    if (infoFichier.fattrib & AM_DIR)
    {
      DEBUG_PRINTF("Erreur : '%s' est un répertoire, pas un fichier.\r\n", nomFichier);
      *pdwTaille = 0;
      return -1;
    }
    *pdwTaille = infoFichier.fsize;
    DEBUG_PRINTF("Taille du fichier '%s': %lu octets.\r\n", nomFichier, (unsigned long)*pdwTaille);
    return 0;
  }
  else
  {
    DEBUG_PRINTF("Erreur : Impossible d'obtenir les informations du fichier '%s' (%i)\r\n", nomFichier, resultat);
    *pdwTaille = 0;
    return -1;
  }
}

/**
 * @brief Formate la carte SD. Attention : Opération destructive !
 * @note Nécessite que _USE_MKFS soit défini à 1 dans ffconf.h.
 * @retval 0 si succès, -1 si échec.
 */
DRESULT SD_FormaterCarte(void)
{
  // Utiliser _MAX_SS de ffconf.h pour la taille du buffer. Assurez-vous que _MAX_SS est défini dans ffconf.h (ex: 512 ou 4096).
  BYTE work_buffer[_MAX_SS];      // Espace de travail pour f_mkfs
  BYTE format_option = FM_SFD;    // Option de formatage: FM_SFD pour créer une partition. Ou FM_FAT.
  DWORD allocation_unit_size = 0; // Taille de l'unité d'allocation, 0 pour défaut.

  DEBUG_PRINTF("AVERTISSEMENT : Tentative de formatage de la carte SD. Toutes les données seront perdues.\r\n");
  DEBUG_PRINTF("Démontage de la carte SD avant formatage...\r\n");
  f_mount(NULL, "", 0); // D'abord démonter

  DEBUG_PRINTF("Formatage de la carte SD en cours...\r\n");
  FRESULT resultat = f_mkfs("", format_option, allocation_unit_size, work_buffer, sizeof(work_buffer));

  if (resultat != FR_OK)
  {
    DEBUG_PRINTF("Erreur : Impossible de formater la carte SD (%i)\r\n", resultat);
    // Tenter de remonter l'ancien système de fichiers pourrait être pertinent si le formatage échoue tôt
    // Mais si le formatage a commencé, le FS est probablement corrompu.
    // Pour la simplicité, on ne remonte pas ici en cas d'échec de formatage.
    return -1;
  }

  DEBUG_PRINTF("Carte SD formatée avec succès.\r\n");
  DEBUG_PRINTF("Remontage de la carte SD après formatage...\r\n");
  resultat = f_mount(&systemeFichiers, "", 1); // Remonter la carte SD

  if (resultat != FR_OK)
  {
    DEBUG_PRINTF("Erreur : Impossible de remonter la carte SD après formatage (%i)\r\n", resultat);
    return -1;
  }

  DEBUG_PRINTF("Carte SD remontée avec succès après formatage.\r\n");
  return 0;
}
