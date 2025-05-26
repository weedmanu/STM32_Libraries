/*
 * STM32_WifiESP.c
 * Driver STM32 <-> ESP01 (AT)
 * Version structurée avec sous-fonctions pour chaque étape
 * 2025 - weedm - CORRIGÉ
 */

#include "STM32_WifiESP.h"

#define ESP01_MAX_ROUTES 8

static esp01_route_t g_routes[ESP01_MAX_ROUTES];
static int g_route_count = 0;

// ==================== Variables globales internes ====================
static UART_HandleTypeDef *g_esp_uart = NULL;
static UART_HandleTypeDef *g_debug_uart = NULL;
static uint8_t *g_dma_rx_buf = NULL;
static uint16_t g_dma_buf_size = 0;
static volatile uint16_t g_rx_last_pos = 0;
static char g_accumulator[ESP01_DMA_RX_BUF_SIZE * 2];
static uint16_t g_acc_len = 0;

// Prototypes des fonctions internes
static void _flush_rx_buffer(uint32_t timeout_ms);

// ==================== Fonctions internes (static) ====================

// --- Log debug ---
#if ESP01_DEBUG
static void _esp_logln(const char *msg)
{
	if (g_debug_uart && msg)
	{
		HAL_UART_Transmit(g_debug_uart, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
		HAL_UART_Transmit(g_debug_uart, (uint8_t *)"\r\n", 2, HAL_MAX_DELAY);
	}
}
#else
static void _esp_logln(const char *msg) { (void)msg; }
#endif

// Calcule la position actuelle d'écriture du DMA
static uint16_t _get_dma_write_pos(void)
{
	return g_dma_buf_size - __HAL_DMA_GET_COUNTER(g_esp_uart->hdmarx);
}

// Calcule le nombre de nouveaux octets disponibles
static uint16_t _get_available_bytes(void)
{
	uint16_t write_pos = _get_dma_write_pos();
	if (write_pos >= g_rx_last_pos)
		return write_pos - g_rx_last_pos;
	else
		return (g_dma_buf_size - g_rx_last_pos) + write_pos;
}

// Accumule les données et cherche un pattern
static bool _accumulate_and_search(const char *pattern, uint32_t timeout_ms, bool clear_first)
{
	uint32_t start_tick = HAL_GetTick();
	char temp_buf[256];

	if (clear_first)
	{
		g_acc_len = 0;
		g_accumulator[0] = '\0';
	}

	while ((HAL_GetTick() - start_tick) < timeout_ms)
	{
		uint16_t new_data = esp01_get_new_data((uint8_t *)temp_buf, sizeof(temp_buf));
		if (new_data > 0)
		{
			uint16_t space_left = sizeof(g_accumulator) - g_acc_len - 1;
			uint16_t to_add = (new_data < space_left) ? new_data : space_left;
			if (to_add > 0)
			{
				memcpy(&g_accumulator[g_acc_len], temp_buf, to_add);
				g_acc_len += to_add;
				g_accumulator[g_acc_len] = '\0';
			}
			if (strstr(g_accumulator, pattern))
				return true;
		}
		else
		{
			HAL_Delay(10);
		}
	}
	return false;
}

// ==================== Fonctions bas niveau (driver) ====================

ESP01_Status_t esp01_init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug,
						  uint8_t *dma_rx_buf, uint16_t dma_buf_size)
{
	g_esp_uart = huart_esp;
	g_debug_uart = huart_debug;
	g_dma_rx_buf = dma_rx_buf;
	g_dma_buf_size = dma_buf_size;
	g_rx_last_pos = 0;
	g_acc_len = 0;
	g_accumulator[0] = '\0';

	if (!g_esp_uart || !g_dma_rx_buf || g_dma_buf_size == 0)
	{
		_esp_logln("ESP01 Init Error: Invalid params");
		return ESP01_NOT_INITIALIZED;
	}

	if (HAL_UART_Receive_DMA(g_esp_uart, g_dma_rx_buf, g_dma_buf_size) != HAL_OK)
	{
		_esp_logln("ESP01 Init Error: HAL_UART_Receive_DMA failed");
		return ESP01_FAIL;
	}

	_esp_logln("--- ESP01 Driver Init OK ---");
	HAL_Delay(100);
	return ESP01_OK;
}

uint16_t esp01_get_new_data(uint8_t *buffer, uint16_t buffer_size)
{
	if (!g_esp_uart || !g_dma_rx_buf || !buffer || buffer_size == 0)
	{
		if (buffer && buffer_size > 0)
			buffer[0] = '\0';
		return 0;
	}

	uint16_t available = _get_available_bytes();
	if (available == 0)
	{
		buffer[0] = '\0';
		return 0;
	}

	uint16_t to_copy = (available < buffer_size - 1) ? available : buffer_size - 1;
	uint16_t copied = 0;
	uint16_t write_pos = _get_dma_write_pos();

	if (write_pos > g_rx_last_pos)
	{
		memcpy(buffer, &g_dma_rx_buf[g_rx_last_pos], to_copy);
		copied = to_copy;
	}
	else
	{
		uint16_t first_chunk = g_dma_buf_size - g_rx_last_pos;
		if (first_chunk >= to_copy)
		{
			memcpy(buffer, &g_dma_rx_buf[g_rx_last_pos], to_copy);
			copied = to_copy;
		}
		else
		{
			memcpy(buffer, &g_dma_rx_buf[g_rx_last_pos], first_chunk);
			copied = first_chunk;
			uint16_t remaining = to_copy - first_chunk;
			if (remaining > 0)
			{
				memcpy(buffer + first_chunk, &g_dma_rx_buf[0], remaining);
				copied += remaining;
			}
		}
	}

	buffer[copied] = '\0';
	g_rx_last_pos = (g_rx_last_pos + copied) % g_dma_buf_size;
	return copied;
}

// ==================== Commandes AT génériques ====================

ESP01_Status_t esp01_send_raw_command_dma(const char *cmd, char **response_buffer,
										  uint32_t max_response_size,
										  const char *expected_terminator,
										  uint32_t timeout_ms)
{
	// Validation des paramètres
	if (!g_esp_uart || !cmd || !response_buffer)
	{
		if (response_buffer)
			*response_buffer = NULL;
		return ESP01_NOT_INITIALIZED;
	}

	// Allocation avec vérification
	*response_buffer = (char *)calloc(max_response_size, 1);
	if (!*response_buffer)
	{
		_esp_logln("ESP01: Échec allocation mémoire");
		return ESP01_BUFFER_TOO_SMALL;
	}

	const char *terminator = expected_terminator ? expected_terminator : "OK";

	// Préparer la commande complète
	char full_cmd[256];
	int cmd_len = snprintf(full_cmd, sizeof(full_cmd), "%s\r\n", cmd);

	// Vider le buffer avant envoi
	_flush_rx_buffer(100);

	// Envoyer la commande
	if (HAL_UART_Transmit(g_esp_uart, (uint8_t *)full_cmd, cmd_len, 1000) != HAL_OK)
	{
		free(*response_buffer);
		*response_buffer = NULL;
		return ESP01_FAIL;
	}

	// Attendre et accumuler la réponse
	bool found = _accumulate_and_search(terminator, timeout_ms, true);

	// Copier la réponse
	uint32_t copy_len = (g_acc_len < max_response_size - 1) ? g_acc_len : max_response_size - 1;
	memcpy(*response_buffer, g_accumulator, copy_len);
	(*response_buffer)[copy_len] = '\0';

	return found ? ESP01_OK : ESP01_TIMEOUT;
}

ESP01_Status_t esp01_terminal_command(UART_HandleTypeDef *huart_terminal, uint32_t max_cmd_size,
									  uint32_t max_resp_size, uint32_t timeout_ms)
{
	if (!huart_terminal)
		return ESP01_FAIL;

	char cmd_buf[128] = {0};
	char *resp_buf = NULL;

	const char *prompt = "\r\nCommande AT: ";
	HAL_UART_Transmit(huart_terminal, (uint8_t *)prompt, strlen(prompt), HAL_MAX_DELAY);

	uint32_t idx = 0;
	uint8_t c = 0;
	while (idx < sizeof(cmd_buf) - 1)
	{
		if (HAL_UART_Receive(huart_terminal, &c, 1, HAL_MAX_DELAY) == HAL_OK)
		{
			if (c == '\r' || c == '\n')
			{
				if (idx > 0)
					break;
				continue;
			}
			else if (c == 0x08 || c == 0x7F)
			{
				if (idx > 0)
				{
					idx--;
					HAL_UART_Transmit(huart_terminal, (uint8_t *)"\b \b", 3, HAL_MAX_DELAY);
				}
			}
			else if (c >= 0x20 && c <= 0x7E)
			{
				cmd_buf[idx++] = c;
				HAL_UART_Transmit(huart_terminal, &c, 1, HAL_MAX_DELAY);
			}
		}
	}
	cmd_buf[idx] = '\0';
	HAL_UART_Transmit(huart_terminal, (uint8_t *)"\r\n", 2, HAL_MAX_DELAY);

	if (strlen(cmd_buf) == 0)
		return ESP01_OK;
	if (strcmp(cmd_buf, "exit") == 0)
		return ESP01_FAIL;

	ESP01_Status_t status = esp01_send_raw_command_dma(cmd_buf, &resp_buf, max_resp_size, NULL, timeout_ms);

	if (resp_buf)
	{
		char status_msg[64];
		snprintf(status_msg, sizeof(status_msg), "\r\nRéponse (status %d):\r\n", status);
		HAL_UART_Transmit(huart_terminal, (uint8_t *)status_msg, strlen(status_msg), HAL_MAX_DELAY);
		HAL_UART_Transmit(huart_terminal, (uint8_t *)resp_buf, strlen(resp_buf), HAL_MAX_DELAY);
		free(resp_buf);
	}
	else
	{
		char err_msg[64];
		snprintf(err_msg, sizeof(err_msg), "\r\nPas de réponse (status %d)\r\n", status);
		HAL_UART_Transmit(huart_terminal, (uint8_t *)err_msg, strlen(err_msg), HAL_MAX_DELAY);
	}

	return status;
}

// ==================== Fonctions haut niveau (WiFi & serveur) ====================

ESP01_Status_t esp01_test_at(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug, uint8_t *dma_rx_buf, uint16_t dma_buf_size)
{
	char *response = NULL;
	ESP01_Status_t status = esp01_init(huart_esp, huart_debug, dma_rx_buf, dma_buf_size);
	if (status != ESP01_OK)
		return status;
	status = esp01_send_raw_command_dma("AT", &response, 256, "OK", 2000);
	if (response)
		free(response);
	return status;
}

ESP01_Status_t esp01_set_wifi_mode(ESP01_WifiMode_t mode)
{
	char *response = NULL;
	char cmd[32];
	snprintf(cmd, sizeof(cmd), "AT+CWMODE=%d", (int)mode);
	ESP01_Status_t status = esp01_send_raw_command_dma(cmd, &response, 128, "OK", 2000);
	if (response)
		free(response);
	return status;
}

ESP01_Status_t esp01_connect_wifi(const char *ssid, const char *password)
{
	char *response = NULL;
	char cmd[128];
	snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
	ESP01_Status_t status = esp01_send_raw_command_dma(cmd, &response, 512, "WIFI GOT IP", 15000);
	if (response)
		free(response);
	return status;
}

ESP01_Status_t esp01_start_web_server(uint16_t port)
{
	char *resp = NULL;
	char cmd[32];
	snprintf(cmd, sizeof(cmd), "AT+CIPSERVER=1,%u", port);
	ESP01_Status_t status = esp01_send_raw_command_dma(cmd, &resp, 128, "OK", 3000);
	if (resp)
	{
		// Accepte aussi "no change" ou "ERROR" si déjà lancé
		if (strstr(resp, "OK") || strstr(resp, "no change"))
		{
			free(resp);
			return ESP01_OK;
		}
		free(resp);
	}
	return status;
}

/**
 * @brief Gère l'initialisation complète de l'ESP01 et la connexion WiFi
 */
bool init_esp01_serveur_web(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug,
							uint8_t *dma_rx_buf, uint16_t dma_buf_size,
							const char *ssid, const char *password)
{
	char *response = NULL;
	ESP01_Status_t status;

	// 1. Initialiser le driver
	if (esp01_init(huart_esp, huart_debug, dma_rx_buf, dma_buf_size) != ESP01_OK)
		return false;
	HAL_Delay(1000);

	// 2. Test AT
	status = esp01_send_raw_command_dma("AT", &response, ESP01_DMA_RX_BUF_SIZE, "OK", 2000);
	if (status != ESP01_OK || !response || !strstr(response, "OK"))
	{
		_esp_logln("ERREUR: Test AT échoué");
		if (response)
			free(response);
		return false;
	}
	_esp_logln("Test AT: OK");
	free(response);
	HAL_Delay(1000);

	// 3. Mode station
	status = esp01_send_raw_command_dma("AT+CWMODE=1", &response, ESP01_DMA_RX_BUF_SIZE, "OK", 5000);
	if (status != ESP01_OK || !response || !strstr(response, "OK"))
	{
		_esp_logln("ERREUR: Mode station échoué");
		if (response)
			free(response);
		return false;
	}
	_esp_logln("Mode station: OK");
	free(response);
	HAL_Delay(1000);

	// 4. Connexion WiFi
	char wifi_cmd[512];
	snprintf(wifi_cmd, sizeof(wifi_cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
	status = esp01_send_raw_command_dma(wifi_cmd, &response, ESP01_DMA_RX_BUF_SIZE, "WIFI GOT IP", 15000);
	if (status != ESP01_OK || !response || !strstr(response, "WIFI GOT IP"))
	{
		_esp_logln("ERREUR: Connexion WiFi échouée");
		if (response)
			free(response);
		return false;
	}
	_esp_logln("Connexion WiFi: OK");
	free(response);
	HAL_Delay(1000);

	// 5. Mode multi-connexion
	status = esp01_send_raw_command_dma("AT+CIPMUX=1", &response, ESP01_DMA_RX_BUF_SIZE, "OK", 5000);
	if (status != ESP01_OK || !response || !strstr(response, "OK"))
	{
		_esp_logln("ERREUR: Mode multi-connexion échoué");
		if (response)
			free(response);
		return false;
	}
	_esp_logln("Mode multi-connexion: OK");
	free(response);
	HAL_Delay(1000);

	// 6. Démarrer le serveur
	status = esp01_send_raw_command_dma("AT+CIPSERVER=1,80", &response, ESP01_DMA_RX_BUF_SIZE, "OK", 2000);
	if (status != ESP01_OK && (!response || (!strstr(response, "OK") && !strstr(response, "no change"))))
	{
		_esp_logln("Echec démarrage serveur, tentative d'arrêt puis redémarrage...");
		if (response)
		{
			free(response);
			response = NULL;
		}
		HAL_Delay(1500);

		status = esp01_send_raw_command_dma("AT+CIPSERVER=0", &response, ESP01_DMA_RX_BUF_SIZE, "OK", 5000);
		if (response)
		{
			free(response);
			response = NULL;
		}
		HAL_Delay(2000);

		// Retenter de démarrer le serveur
		status = esp01_send_raw_command_dma("AT+CIPSERVER=1,80", &response, ESP01_DMA_RX_BUF_SIZE, "OK", 5000);
		if (status != ESP01_OK && (!response || (!strstr(response, "OK") && !strstr(response, "no change"))))
		{
			_esp_logln("ERREUR: Démarrage serveur échoué (après reset)");
			if (response)
				free(response);
			return false;
		}
	}
	_esp_logln("Serveur web: OK");
	if (response)
		free(response);
	HAL_Delay(1500);

	// Vider le buffer de réception pour éviter la pollution
	_flush_rx_buffer(300);
	HAL_Delay(200);

	char ip_address[32] = "N/A";
	status = esp01_get_current_ip(ip_address, sizeof(ip_address));
	if (status != ESP01_OK)
	{
		_esp_logln("ERREUR: IP non récupérée !");
		return false;
	}
	_esp_logln("IP récupérée : OK");
	char msg[128];
	snprintf(msg, 128, "Connectez-vous à l'URL: http://%s/", ip_address);
	_esp_logln(msg);
	return true;
}

// ==================== Fonctions HTTP & utilitaires ====================

http_request_t parse_ipd_header(const char *data)
{
	http_request_t request = {0};

	char *ipd_start = strstr(data, "+IPD,");
	if (!ipd_start)
		return request; // is_valid reste false

	// Parser: +IPD,conn_id,length:
	int parsed = sscanf(ipd_start, "+IPD,%d,%d:", &request.conn_id, &request.content_length);

	if (parsed == 2 && request.conn_id >= 0 && request.content_length >= 0)
	{
		request.is_valid = true;

		// Chercher le début du payload HTTP
		char *colon_pos = strchr(ipd_start, ':');
		if (colon_pos)
		{
			colon_pos++; // Passer le ':'

			// Vérifier si c'est une requête HTTP (GET, POST, PUT, etc.)
			if (strncmp(colon_pos, "GET ", 4) == 0 ||
				strncmp(colon_pos, "POST ", 5) == 0 ||
				strncmp(colon_pos, "PUT ", 4) == 0 ||
				strncmp(colon_pos, "DELETE ", 7) == 0)
			{
				request.is_http_request = true;

				if (ESP01_DEBUG)
				{
					char method[10] = {0};
					sscanf(colon_pos, "%9s", method);
					char debug_msg[50];
					snprintf(debug_msg, sizeof(debug_msg), "Méthode HTTP détectée: %s", method);
					_esp_logln(debug_msg);
				}
			}
		}
	}

	return request;
}

// Fonction pour vider le payload HTTP
void discard_http_payload(int expected_length)
{
	char discard_buf[ESP01_DMA_RX_BUF_SIZE];
	int remaining = expected_length;
	uint32_t timeout_start = HAL_GetTick();

	_esp_logln("Vidage du payload HTTP...");

	while (remaining > 0 && (HAL_GetTick() - timeout_start) < 3000)
	{
		int to_read = (remaining < sizeof(discard_buf)) ? remaining : sizeof(discard_buf);
		uint16_t read_len = esp01_get_new_data((uint8_t *)discard_buf, to_read);

		if (read_len > 0)
		{
			remaining -= read_len;
			if (ESP01_DEBUG)
			{
				char debug_msg[50];
				snprintf(debug_msg, sizeof(debug_msg), "Ignoré %d octets, reste %d", read_len, remaining);
				_esp_logln(debug_msg);
			}
		}
		else
		{
			// Si il reste très peu à lire, tente de forcer la lecture
			if (remaining <= 2)
			{
				uint16_t extra = esp01_get_new_data((uint8_t *)discard_buf, remaining);
				if (extra > 0)
					remaining -= extra;
			}
			HAL_Delay(10);
		}
	}

	// Vider complètement le buffer DMA
	HAL_Delay(100);
	char temp_buf[ESP01_DMA_RX_BUF_SIZE];
	int flush_count = 0;

	while (remaining > 0 && flush_count < 10)
	{
		uint16_t extra = esp01_get_new_data((uint8_t *)temp_buf, remaining);
		if (extra > 0)
		{
			remaining -= extra;
			if (ESP01_DEBUG)
			{
				char debug_msg[100];
				snprintf(debug_msg, sizeof(debug_msg), "Flush final: ignoré %u octets, reste %d",
						 (unsigned int)extra, remaining);
				_esp_logln(debug_msg);
			}
		}
		else
		{
			HAL_Delay(10);
		}
		flush_count++;
	}

	if (remaining > 0)
	{
		char warn_msg[100];
		snprintf(warn_msg, sizeof(warn_msg), "AVERTISSEMENT: %d octets non lus", remaining);
		_esp_logln(warn_msg);
	}
	else
	{
		_esp_logln("Payload HTTP complètement vidé");
	}
}

// Fonction pour vider complètement le buffer de réception
static void _flush_rx_buffer(uint32_t timeout_ms)
{
	char temp_buf[ESP01_DMA_RX_BUF_SIZE];
	uint32_t start_time = HAL_GetTick();

	while ((HAL_GetTick() - start_time) < timeout_ms)
	{
		uint16_t len = esp01_get_new_data((uint8_t *)temp_buf, sizeof(temp_buf));
		if (len == 0)
		{
			HAL_Delay(10);
		}
		else
		{
			if (ESP01_DEBUG)
			{
				temp_buf[len] = '\0';
				_esp_logln("Buffer vidé:");
				_esp_logln(temp_buf);
			}
		}
	}
}

// Fonction handle_http_request corrigée
void handle_http_request(int conn_id, const char *page, size_t page_len)
{
	char *response = NULL;
	ESP01_Status_t status;

	if (ESP01_DEBUG)
	{
		char debug_msg[100];
		snprintf(debug_msg, sizeof(debug_msg),
				 "=== DEBUT TRAITEMENT HTTP - conn_id: %d, taille: %d ===",
				 conn_id, (int)page_len);
		_esp_logln(debug_msg);
	}

	// ÉTAPE 1: Vider complètement le buffer avant d'envoyer CIPSEND
	_esp_logln("Vidage du buffer de réception...");
	_flush_rx_buffer(500);

	// ÉTAPE 2: Attendre un peu pour être sûr que tout est stable
	HAL_Delay(100);

	// ÉTAPE 3: Envoyer CIPSEND
	char cipsend_cmd[32];
	snprintf(cipsend_cmd, sizeof(cipsend_cmd), "AT+CIPSEND=%d,%d", conn_id, (int)page_len);

	_esp_logln("Envoi commande CIPSEND...");
	status = esp01_send_raw_command_dma(cipsend_cmd, &response, 256, ">", 5000);

	if (status == ESP01_OK && response && strstr(response, ">"))
	{
		_esp_logln("✅ Prompt '>' reçu, envoi de la page...");
		if (response)
		{
			free(response);
			response = NULL;
		}

		// ÉTAPE 4: Envoyer la page HTML
		if (HAL_UART_Transmit(g_esp_uart, (uint8_t *)page, page_len, 5000) == HAL_OK)
		{
			_esp_logln("✅ Page HTML transmise, attente SEND OK...");

			// ÉTAPE 5: Attendre "SEND OK"
			status = esp01_send_raw_command_dma("", &response, 512, "SEND OK", 8000);
			if (status == ESP01_OK && response && strstr(response, "SEND OK"))
			{
				_esp_logln("✅ SEND OK reçu - Transmission réussie !");
			}
			else
			{
				_esp_logln("❌ ERREUR: SEND OK non reçu");
				if (response)
				{
					_esp_logln("Réponse reçue après envoi:");
					_esp_logln(response);
				}
			}
		}
		else
		{
			_esp_logln("❌ ERREUR: Échec transmission UART");
		}
	}
	else
	{
		_esp_logln("❌ ERREUR: Prompt '>' non reçu");
		if (response)
		{
			_esp_logln("Réponse CIPSEND reçue:");
			_esp_logln(response);
		}

		// Diagnostic: vérifier si on reçoit encore des données HTTP
		char diag_buf[ESP01_DMA_RX_BUF_SIZE];
		uint16_t diag_len = esp01_get_new_data((uint8_t *)diag_buf, sizeof(diag_buf) - 1);
		if (diag_len > 0)
		{
			diag_buf[diag_len] = '\0';
			_esp_logln("Données supplémentaires détectées:");
			_esp_logln(diag_buf);
		}
	}

	if (response)
	{
		free(response);
		response = NULL;
	}

	// ÉTAPE 6: Attendre avant fermeture
	HAL_Delay(200);

	// ÉTAPE 7: Fermer la connexion
	_esp_logln("Fermeture de la connexion...");
	char cipclose_cmd[32];
	snprintf(cipclose_cmd, sizeof(cipclose_cmd), "AT+CIPCLOSE=%d", conn_id);
	status = esp01_send_raw_command_dma(cipclose_cmd, &response, 256, "OK", 3000);

	if (status == ESP01_OK && response && strstr(response, "OK"))
	{
		_esp_logln("✅ Connexion fermée avec succès");
	}
	else
	{
		_esp_logln("⚠️ AVERTISSEMENT: Problème fermeture connexion");
		if (response)
		{
			_esp_logln("Réponse CIPCLOSE:");
			_esp_logln(response);
		}
	}

	if (response)
		free(response);

	_esp_logln("=== FIN TRAITEMENT HTTP ===");

	// ÉTAPE 8: Vider le buffer une dernière fois pour éviter la pollution
	_flush_rx_buffer(200);
}

// ==================== Fonctions utilitaires réseau ====================

ESP01_Status_t esp01_get_current_ip(char *ip_buffer, size_t buffer_size)
{
	char *response = NULL;
	ESP01_Status_t status = esp01_send_raw_command_dma("AT+CIFSR", &response, 512, "OK", 5000);

	if (status != ESP01_OK || !response)
	{
		if (response)
			free(response);
		return ESP01_FAIL;
	}

	// Chercher la ligne STAIP
	char *staip_line = strstr(response, "STAIP,\"");
	if (staip_line)
	{
		staip_line += 7; // Passer "STAIP,\""
		char *end_quote = strchr(staip_line, '"');
		if (end_quote)
		{
			size_t ip_len = end_quote - staip_line;
			if (ip_len < buffer_size)
			{
				strncpy(ip_buffer, staip_line, ip_len);
				ip_buffer[ip_len] = '\0';
				free(response);
				return ESP01_OK;
			}
		}
	}

	// Si pas trouvé, mettre une IP par défaut
	strncpy(ip_buffer, "192.168.1.100", buffer_size - 1);
	ip_buffer[buffer_size - 1] = '\0';

	free(response);
	return ESP01_FAIL;
}

ESP01_Status_t esp01_disconnect_wifi(void)
{
	char *response = NULL;
	ESP01_Status_t status = esp01_send_raw_command_dma("AT+CWQAP", &response, 256, "OK", 5000);
	if (response)
		free(response);
	return status;
}

ESP01_Status_t esp01_reset(void)
{
	char *response = NULL;
	ESP01_Status_t status = esp01_send_raw_command_dma("AT+RST", &response, 256, "ready", 10000);
	if (response)
		free(response);

	// Après un reset, attendre et réinitialiser les variables
	HAL_Delay(2000);
	g_rx_last_pos = 0;
	g_acc_len = 0;
	g_accumulator[0] = '\0';

	return status;
}

// ==================== Système de routage HTTP ====================

ESP01_Status_t esp01_add_route(const char *path, esp01_route_handler_t handler)
{
	if (!path || !handler || g_route_count >= ESP01_MAX_ROUTES)
	{
		return ESP01_FAIL;
	}

	// Vérifier si la route existe déjà
	for (int i = 0; i < g_route_count; i++)
	{
		if (strcmp(g_routes[i].path, path) == 0)
		{
			// Remplacer le handler existant
			g_routes[i].handler = handler;
			return ESP01_OK;
		}
	}

	// Ajouter nouvelle route
	strncpy(g_routes[g_route_count].path, path, sizeof(g_routes[g_route_count].path) - 1);
	g_routes[g_route_count].path[sizeof(g_routes[g_route_count].path) - 1] = '\0';
	g_routes[g_route_count].handler = handler;
	g_route_count++;

	return ESP01_OK;
}

esp01_route_handler_t esp01_find_route_handler(const char *path)
{
	if (!path)
		return NULL;

	for (int i = 0; i < g_route_count; i++)
	{
		if (strcmp(g_routes[i].path, path) == 0)
		{
			return g_routes[i].handler;
		}
	}

	return NULL;
}

void esp01_clear_routes(void)
{
	g_route_count = 0;
	memset(g_routes, 0, sizeof(g_routes));
}

// ==================== Fonctions de parsing HTTP ====================

ESP01_Status_t esp01_parse_http_request(const char *raw_request, http_parsed_request_t *parsed)
{
	if (!raw_request || !parsed)
		return ESP01_FAIL;

	// Initialiser la structure
	memset(parsed, 0, sizeof(http_parsed_request_t));

	// Parser la première ligne: METHOD /path HTTP/1.1
	char *line_end = strstr(raw_request, "\r\n");
	if (!line_end)
		return ESP01_FAIL;

	// Copier la première ligne pour la parser
	size_t first_line_len = line_end - raw_request;
	if (first_line_len >= sizeof(parsed->method) + sizeof(parsed->path) + 20)
		return ESP01_FAIL;

	char first_line[256];
	strncpy(first_line, raw_request, first_line_len);
	first_line[first_line_len] = '\0';

	// Parser METHOD et PATH
	char *space1 = strchr(first_line, ' ');
	if (!space1)
		return ESP01_FAIL;

	// Extraire la méthode
	size_t method_len = space1 - first_line;
	if (method_len >= sizeof(parsed->method))
		return ESP01_FAIL;

	strncpy(parsed->method, first_line, method_len);
	parsed->method[method_len] = '\0';

	// Extraire le path
	char *space2 = strchr(space1 + 1, ' ');
	if (!space2)
		return ESP01_FAIL;

	size_t path_len = space2 - space1 - 1;
	if (path_len >= sizeof(parsed->path))
		return ESP01_FAIL;

	strncpy(parsed->path, space1 + 1, path_len);
	parsed->path[path_len] = '\0';

	// Séparer le path des paramètres de requête
	char *query_start = strchr(parsed->path, '?');
	if (query_start)
	{
		*query_start = '\0'; // Terminer le path
		query_start++;		 // Pointer vers les paramètres

		// Copier les paramètres de requête
		strncpy(parsed->query_string, query_start, sizeof(parsed->query_string) - 1);
		parsed->query_string[sizeof(parsed->query_string) - 1] = '\0';
	}

	parsed->is_valid = true;
	return ESP01_OK;
}

ESP01_Status_t esp01_get_query_param(const char *query_string, const char *param_name,
									 char *value_buffer, size_t buffer_size)
{
	if (!query_string || !param_name || !value_buffer || buffer_size == 0)
		return ESP01_FAIL;

	value_buffer[0] = '\0';

	// Chercher le paramètre dans la chaîne de requête
	char search_pattern[64];
	snprintf(search_pattern, sizeof(search_pattern), "%s=", param_name);

	char *param_start = strstr(query_string, search_pattern);
	if (!param_start)
		return ESP01_FAIL;

	param_start += strlen(search_pattern);

	// Chercher la fin du paramètre (& ou fin de chaîne)
	char *param_end = strchr(param_start, '&');
	size_t param_len;

	if (param_end)
		param_len = param_end - param_start;
	else
		param_len = strlen(param_start);

	if (param_len >= buffer_size)
		param_len = buffer_size - 1;

	strncpy(value_buffer, param_start, param_len);
	value_buffer[param_len] = '\0';

	return ESP01_OK;
}

// ==================== Fonctions de réponse HTTP ====================

ESP01_Status_t esp01_send_http_response(int conn_id, int status_code, const char *content_type,
										const char *body, size_t body_len)
{
	if (!content_type)
		content_type = "text/html";

	if (!body)
	{
		body = "";
		body_len = 0;
	}
	else if (body_len == 0)
	{
		body_len = strlen(body);
	}

	// Construire l'en-tête HTTP
	char header[512];
	const char *status_text = "OK";

	switch (status_code)
	{
	case 200:
		status_text = "OK";
		break;
	case 404:
		status_text = "Not Found";
		break;
	case 500:
		status_text = "Internal Server Error";
		break;
	default:
		status_text = "Unknown";
		break;
	}

	int header_len = snprintf(header, sizeof(header),
							  "HTTP/1.1 %d %s\r\n"
							  "Content-Type: %s\r\n"
							  "Content-Length: %d\r\n"
							  "Connection: close\r\n"
							  "\r\n",
							  status_code, status_text, content_type, (int)body_len);

	if (header_len >= sizeof(header))
		return ESP01_BUFFER_TOO_SMALL;

	// Calculer la taille totale
	size_t total_len = header_len + body_len;

	// Allouer un buffer pour la réponse complète
	char *response = malloc(total_len + 1);
	if (!response)
		return ESP01_BUFFER_TOO_SMALL;

	// Construire la réponse complète
	memcpy(response, header, header_len);
	if (body_len > 0)
		memcpy(response + header_len, body, body_len);
	response[total_len] = '\0';

	// Envoyer la réponse
	handle_http_request(conn_id, response, total_len);

	free(response);
	return ESP01_OK;
}

ESP01_Status_t esp01_send_json_response(int conn_id, const char *json_data)
{
	return esp01_send_http_response(conn_id, 200, "application/json", json_data, 0);
}

ESP01_Status_t esp01_send_404_response(int conn_id)
{
	const char *body = "<html><body><h1>404 - Page Not Found</h1></body></html>";
	return esp01_send_http_response(conn_id, 404, "text/html", body, 0);
}

// ==================== Fonctions de gestion d'état ====================

ESP01_Status_t esp01_get_connection_status(void)
{
	char *response = NULL;
	ESP01_Status_t status = esp01_send_raw_command_dma("AT+CIPSTATUS", &response, 512, "OK", 3000);

	if (status == ESP01_OK && response)
	{
		if (strstr(response, "STATUS:2") || strstr(response, "STATUS:3"))
		{
			free(response);
			return ESP01_OK; // Connecté
		}
	}

	if (response)
		free(response);
	return ESP01_FAIL; // Non connecté
}

ESP01_Status_t esp01_stop_web_server(void)
{
	char *response = NULL;
	ESP01_Status_t status = esp01_send_raw_command_dma("AT+CIPSERVER=0", &response, 256, "OK", 3000);
	if (response)
		free(response);
	return status;
}

// ==================== Fonctions de diagnostic ====================

void esp01_print_status(UART_HandleTypeDef *huart_output)
{
	if (!huart_output)
		return;

	const char *header = "\r\n=== STATUS ESP01 ===\r\n";
	HAL_UART_Transmit(huart_output, (uint8_t *)header, strlen(header), HAL_MAX_DELAY);

	// Test de base
	char *response = NULL;
	ESP01_Status_t status = esp01_send_raw_command_dma("AT", &response, 256, "OK", 2000);

	char msg[128];
	snprintf(msg, sizeof(msg), "Test AT: %s\r\n", (status == ESP01_OK) ? "OK" : "FAIL");
	HAL_UART_Transmit(huart_output, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);

	if (response)
	{
		free(response);
		response = NULL;
	}

	// Status WiFi
	status = esp01_send_raw_command_dma("AT+CWJAP?", &response, 512, "OK", 3000);
	if (status == ESP01_OK && response)
	{
		if (strstr(response, "No AP"))
		{
			HAL_UART_Transmit(huart_output, (uint8_t *)"WiFi: Non connecté\r\n", 20, HAL_MAX_DELAY);
		}
		else
		{
			HAL_UART_Transmit(huart_output, (uint8_t *)"WiFi: Connecté\r\n", 16, HAL_MAX_DELAY);
		}
	}
	else
	{
		HAL_UART_Transmit(huart_output, (uint8_t *)"WiFi: Status inconnu\r\n", 22, HAL_MAX_DELAY);
	}

	if (response)
	{
		free(response);
		response = NULL;
	}

	// IP Address
	char ip[32];
	if (esp01_get_current_ip(ip, sizeof(ip)) == ESP01_OK)
	{
		snprintf(msg, sizeof(msg), "IP: %s\r\n", ip);
		HAL_UART_Transmit(huart_output, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
	}

	// Nombre de routes
	snprintf(msg, sizeof(msg), "Routes définies: %d/%d\r\n", g_route_count, ESP01_MAX_ROUTES);
	HAL_UART_Transmit(huart_output, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);

	const char *footer = "==================\r\n";
	HAL_UART_Transmit(huart_output, (uint8_t *)footer, strlen(footer), HAL_MAX_DELAY);
}

// ==================== Fonction de boucle principale ====================

void esp01_process_requests(void)
{
	// 1. Vérifier si de nouvelles données sont arrivées via DMA
	uint8_t buffer[ESP01_DMA_RX_BUF_SIZE];
	uint16_t len = esp01_get_new_data(buffer, sizeof(buffer));
	if (len == 0)
	{
		// Rien à traiter, retour immédiat
		return;
	}

	// 2. Chercher une requête HTTP dans les données reçues
	http_request_t req = parse_ipd_header((const char *)buffer);
	if (req.is_http_request && req.is_valid)
	{
		// 3. Parser la requête HTTP (extraction du chemin)
		http_parsed_request_t parsed = {0};
		if (esp01_parse_http_request((const char *)buffer, &parsed) == ESP01_OK && parsed.is_valid)
		{
			// 4. Trouver le handler associé à la route
			esp01_route_handler_t handler = esp01_find_route_handler(parsed.path);
			if (handler)
			{
				handler(req.conn_id, &parsed);
			}
			else
			{
				// Route non trouvée : envoyer 404
				esp01_send_404_response(req.conn_id);
			}
		}
		// 5. Vider le payload HTTP restant si besoin
		if (req.content_length > 0)
		{
			discard_http_payload(req.content_length);
		}
	}
}
