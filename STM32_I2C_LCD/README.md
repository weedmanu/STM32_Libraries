# Librairie pour utiliser un écran LCD I2C 16x2 ou 20x4 avec une STM32 et STM32CubeIDE


1. Dans le header **STM32_I2C_LCD.h** on adapte la ligne 6 qui concerne l'insertion des librairies correspondant à la carte STM32 :

```c
     #include "stm32f1xx_hal.h"
```
2. Il n'y a que l'I2C à activer dans le .ioc et connecter l’écran sur les pins GPIO correspondantes.

3. L'I2C utilisé, le type d'écran et l'adresse I2C se configurent directement par la fonction lcd_init:    
  
```c
     void lcd_init(I2C_HandleTypeDef *hi2c, uint8_t columns, uint8_t rows, uint8_t i2c_address);
```

4. Le fichier **USER_CODE_main.c** contient tous les block de code nécessaires pour faire un programme de demo de la librairies. 

Il faudra adapter le USER CODE BEGIN PD : 

```c
     /* USER CODE BEGIN PD */
     #define COLUMNS	20	 // Nombre de colonnes de l'écran LCD
     #define ROWS		4     // Nombre de lignes de l'écran LCD
     #define I2C_ADDRESS 0x3f	 // Adresse I2C de l'écran LCD
     /* USER CODE END PD */
```
et choisir le votre I2C dans la fonction **lcd_init**, exemple :

```c
     lcd_init(&hi2c1, COLUMNS, ROWS, I2C_ADDRESS);
 ```
