/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c - Version corrigée
 * @brief          : Main program body avec gestion HTTP améliorée
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "STM32_WifiESP.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SSID "freeman"
#define SSID_MDP "manu2612@SOSSO1008"
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart1_rx;

/* USER CODE BEGIN PV */
uint8_t esp01_dma_rx_buf[ESP01_DMA_RX_BUF_SIZE];
char ip_address[32] = "N/A";
char page[4096];
int page_len;
GPIO_PinState led_state = GPIO_PIN_RESET; // Variable pour suivre l'état de la LED
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Fonction qui transmet un caractère via UART et le renvoie.Utilisé pour la sortie standard (printf).
int __io_putchar(int ch)
{
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF); // Pour Envoyer le caractère via UART
  // ITM_SendChar(ch); // Option alternative pour envoyer le caractère via ITM
  return ch;
}

// Fonction pour générer la page HTML avec l'état actuel de la LED
void generate_current_page(void)
{
  const char *status_text = (led_state == GPIO_PIN_SET) ? "ALLUMÉE" : "ÉTEINTE";
  const char *led_class = (led_state == GPIO_PIN_SET) ? "on" : "off";

  page_len = snprintf(page, sizeof(page),
                      "HTTP/1.1 200 OK\r\n"
                      "Content-Type: text/html; charset=UTF-8\r\n"
                      "\r\n"
                      "<!DOCTYPE html><html lang='fr'><head><meta charset='UTF-8'>"
                      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                      "<title>STM32 LED</title>"
                      "<style>"
                      "body{margin:0;background:linear-gradient(135deg, #181c24, #23272e);color:#e0e0e0;font-family:'Arial', sans-serif;height:100vh;display:flex;justify-content:center;align-items:center;}"
                      ".card{max-width:440px;width:100%%;background:#23272e;border-radius:14px;box-shadow:0 10px 25px rgba(0, 255, 224, 0.2);padding:32px 22px 26px;transition:transform 0.3s ease, box-shadow 0.3s ease;}"
                      ".card:hover{transform:translateY(-5px);box-shadow:0 15px 30px rgba(0, 255, 224, 0.3);}"
                      "h1{text-align:center;margin-bottom:22px;font-size:1.7em;letter-spacing:1px;text-shadow:0 0 8px #00ffe0;color:#00ffe0;}"
                      ".logo{display:block;margin:0 auto 20px;width:100px;height:auto;}"
                      ".led{display:inline-block;width:22px;height:22px;border-radius:50%%;margin-right:10px;vertical-align:middle;box-shadow:0 0 16px rgba(0, 255, 224, 0.5);transition:all 0.3s ease;}"
                      ".on{background:#00ffe0;box-shadow:0 0 24px #00ffe0, 0 0 8px #fff;}"
                      ".off{background:#ff1744;box-shadow:0 0 12px rgba(255, 23, 68, 0.5);}"
                      ".status{text-align:center;margin-bottom:20px;font-size:1.15em;}"
                      ".ip{color:#00ffe0;text-align:center;font-size:.98em;margin-bottom:20px;}"
                      ".btns{display:flex;justify-content:center;gap:16px;}"
                      ".btn{padding:11px 28px;border-radius:7px;border:none;font-weight:bold;font-size:1em;cursor:pointer;text-decoration:none;transition:all 0.2s ease;background:#00ffe0;color:#222;box-shadow:0 2px 8px rgba(0, 255, 224, 0.3);}"
                      ".btn.off{background:#ff1744;color:#fff;}"
                      ".btn:hover{filter:brightness(1.12);transform:scale(1.05);}"
                      ".footer{text-align:center;color:#888;font-size:.93em;margin-top:28px;}"
                      "</style></head><body>"
                      "<div class='card'>"
                      "<img class='logo' src='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAA...' alt='Logo ST'>"
                      "<h1>Contrôle LED STM32</h1>"
                      "<div class='status'><span class='led %s'></span>LED %s</div>"
                      "<div class='ip'>IP : <b>%s</b></div>"
                      "<div class='btns'>"
                      "<a class='btn' href='/on'>Allumer</a>"
                      "<a class='btn off' href='/off'>Éteindre</a>"
                      "</div>"
                      "<div class='footer'>STM32 + ESP01 &copy; 2025</div>"
                      "</div></body></html>",
                      led_class, status_text, ip_address);
}

void page_on(int conn_id)
{
  printf("page_on: Entrée. conn_id: %d. État LED actuel (avant modif): %s\r\n", conn_id, (led_state == GPIO_PIN_SET) ? "SET" : "RESET");

  led_state = GPIO_PIN_SET;
  printf("page_on: led_state mis à GPIO_PIN_SET.\r\n");

  // Test immédiat de la LED avant de générer/envoyer la page
  printf("page_on: Tentative d'allumage de la LED (écriture directe GPIO_PIN_SET).\r\n");
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
  // Petite pause pour observer si la LED s'allume, même brièvement
  // HAL_Delay(100); // Décommentez pour un test visuel si nécessaire

  generate_current_page();
  printf("page_on: Page HTML générée (devrait indiquer LED ALLUMÉE).\r\n");

  handle_http_request(conn_id, page, page_len);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, led_state); // Allume la LED
  printf("page_on: handle_http_request terminé.\r\n");

  // Vérification finale de led_state et écriture GPIO
  printf("page_on: État LED avant écriture finale: %s. Tentative d'écriture GPIO avec cet état.\r\n", (led_state == GPIO_PIN_SET) ? "SET" : "RESET");
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, led_state);
  printf("page_on: Écriture GPIO finale effectuée. La LED devrait être ALLUMÉE.\r\n");
}

void page_off(int conn_id)
{
  printf("page_off: Entrée. conn_id: %d. État LED actuel (avant modif): %s\r\n", conn_id, (led_state == GPIO_PIN_SET) ? "SET" : "RESET");

  led_state = GPIO_PIN_RESET;
  printf("page_off: led_state mis à GPIO_PIN_RESET.\r\n");

  printf("page_off: Tentative d'extinction de la LED (écriture directe GPIO_PIN_RESET).\r\n");
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
  // HAL_Delay(100); // Décommentez pour un test visuel si nécessaire

  generate_current_page();
  printf("page_off: Page HTML générée (devrait indiquer LED ÉTEINTE).\r\n");

  handle_http_request(conn_id, page, page_len);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, led_state); // Éteint la LED
  printf("page_off: handle_http_request terminé.\r\n");

  printf("page_off: État LED avant écriture finale: %s. Tentative d'écriture GPIO avec cet état.\r\n", (led_state == GPIO_PIN_SET) ? "SET" : "RESET");
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, led_state);
  printf("page_off: Écriture GPIO finale effectuée. La LED devrait être ÉTEINTE.\r\n");
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
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  // Initialiser la LED éteinte
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
  led_state = GPIO_PIN_RESET;
  HAL_Delay(1000);

  // Test LED : clignote 3 fois au démarrage
  for (int i = 0; i < 3; i++)
  {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
    HAL_Delay(200);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
    HAL_Delay(200);
  }

  printf("=== ESP01 Web Server Start ===\r\n");

  // Initialisation complète ESP01 + WiFi + serveur web
  if (!init_esp01_serveur_web(&huart1, &huart2, esp01_dma_rx_buf, ESP01_DMA_RX_BUF_SIZE, SSID, SSID_MDP))
  {
    printf("ERREUR: Initialisation ESP01/WiFi/Serveur\r\n");
    Error_Handler();
  }

  // Récupérer l'adresse IP
  if (esp01_get_current_ip(ip_address, sizeof(ip_address)) == ESP01_OK)
  {
    printf("IP récupérée: %s\r\n", ip_address);
  }

  // Générer la page HTML initiale
  generate_current_page();
  printf("Page HTML créée - Taille totale: %d octets\r\n", page_len);

  // Ajouter les routes
  esp01_add_route("/on", page_on);
  esp01_add_route("/off", page_off);

  printf("=== Serveur Web Prêt ===\r\n");
  printf("Connectez-vous à: http://%s/\r\n", ip_address);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    esp01_server_handle();
  }
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */

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
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
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
  huart1.Init.BaudRate = 115200;
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
 * Enable DMA controller clock
 */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
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
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0 | LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PA0 LD2_Pin */
  GPIO_InitStruct.Pin = GPIO_PIN_0 | LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  printf("ERREUR SYSTÈME DÉTECTÉE!\r\n");
  __disable_irq();
  while (1)
  {
    // Boucle infinie en cas d'erreur
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
