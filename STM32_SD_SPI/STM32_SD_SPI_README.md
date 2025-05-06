# 3 Fichiers à modifier

1. ## Modifications des interruptions dans `stm32l1xx_it.c` 

Modifiez les sections suivantes dans le fichier `stm32l1xx_it.c` du dossier `Src` de votre projet pour intégrer les fonctionnalités personnalisées :

### Gestion des Timers dans `SysTick_Handler`

Ajoutez la gestion des timers `Timer1` et `Timer2` dans la fonction `SysTick_Handler` entre les balises `/* USER CODE BEGIN SysTick_IRQn 0 */` et `/* USER CODE END SysTick_IRQn 0 */` :


```c
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */
  if (Timer1 > 0)
    Timer1--;
  if (Timer2 > 0)
    Timer2--;
  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */
  /* USER CODE END SysTick_IRQn 1 */
}
```

2. ## Modifications du fichier `user_diskio.c`

Modifiez les sections suivantes dans le fichier `user_diskio.c` du dossier `FATFS/Target/` de votre projet pour intégrer les fonctionnalités personnalisées :


Nous allons remplacer les sections entre les balises `USER CODE BEGIN` et `USER CODE END` par les appels aux fonctions correspondantes de la librairie.

Voici les modifications à apporter :

### Initialisation du disque

Remplacez le code entre `USER CODE BEGIN INIT` et `USER CODE END INIT` :

```c
/* USER CODE BEGIN INIT */
return initialize_disk(pdrv);
/* USER CODE END INIT */
```

### Statut du disque

Remplacez le code entre `USER CODE BEGIN STATUS` et `USER CODE END STATUS` :

```c
/* USER CODE BEGIN STATUS */
	return SD_disk_status(pdrv);
  /* USER CODE END STATUS */
```

### Lecture des secteurs

Remplacez le code entre `USER CODE BEGIN READ` et `USER CODE END READ` :

```c
/* USER CODE BEGIN READ */
return SD_disk_read(pdrv, buff, sector, count);
/* USER CODE END READ */
```

### Écriture des secteurs

Remplacez le code entre `USER CODE BEGIN WRITE` et `USER CODE END WRITE` :

```c
/* USER CODE BEGIN WRITE */
return SD_disk_write(pdrv, buff, sector, count);
/* USER CODE END WRITE */
```

### Commandes de contrôle I/O

Remplacez le code entre `USER CODE BEGIN IOCTL` et `USER CODE END IOCTL` :

```c
/* USER CODE BEGIN IOCTL */
return SD_disk_ioctl(pdrv, cmd, buff);
/* USER CODE END IOCTL */
```

Ces modifications permettent de connecter les fonctions de bas niveau du pilote SPI SD à l'interface FATFS

3. ## Modifications du fichier `TM32_SD_SPI_config.h`

Nous allons modifier le fichier `TM32_SD_SPI_config.h` pour configurer les paramètres spécifiques à votre matériel et application. 

Voici les sections à adapter :

### Définition du SPI utilisé et de la broche CS

```c
/* Configuration du driver HAL */
#include "stm32l4xx_hal.h"            // Remplacer par le driver HAL correspondant à votre carte

/* Configuration des broches pour la carte SD */
// Configuration du SPI
extern SPI_HandleTypeDef  	hspi2;    // Remplacer par votre SPI
#define HSPI_SDCARD     	&hspi2      // Remplacer par votre SPI
#define SPI_TIMEOUT     	100         // Timeout pour les opérations SPI (en ms) 

// Configuration de la broche CS
#define SD_CS_PORT      GPIOB         // Remplacer par votre port
#define SD_CS_PIN       GPIO_PIN_1    // Remplacer par votre pin  
```
