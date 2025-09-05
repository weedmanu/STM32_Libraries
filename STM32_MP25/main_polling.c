/* USER CODE BEGIN Header */

/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "STM32_MP25.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
void display_pm25_data(PM25_FullData *donnees);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
 * @brief  Envoie un octet sur UART via le handle `huart2` (bloquant).
 * @param  ch Caractère à transmettre
 * @retval Le caractère transmis
 */
int __io_putchar(int ch)
{
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_Delay(1000);
  printf("====================================================\n");
  printf("========= DEMO SEN0177 PM2.5 mode polling ==========\n");
  printf("====================================================\n\n");
  PM25_SetDebugMode(1); // Activation du mode debug
  // Initialisation du capteur avec la librairie
  printf("Initialisation du capteur...\n");
  PM25_Polling_Init(&huart1);            // Initialisation du capteur
  PM25_SetControlPin(GPIOA, GPIO_PIN_8); // Configuration de la pin SET
  printf("✅ Capteur initialisé\n");
  printf("====================================================\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    PM25_FullData donnees; // Structure de données complète

    printf("\n====================================================\n\n");
    printf("⏳ Attente de données...\n");

    // Lecture avec la librairie
    uint32_t polling_delay = 5000;
    if (PM25_Polling_ReadFull(&huart1, &donnees, &polling_delay)) // Lecture en continu
    {
      printf("✅ Données reçues\n");
      display_pm25_data(&donnees);
    }
    else
    {
      printf("❌ Erreur lecture\n\n");
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LD2_Pin|GPIO_PIN_8, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD2_Pin PA8 */
  GPIO_InitStruct.Pin = LD2_Pin|GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void display_pm25_data(PM25_FullData *donnees)
{
  // Affichage trame brute directement depuis la structure
  printf("\n=== Trame reçue (32 octets) ===\n");
  for (int i = 0; i < PM25_FRAME_LEN; i++) // PM25_FRAME_LEN = 32
  {
    printf("%02X ", donnees->raw_frame[i]); // Affichage de chaque octet en hexadécimal
  }
  printf("\n");

  // Vérification du checksum
  if (PM25_Validate_Checksum(donnees->raw_frame))
  {
    printf("✅ Checksum valide (0x%04X)\n", donnees->checksum);
  }
  else
  {
    printf("❌ Checksum invalide (0x%04X)\n", donnees->checksum);
  }

  // Données brutes sans interprétation
  printf("\n=== Données brutes ===\n");
  printf("PM1.0 std:%d atm:%d | PM2.5 std:%d atm:%d | PM10 std:%d atm:%d\n",
         donnees->pm1_0_standard, donnees->pm1_0_atmospheric,
         donnees->pm2_5_standard, donnees->pm2_5_atmospheric,
         donnees->pm10_standard, donnees->pm10_atmospheric);
  printf("Particules: 0.3µm:%d 0.5µm:%d 1.0µm:%d 2.5µm:%d 5.0µm:%d 10µm:%d\n",
         donnees->particles_0_3um, donnees->particles_0_5um, donnees->particles_1_0um,
         donnees->particles_2_5um, donnees->particles_5_0um, donnees->particles_10um);
  printf("Version:%d Checksum:0x%04X\n", donnees->version, donnees->checksum);

  // Interprétation qualité PM1.0, PM2.5, PM10 (standard) et ratio
  printf("\n=== Interprétation qualité air ===\n");
  // Récupération des codes d'interprétation
  int code_pm1 = PM25_Quality_GetCode(donnees->pm1_0_standard, "PM1.0");
  int code_pm25 = PM25_Quality_GetCode(donnees->pm2_5_standard, "PM2.5");
  int code_pm10 = PM25_Quality_GetCode(donnees->pm10_standard, "PM10");
  int code_ratio = PM25_Ratio_GetCode(donnees->pm2_5_standard, donnees->pm10_standard);

  // Interprétation des codes
  PM_StatusInfo info_pm1 = PM25_Quality_InterpretCode(code_pm1);
  PM_StatusInfo info_pm25 = PM25_Quality_InterpretCode(code_pm25);
  PM_StatusInfo info_pm10 = PM25_Quality_InterpretCode(code_pm10);
  PM_StatusInfo info_ratio = PM25_Ratio_Interpret(code_ratio);

  // Affichage qualité air et ratio PM
  printf("Qualité PM1.0 : %d µg/m³ -> %s %s (%s)\n", donnees->pm1_0_standard, info_pm1.emoji, info_pm1.label, info_pm1.description);
  printf("Qualité PM2.5 : %d µg/m³ -> %s %s (%s)\n", donnees->pm2_5_standard, info_pm25.emoji, info_pm25.label, info_pm25.description);
  printf("Qualité PM10  : %d µg/m³ -> %s %s (%s)\n", donnees->pm10_standard, info_pm10.emoji, info_pm10.label, info_pm10.description);

  float ratio = donnees->pm10_standard > 0 ? (float)donnees->pm2_5_standard / donnees->pm10_standard : 0.0f;
  printf("\nRatio PM2.5/PM10 : %.2f -> %s %s (%s)\n", ratio, info_ratio.emoji, info_ratio.label, info_ratio.description);
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
