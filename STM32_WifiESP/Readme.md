# Serveur Web Avancé STM32 + ESP01

Ce projet propose un serveur web avancé tournant sur une carte STM32, utilisant un module WiFi ESP01 piloté en UART.  
Il permet de contrôler une LED, d'afficher des informations système, de tester des requêtes GET et de consulter le statut du serveur via une interface web responsive.

## Fonctionnalités principales

- Connexion automatique au WiFi (SSID et mot de passe anonymisés)
- Serveur HTTP multi-route (accueil, LED, test GET, statut, device)
- Contrôle d'une LED via le web
- Affichage dynamique des paramètres GET reçus
- Statistiques serveur et informations système/réseau
- Code modulaire, facilement extensible

---

## Extraits de code utilisateur (`USER CODE`)

### Includes

````c
/* USER CODE BEGIN Includes */
#include <stdio.h>		   // Inclusion de la bibliothèque standard pour les entrées/sorties (pour printf)
#include "STM32_WifiESP.h" // Inclusion du fichier d'en-tête pour le driver ESP01
/* USER CODE END Includes */
````
### Defines 

````c
/* USER CODE BEGIN PD */
#define SSID "XXXXX"
#define PASSWORD "XXXXX"
#define LED_GPIO_PORT GPIOA
#define LED_GPIO_PIN GPIO_PIN_5
/* USER CODE END PD */
````
### Variables

````c
/* USER CODE BEGIN PV */
uint8_t esp01_dma_rx_buf[ESP01_DMA_RX_BUF_SIZE]; // Tampon DMA pour la réception ESP01
volatile bool at_terminal_mode = true;			 // On démarre en mode terminal AT
/* USER CODE END PV */
````
### Redirection de printf vers l'UART2

````c
/* USER CODE BEGIN 0 */

// Redirige printf vers l'UART2 (console série)
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF); // Envoie le caractère sur l'UART2
    return ch;											   // Retourne le caractère envoyé (pour compatibilité printf)
}

// ==================== Constantes HTML et CSS ====================

// --- Parties communes HTML ---
static const char HTML_DOC_START[] = "<!DOCTYPE html><html lang='fr'><head><meta charset='UTF-8'>";
static const char HTML_TITLE_START[] = "<title>";
static const char HTML_TITLE_END_STYLE_START[] = "</title><style>";
static const char HTML_STYLE_END_HEAD_BODY_CARD_START[] = "</style></head><body><div class='card'>";
static const char HTML_CARD_END_BODY_END[] = "</div></body></html>";

// --- CSS Commun ---
static const char PAGE_CSS[] =
    "body{font-family:sans-serif;background:#222;text-align:center;margin:0;padding:0;}"
    ".card{background:linear-gradient(135deg,#c8f7c5 0%,#fff9c4 50%,#ffd6d6 100%);margin:3em auto 0 auto;padding:2.5em 2em 2em 2em;border-radius:18px;box-shadow:0 4px 24px #0004;max-width:420px;display:flex;flex-direction:column;align-items:center;}"
    "h1{color:#2d3a1a;margin-top:0;margin-bottom:1.5em;}";

// ... (handlers HTTP page_root, page_led, page_testget, page_status, page_device) ...
/* USER CODE END 0 */
````
### Initialisation et configuration dans `main()`

````c
/* USER CODE BEGIN 2 */
HAL_Delay(1000);
printf("[ESP01] === Démarrage du programme ===\r\n");
HAL_Delay(500);

ESP01_Status_t status;

// 1. Initialisation du driver ESP01
printf("[ESP01] === Initialisation du driver ESP01 ===\r\n");
status = esp01_init(&huart1, &huart2, esp01_dma_rx_buf, ESP01_DMA_RX_BUF_SIZE);
printf("[ESP01] >>> Initialisation du driver ESP01 : %s\r\n", esp01_get_error_string(status));

// 2. Flush du buffer RX
printf("[ESP01] === Flush RX Buffer ===\r\n");
status = esp01_flush_rx_buffer(500);
printf("[ESP01] >>> Buffer UART/DMA vidé : %s\r\n", esp01_get_error_string(status));
HAL_Delay(100);

// 1. test de communication AT
printf("[ESP01] === Test de communication AT ===\r\n");
status = esp01_test_at(esp01_dma_rx_buf, ESP01_DMA_RX_BUF_SIZE);
printf("[ESP01] >>> Test AT : %s\r\n", esp01_get_error_string(status));

// 2. Test de version AT+GMR
printf("[ESP01] === Lecture version firmware ESP01 (AT+GMR) ===\r\n");
char at_version[128] = {0};
status = esp01_get_at_version(at_version, sizeof(at_version));
printf("[ESP01] >>> Version ESP01 : %s\r\n", at_version);

// 3. Connexion au réseau WiFi
printf("[WIFI] === Connexion au réseau WiFi ===\r\n");
status = esp01_connect_wifi_config(
    ESP01_WIFI_MODE_STA, // mode
    SSID,				 // ssid
    PASSWORD,			 // password
    true,				 // use_dhcp
    NULL,				 // ip
    NULL,				 // gateway
    NULL				 // netmask
);
printf("[WIFI] >>> Connexion WiFi : %s\r\n", esp01_get_error_string(status));

// 5. Activation du mode multi-connexion ET démarrage du serveur web
printf("[WEB] === Activation multi-connexion + démarrage serveur web ===\r\n");
ESP01_Status_t server_status = esp01_start_server_config(
    true, // true = multi-connexion (CIPMUX=1)
    80	  // port du serveur web
);
if (server_status != ESP01_OK)
{
    printf("[WEB] >>> ERREUR: CIPMUX/CIPSERVER\r\n");
    Error_Handler();
}
else
{
    printf("[WEB] >>> Serveur web démarré sur le port 80\r\n");
}

// 7. Ajout des routes HTTP
printf("[WEB] === Ajout des routes HTTP ===\r\n");
esp01_clear_routes();
printf("[WEB] Ajout route /\r\n");
esp01_add_route("/", page_root);
printf("[WEB] Ajout route /status\r\n");
esp01_add_route("/status", page_status);
printf("[WEB] Ajout route /led\r\n");
esp01_add_route("/led", page_led);
printf("[WEB] Ajout route /testget\r\n");
esp01_add_route("/testget", page_testget);
printf("[WEB] Ajout route /device\r\n");
esp01_add_route("/device", page_device);

// 4. Vérification Connexion au réseau WiFi
printf("[WIFI] === Vérification de la connexion WiFi ===\r\n");
esp01_print_connection_status(); // ou le UART de debug que tu utilises
printf("[WIFI] >>> Connexion WiFi : %s\r\n", esp01_get_error_string(esp01_get_connection_status()));

// 8. Affichage de l'adresse IP
printf("[WEB] === Serveur Web prêt ===\r\n");
char ip[32];
if (esp01_get_current_ip(ip, sizeof(ip)) == ESP01_OK)
{
    printf("[WEB] >>> Connectez-vous à : http://%s/\r\n", ip);
}
else
{
    printf("[WIFI] >>> Impossible de récupérer l'IP STA\r\n");
}
/* USER CODE END 2 */
````
### Boucle principale

````c
/* USER CODE BEGIN WHILE */
// === Boucle serveur web ===
while (1)
{
    esp01_process_requests();
    HAL_Delay(10);
}
/* USER CODE END WHILE */
````
### Gestion des erreurs

````c
/* USER CODE BEGIN Error_Handler_Debug */
printf("ERREUR SYSTÈME DÉTECTÉE!\r\n"); // Affiche une erreur système
__disable_irq();						// Désactive les interruptions
while (1)
{
    // Boucle infinie en cas d'erreur
}
/* USER CODE END Error_Handler_Debug */
````

## Pour aller plus loin

- **Ajoutez vos propres routes HTTP** en vous inspirant des handlers fournis (`page_root`, `page_led`, etc.).
- **Modifiez le HTML/CSS** pour personnaliser l'interface web selon vos besoins.
- **Utilisez les sections `USER CODE`** pour intégrer vos propres fonctionnalités sans perdre vos modifications lors des régénérations CubeMX.
- **Exploitez la modularité du code** pour ajouter de nouveaux périphériques ou pages web dynamiques.



