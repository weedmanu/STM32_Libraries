/*
 * STM32_WifiESP.c
 * Driver STM32 <-> ESP01 (AT) Serveur web
 * Version structurée avec routage HTTP
 * 2025 - manu
 */

// ==================== INCLUDES ====================

#include "STM32_WifiESP.h"

// ==================== DÉFINITIONS & MACROS ====================

#ifndef VALIDATE_PARAM
#define VALIDATE_PARAM(expr, ret) \
	do                            \
	{                             \
		if (!(expr))              \
			return (ret);         \
	} while (0)
#endif

#define ESP01_CONN_TIMEOUT_MS 30000 // 30 secondes d'inactivité

// ==================== VARIABLES GLOBALES ====================

esp01_stats_t g_stats = {0};
connection_info_t g_connections[ESP01_MAX_CONNECTIONS] = {0};
int g_connection_count = 0;
uint16_t g_server_port = 80;

// ==================== VARIABLES INTERNES ====================
static UART_HandleTypeDef *g_terminal_uart = NULL;
// Tableau des routes HTTP enregistrées
static esp01_route_t g_routes[ESP01_MAX_ROUTES];
// Nombre de routes actuellement enregistrées
static int g_route_count = 0;

// UART utilisé pour l'ESP01
static UART_HandleTypeDef *g_esp_uart = NULL;
// UART pour debug (logs)
static UART_HandleTypeDef *g_debug_uart = NULL;
// Buffer DMA pour la réception UART
static uint8_t *g_dma_rx_buf = NULL;
// Taille du buffer DMA
static uint16_t g_dma_buf_size = 0;
// Dernière position lue dans le buffer DMA
static volatile uint16_t g_rx_last_pos = 0;

// Accumulateur global pour les données reçues (permet de gérer les trames fragmentées)
static char g_accumulator[ESP01_DMA_RX_BUF_SIZE * 2];
static uint16_t g_acc_len = 0;
static volatile bool g_processing_request = false;

// État de l'analyse des trames
typedef enum
{
	PARSE_STATE_SEARCHING_IPD,
	PARSE_STATE_READING_HEADER,
	PARSE_STATE_READING_PAYLOAD
} parse_state_t;

static parse_state_t g_parse_state = PARSE_STATE_SEARCHING_IPD;

/***************************************************************/
// Fonctions internes utilisées par le driver ESP01
/***************************************************************/

// Logge un message sur l'UART debug si activé
static void _esp_logln(const char *msg)
{
	if (ESP01_DEBUG && g_debug_uart && msg) // Si debug activé et UART debug dispo
	{
		HAL_UART_Transmit(g_debug_uart, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY); // Envoie le message
		HAL_UART_Transmit(g_debug_uart, (uint8_t *)"\r\n", 2, HAL_MAX_DELAY);		 // Ajoute retour à la ligne
	}
}

// Calcule la position actuelle d'écriture du DMA
static uint16_t _get_dma_write_pos(void)
{
	return g_dma_buf_size - __HAL_DMA_GET_COUNTER(g_esp_uart->hdmarx); // Position d'écriture dans le buffer circulaire
}

// Calcule le nombre de nouveaux octets disponibles dans le buffer DMA
static uint16_t _get_available_bytes(void)
{
	uint16_t write_pos = _get_dma_write_pos(); // Position actuelle d'écriture
	if (write_pos >= g_rx_last_pos)
		return write_pos - g_rx_last_pos; // Cas normal
	else
		return (g_dma_buf_size - g_rx_last_pos) + write_pos; // Cas de débordement circulaire
}

// Récupère les nouveaux octets reçus via DMA UART
static uint16_t esp01_get_new_data(uint8_t *buffer, uint16_t buffer_size)
{
	if (!g_esp_uart || !g_dma_rx_buf || !buffer || buffer_size == 0)
	{
		if (buffer && buffer_size > 0)
			buffer[0] = '\0';
		return 0;
	}

	uint16_t available = _get_available_bytes(); // Octets disponibles
	if (available == 0)
	{
		buffer[0] = '\0';
		return 0;
	}

	uint16_t to_copy = (available < buffer_size - 1) ? available : buffer_size - 1;
	uint16_t copied = 0;
	uint16_t write_pos = _get_dma_write_pos();

	// Cas où les données ne sont pas fragmentées dans le buffer circulaire
	if (write_pos > g_rx_last_pos)
	{
		memcpy(buffer, &g_dma_rx_buf[g_rx_last_pos], to_copy);
		copied = to_copy;
	}
	else
	{
		// Cas où les données sont fragmentées (fin du buffer)
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

	buffer[copied] = '\0';									   // Ajoute fin de chaîne
	g_rx_last_pos = (g_rx_last_pos + copied) % g_dma_buf_size; // Met à jour la position

	if (copied > 0)
	{
		char dbg[64];
		snprintf(dbg, sizeof(dbg), "[GET NEW DATA]  %u octets reçus", copied);
		_esp_logln(dbg);
	}

	return copied;
}

// Accumule les données reçues et cherche un motif (pattern) dans l'accumulateur
static bool _accumulate_and_search(const char *pattern, uint32_t timeout_ms, bool clear_first)
{
	uint32_t start_tick = HAL_GetTick(); // Début du timeout
	char temp_buf[256];

	if (clear_first)
	{
		g_acc_len = 0; // Vide l'accumulateur
		g_accumulator[0] = '\0';
	}

	while ((HAL_GetTick() - start_tick) < timeout_ms) // Boucle jusqu'au timeout
	{
		uint16_t new_data = esp01_get_new_data((uint8_t *)temp_buf, sizeof(temp_buf)); // Récupère les nouveaux octets
		if (new_data > 0)
		{
			uint16_t space_left = sizeof(g_accumulator) - g_acc_len - 1; // Espace restant
			uint16_t to_add = (new_data < space_left) ? new_data : space_left;
			if (to_add > 0)
			{
				memcpy(&g_accumulator[g_acc_len], temp_buf, to_add); // Ajoute au buffer global
				g_acc_len += to_add;
				g_accumulator[g_acc_len] = '\0';
			}
			if (strstr(g_accumulator, pattern)) // Cherche le motif
				return true;
		}
		else
		{
			HAL_Delay(10); // Petite pause si rien de nouveau
		}
	}
	return false; // Timeout sans trouver le motif
}

// Vide le buffer RX UART/DMA pendant un certain temps (utilisé pour nettoyer avant une commande AT)
static void _flush_rx_buffer(uint32_t timeout_ms)
{
	char temp_buf[ESP01_DMA_RX_BUF_SIZE];
	uint32_t start_time = HAL_GetTick();

	while ((HAL_GetTick() - start_time) < timeout_ms)
	{
		uint16_t len = esp01_get_new_data((uint8_t *)temp_buf, sizeof(temp_buf)); // Vide les nouveaux octets
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

// Cherche le prochain +IPD valide dans un buffer (pour resynchroniser en cas de trame corrompue)
static char *_find_next_ipd(char *buffer, int buffer_len)
{
	char *search_pos = buffer;
	char *ipd_pos;

	while ((ipd_pos = strstr(search_pos, "+IPD,")) != NULL) // Recherche "+IPD,"
	{
		// Vérifie la présence du ':' après +IPD
		char *colon = strchr(ipd_pos, ':');
		if (colon && (colon - buffer) < buffer_len - 1)
		{
			return ipd_pos; // Trouvé
		}
		// Continue la recherche après ce +IPD invalide
		search_pos = ipd_pos + IPD_HEADER_MIN_LEN;
	}
	return NULL; // Aucun trouvé
}

// Parse un header IPD brut pour extraire les infos de connexion
static http_request_t parse_ipd_header(const char *data)
{
	http_request_t request = {0};

	char *ipd_start = strstr(data, "+IPD,");
	if (!ipd_start)
	{
		_esp_logln("parse_ipd_header: +IPD non trouvé");
		return request;
	}

	int parsed = sscanf(ipd_start, "+IPD,%d,%d,\"%15[0-9.]\",%d:", &request.conn_id, &request.content_length, request.client_ip, &request.client_port);
	if (parsed == 4)
	{
		request.is_valid = true;
		request.has_ip = true;
		char dbg[80];
		snprintf(dbg, sizeof(dbg), "[PARSE] parse_ipd_header: IPD avec IP %s:%d, conn_id=%d, len=%d", request.client_ip, request.client_port, request.conn_id, request.content_length);
		_esp_logln(dbg);
	}
	else
	{
		parsed = sscanf(ipd_start, "+IPD,%d,%d:", &request.conn_id, &request.content_length);
		if (parsed == 2)
		{
			request.is_valid = true;
			request.has_ip = false;
			char dbg[80];
			snprintf(dbg, sizeof(dbg), "[PARSE] parse_ipd_header: IPD sans IP, conn_id=%d, len=%d", request.conn_id, request.content_length);
			_esp_logln(dbg);
		}
		else
		{
			_esp_logln("[PARSE] parse_ipd_header: format IPD non reconnu");
		}
	}
	return request;
}

// Ignore/consomme un payload HTTP restant (ex: body POST non traité)
static void discard_http_payload(int expected_length)
{
	char discard_buf[256];
	int remaining = expected_length;
	uint32_t timeout_start = HAL_GetTick();
	const uint32_t timeout_ms = ESP01_TIMEOUT_MEDIUM;

	_esp_logln("[HTTP] discard_http_payload: début vidage payload HTTP");

	while (remaining > 0 && (HAL_GetTick() - timeout_start) < timeout_ms)
	{
		int to_read = (remaining < sizeof(discard_buf)) ? remaining : sizeof(discard_buf);
		uint16_t read_len = esp01_get_new_data((uint8_t *)discard_buf, to_read);

		if (read_len > 0)
		{
			remaining -= read_len;
			char debug_msg[50];
			snprintf(debug_msg, sizeof(debug_msg), "[HTTP] Ignoré %d octets, reste %d", read_len, remaining);
			_esp_logln(debug_msg);
		}
		else
		{
			HAL_Delay(10);
		}
	}

	if (remaining > 0)
	{
		char warn_msg[100];
		snprintf(warn_msg, sizeof(warn_msg), "[HTTP] AVERTISSEMENT: %d octets non lus", remaining);
		_esp_logln(warn_msg);
	}
	else
	{
		_esp_logln("[HTTP] Payload HTTP complètement vidé");
	}
}

static ESP01_Status_t esp01_set_wifi_mode(ESP01_WifiMode_t mode)
{
	char cmd[32], resp[64];
	snprintf(cmd, sizeof(cmd), "AT+CWMODE=%d", mode);
	ESP01_Status_t status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_SHORT);
	return status;
}

static ESP01_Status_t esp01_connect_wifi(const char *ssid, const char *password)
{
	char cmd[128], resp[128];
	snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
	ESP01_Status_t status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_WIFI);
	return status;
}

// Attend un motif précis dans le flux RX (avec timeout)
static ESP01_Status_t esp01_wait_for_pattern(const char *pattern, uint32_t timeout_ms)
{
	if (_accumulate_and_search(pattern, timeout_ms, false))
		return ESP01_OK;
	return ESP01_TIMEOUT;
}

static void esp01_cleanup_inactive_connections(void)
{
	uint32_t now = HAL_GetTick();
	for (int i = 0; i < g_connection_count; ++i)
	{
		if (g_connections[i].is_active && (now - g_connections[i].last_activity > ESP01_CONN_TIMEOUT_MS))
		{
			// Ferme la connexion côté ESP01
			char cmd[32];
			snprintf(cmd, sizeof(cmd), "AT+CIPCLOSE=%d", g_connections[i].conn_id);
			char resp[64];
			esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 2000);

			g_connections[i].is_active = false;
			g_connections[i].conn_id = -1;
			g_connections[i].client_ip[0] = '\0';
			g_connections[i].server_port = 0;
			g_connections[i].client_port = 0;
			_esp_logln("[CONN] Connexion fermée pour inactivité");
		}
	}
}

/***************************************************************/
// Fonctions utilisateur pour le driver ESP01
/***************************************************************/

// ==================== DRIVER ====================
// Vide le buffer RX UART/DMA pendant un certain temps (externe)
ESP01_Status_t esp01_flush_rx_buffer(uint32_t timeout_ms)
{
	_flush_rx_buffer(timeout_ms);
	return ESP01_OK;
}

// Initialise le driver ESP01 (UART, debug UART, buffer DMA, taille buffer)
ESP01_Status_t esp01_init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug,
						  uint8_t *dma_rx_buf, uint16_t dma_buf_size)
{
	_esp_logln("[ESP01] Initialisation UART/DMA pour ESP01");
	g_esp_uart = huart_esp;		   // UART principal
	g_debug_uart = huart_debug;	   // UART debug
	g_terminal_uart = huart_debug; // UART terminal = UART debug (pour le mode terminal AT)
	g_dma_rx_buf = dma_rx_buf;	   // Buffer DMA
	g_dma_buf_size = dma_buf_size; // Taille buffer
	g_rx_last_pos = 0;			   // Reset position
	g_acc_len = 0;				   // Reset accumulateur
	g_accumulator[0] = '\0';
	g_parse_state = PARSE_STATE_SEARCHING_IPD; // <-- Ajout ici

	if (!g_esp_uart || !g_dma_rx_buf || g_dma_buf_size == 0)
	{
		_esp_logln("[ESP01] ESP01 Init Error: Invalid params");
		return ESP01_NOT_INITIALIZED;
	}

	// Lance la réception DMA
	if (HAL_UART_Receive_DMA(g_esp_uart, g_dma_rx_buf, g_dma_buf_size) != HAL_OK)
	{
		_esp_logln("[ESP01] ESP01 Init Error: HAL_UART_Receive_DMA failed");
		return ESP01_FAIL;
	}

	_esp_logln("[ESP01] --- ESP01 Driver Init OK ---");
	HAL_Delay(100);
	return ESP01_OK;
}

// ==================== COMMANDES AT GÉNÉRIQUES ====================// ==================== Commandes AT génériques ====================

// Envoie une commande AT et récupère la réponse (DMA, timeout, etc.)
ESP01_Status_t esp01_send_raw_command_dma(const char *cmd, char *response_buffer,
										  uint32_t max_response_size,
										  const char *expected_terminator,
										  uint32_t timeout_ms)
{
	if (!g_esp_uart || !cmd || !response_buffer)
		return ESP01_NOT_INITIALIZED;

	const char *terminator = expected_terminator ? expected_terminator : "OK";

	char full_cmd[ESP01_DMA_RX_BUF_SIZE];
	int cmd_len = snprintf(full_cmd, sizeof(full_cmd), "%s\r\n", cmd);

	_flush_rx_buffer(100);

	if (HAL_UART_Transmit(g_esp_uart, (uint8_t *)full_cmd, cmd_len, ESP01_TIMEOUT_SHORT) != HAL_OK)
		return ESP01_FAIL;

	bool found = _accumulate_and_search(terminator, timeout_ms, true);

	uint32_t copy_len = (g_acc_len < max_response_size - 1) ? g_acc_len : max_response_size - 1;
	memcpy(response_buffer, g_accumulator, copy_len);
	response_buffer[copy_len] = '\0';

	return found ? ESP01_OK : ESP01_TIMEOUT;
}

// ==================== FONCTIONS WIFI & SERVEUR ====================

ESP01_Status_t esp01_connect_wifi_config(
	ESP01_WifiMode_t mode,
	const char *ssid,
	const char *password,
	bool use_dhcp,
	const char *ip,
	const char *gateway,
	const char *netmask)
{
	ESP01_Status_t status;
	char cmd[ESP01_DMA_RX_BUF_SIZE];
	char resp[ESP01_DMA_RX_BUF_SIZE];

	_esp_logln("[WIFI] === Début configuration WiFi ===");

	// 1. Définir le mode WiFi (STA ou AP uniquement)
	_esp_logln("[WIFI] -> Définition du mode WiFi...");
	status = esp01_set_wifi_mode(mode);
	if (status != ESP01_OK)
	{
		_esp_logln("[WIFI] !! ERREUR: esp01_set_wifi_mode");
		return status;
	}
	HAL_Delay(300);

	// 2. Si AP, configure le point d'accès
	if (mode == ESP01_WIFI_MODE_AP)
	{
		_esp_logln("[WIFI] -> Configuration du point d'accès (AP)...");
		snprintf(cmd, sizeof(cmd), "AT+CWSAP=\"%s\",\"%s\",5,3", ssid, password);
		status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 2000);
		_esp_logln(resp);
		if (status != ESP01_OK)
		{
			_esp_logln("[WIFI] !! ERREUR: Configuration AP");
			return status;
		}
		HAL_Delay(300);
	}

	// 3. Configurer DHCP ou IP statique
	if (use_dhcp)
	{
		if (mode == ESP01_WIFI_MODE_STA)
		{
			_esp_logln("[WIFI] -> Activation du DHCP STA...");
			status = esp01_send_raw_command_dma("AT+CWDHCP=1,1", resp, sizeof(resp), "OK", 2000);
		}
		else if (mode == ESP01_WIFI_MODE_AP)
		{
			_esp_logln("[WIFI] -> Activation du DHCP AP...");
			status = esp01_send_raw_command_dma("AT+CWDHCP=2,1", resp, sizeof(resp), "OK", 2000);
		}
		_esp_logln(resp);
		if (status != ESP01_OK)
		{
			_esp_logln("[WIFI] !! ERREUR: Activation DHCP");
			return status;
		}
	}
	else if (ip && gateway && netmask && mode == ESP01_WIFI_MODE_STA)
	{
		_esp_logln("[WIFI] -> Déconnexion du WiFi (CWQAP)...");
		esp01_send_raw_command_dma("AT+CWQAP", resp, sizeof(resp), "OK", 2000);

		_esp_logln("[WIFI] -> Désactivation du DHCP client...");
		status = esp01_send_raw_command_dma("AT+CWDHCP=0,1", resp, sizeof(resp), "OK", 2000);
		_esp_logln(resp);
		if (status != ESP01_OK)
		{
			_esp_logln("[WIFI] !! ERREUR: Désactivation DHCP");
			return status;
		}
		_esp_logln("[WIFI] -> Configuration IP statique...");
		snprintf(cmd, sizeof(cmd), "AT+CIPSTA=\"%s\",\"%s\",\"%s\"", ip, gateway, netmask);
		status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 2000);
		_esp_logln(resp);
		if (status != ESP01_OK)
		{
			_esp_logln("[WIFI] !! ERREUR: Configuration IP statique");
			return status;
		}
	}

	// 4. Connexion au réseau WiFi (STA uniquement)
	if (mode == ESP01_WIFI_MODE_STA)
	{
		_esp_logln("[WIFI] -> Connexion au réseau WiFi...");
		status = esp01_connect_wifi(ssid, password);
		if (status != ESP01_OK)
		{
			_esp_logln("[WIFI] !! ERREUR: Connexion WiFi (CWJAP)");
			return status;
		}
		HAL_Delay(300);
	}

	// 5. Activation de l'affichage IP client dans +IPD
	_esp_logln("[WIFI] -> Activation de l'affichage IP client dans +IPD (AT+CIPDINFO=1)...");
	status = esp01_send_raw_command_dma("AT+CIPDINFO=1", resp, sizeof(resp), "OK", 2000);
	_esp_logln(resp);
	if (status != ESP01_OK)
	{
		_esp_logln("[WIFI] !! ERREUR: AT+CIPDINFO=1");
		return status;
	}

	_esp_logln("[WIFI] === Configuration WiFi terminée ===");
	return ESP01_OK;
}

ESP01_Status_t esp01_start_server_config(
	bool multi_conn, // true = multi-connexion (CIPMUX=1), false = mono (CIPMUX=0)
	uint16_t port)	 // Port du serveur web
{
	g_server_port = port;
	ESP01_Status_t status;
	char resp[ESP01_DMA_RX_BUF_SIZE];
	char cmd[ESP01_DMA_RX_BUF_SIZE];

	// 1. Configure le mode multi-connexion
	snprintf(cmd, sizeof(cmd), "AT+CIPMUX=%d", multi_conn ? 1 : 0);
	status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 3000);
	if (status != ESP01_OK)
	{
		_esp_logln("[WEB] ERREUR: AT+CIPMUX");
		return status;
	}

	// 2. Démarre le serveur web sur le port demandé
	snprintf(cmd, sizeof(cmd), "AT+CIPSERVER=1,%u", port);
	status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 5000);
	if (status != ESP01_OK && !strstr(resp, "no change"))
	{
		_esp_logln("[WEB] ERREUR: AT+CIPSERVER");
		return status;
	}

	_esp_logln("[WEB] Serveur web démarré avec succès");
	return ESP01_OK;
}

// Parse une requête HTTP (GET/POST) et remplit la structure correspondante
ESP01_Status_t esp01_parse_http_request(const char *raw_request, http_parsed_request_t *parsed)
{
	VALIDATE_PARAM(raw_request, ESP01_FAIL);
	VALIDATE_PARAM(parsed, ESP01_FAIL);

	memset(parsed, 0, sizeof(http_parsed_request_t));

	// Machine à états pour parser la première ligne HTTP
	const char *p = raw_request;
	const char *method_start = p, *method_end = NULL;
	const char *path_start = NULL, *path_end = NULL;
	const char *query_start = NULL, *query_end = NULL;
	const char *line_end = strstr(p, "\r\n");
	if (!line_end)
		return ESP01_FAIL;

	// État 1 : méthode
	while (p < line_end && *p != ' ')
		p++;
	method_end = p;
	if (method_end - method_start >= ESP01_MAX_HTTP_METHOD_LEN)
		return ESP01_FAIL;
	memcpy(parsed->method, method_start, method_end - method_start);
	parsed->method[method_end - method_start] = '\0';

	// État 2 : chemin
	p++; // skip space
	path_start = p;
	while (p < line_end && *p != ' ' && *p != '?')
		p++;
	path_end = p;
	if (path_end - path_start >= ESP01_MAX_HTTP_PATH_LEN)
		return ESP01_FAIL;
	memcpy(parsed->path, path_start, path_end - path_start);
	parsed->path[path_end - path_start] = '\0';

	// État 3 : query string (optionnel)
	if (*p == '?')
	{
		p++;
		query_start = p;
		while (p < line_end && *p != ' ')
			p++;
		query_end = p;
		size_t qlen = query_end - query_start;
		if (qlen >= ESP01_MAX_HTTP_QUERY_LEN)
			qlen = ESP01_MAX_HTTP_QUERY_LEN - 1;
		memcpy(parsed->query_string, query_start, qlen);
		parsed->query_string[qlen] = '\0';
	}
	else
	{
		parsed->query_string[0] = '\0';
	}

	parsed->is_valid = true;
	return ESP01_OK;
}

// Envoie une réponse HTTP générique (code, type, body)
ESP01_Status_t esp01_send_http_response(int conn_id, int status_code, const char *content_type,
										const char *body, size_t body_len)
{
	VALIDATE_PARAM(conn_id >= 0, ESP01_FAIL);
	VALIDATE_PARAM(status_code >= 100 && status_code < 600, ESP01_FAIL);
	VALIDATE_PARAM(body || body_len == 0, ESP01_FAIL);

	uint32_t start = HAL_GetTick();
	g_stats.total_requests++;
	g_stats.response_count++;
	if (status_code >= 200 && status_code < 300)
	{
		g_stats.successful_responses++;
	}
	else if (status_code >= 400)
	{
		g_stats.failed_responses++;
	}

	char header[256];
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
							  status_code, status_text, content_type ? content_type : "text/html", (int)body_len);

	// Utilise un buffer fixe de 2048 octets pour la réponse HTTP
	char response[2048];
	if ((header_len + body_len) >= sizeof(response))
	{
		_esp_logln("[HTTP] esp01_send_http_response: réponse trop grande");
		return ESP01_FAIL;
	}

	memcpy(response, header, header_len);
	if (body && body_len > 0)
		memcpy(response + header_len, body, body_len);

	int total_len = header_len + (int)body_len;

	char cipsend_cmd[32];
	snprintf(cipsend_cmd, sizeof(cipsend_cmd), "AT+CIPSEND=%d,%d", conn_id, total_len);

	char resp[64];
	ESP01_Status_t st = esp01_send_raw_command_dma(cipsend_cmd, resp, sizeof(resp), ">", ESP01_TIMEOUT_LONG);
	if (st != ESP01_OK)
	{
		_esp_logln("[HTTP] esp01_send_http_response: AT+CIPSEND échoué");
		return st;
	}

	HAL_UART_Transmit(g_esp_uart, (uint8_t *)response, total_len, HAL_MAX_DELAY);

	st = esp01_wait_for_pattern("SEND OK", ESP01_TIMEOUT_LONG);
	char dbg[128];
	snprintf(dbg, sizeof(dbg), "[HTTP] Envoi réponse HTTP sur connexion %d, taille de la page HTML : %d octets", conn_id, (int)body_len);
	_esp_logln(dbg);

	// À la toute fin, après l'envoi :
	uint32_t elapsed = HAL_GetTick() - start;
	g_stats.total_response_time_ms += elapsed;
	g_stats.avg_response_time_ms = g_stats.response_count ? (g_stats.total_response_time_ms / g_stats.response_count) : 0;

	return st;
}

// Envoie une réponse HTTP JSON (200 OK)
ESP01_Status_t esp01_send_json_response(int conn_id, const char *json_data)
{
	_esp_logln("[HTTP] Envoi d'une réponse JSON");
	return esp01_send_http_response(conn_id, 200, "application/json", json_data, strlen(json_data));
}

// Envoie une réponse HTTP 404 Not Found
ESP01_Status_t esp01_send_404_response(int conn_id)
{
	_esp_logln("[HTTP] Envoi d'une réponse 404");
	const char *body = "<html><body><h1>404 - Page Not Found</h1></body></html>";
	return esp01_send_http_response(conn_id, 404, "text/html", body, strlen(body));
}

// Vérifie si l'ESP01 est connecté au WiFi
ESP01_Status_t esp01_get_connection_status(void)
{
	_esp_logln("[STATUS] Vérification du statut de connexion ESP01");
	char response[512];
	ESP01_Status_t status = esp01_send_raw_command_dma("AT+CIPSTATUS", response, sizeof(response), "OK", ESP01_TIMEOUT_MEDIUM);

	if (status == ESP01_OK)
	{
		if (strstr(response, "STATUS:2") || strstr(response, "STATUS:3"))
		{
			_esp_logln("[STATUS] ESP01 connecté au WiFi");
			return ESP01_OK;
		}
	}

	_esp_logln("[STATUS] ESP01 non connecté au WiFi");
	return ESP01_FAIL;
}

// Arrête le serveur web (ferme le port TCP)
ESP01_Status_t esp01_stop_web_server(void)
{
	_esp_logln("[STATUS] Arrêt du serveur web ESP01");
	char response[256];
	ESP01_Status_t status = esp01_send_raw_command_dma("AT+CIPSERVER=0", response, sizeof(response), "OK", ESP01_TIMEOUT_MEDIUM);
	return status;
}

// Affiche le statut de l'ESP01 sur un terminal UART
ESP01_Status_t esp01_print_connection_status(void)
{
	if (!g_debug_uart)
	{
		_esp_logln("[STATUS] esp01_print_connection_status: g_debug_uart NULL");
		return ESP01_NOT_INITIALIZED;
	}

	const char *header = "\r\n=== STATUS ESP01 ===\r\n";
	HAL_UART_Transmit(g_debug_uart, (uint8_t *)header, strlen(header), HAL_MAX_DELAY);

	char response[256];
	ESP01_Status_t status = esp01_send_raw_command_dma("AT", response, sizeof(response), "OK", 2000);

	char msg[128];
	snprintf(msg, sizeof(msg), "Test AT: %s\r\n", (status == ESP01_OK) ? "OK" : "FAIL");
	HAL_UART_Transmit(g_debug_uart, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);

	status = esp01_send_raw_command_dma("AT+CWJAP?", response, sizeof(response), "OK", 3000);
	if (status == ESP01_OK)
	{
		if (strstr(response, "No AP"))
		{
			HAL_UART_Transmit(g_debug_uart, (uint8_t *)"WiFi: Non connecté\r\n", 20, HAL_MAX_DELAY);
		}
		else
		{
			HAL_UART_Transmit(g_debug_uart, (uint8_t *)"WiFi: Connecté\r\n", 16, HAL_MAX_DELAY);
		}
	}
	else
	{
		HAL_UART_Transmit(g_debug_uart, (uint8_t *)"WiFi: Status inconnu\r\n", 22, HAL_MAX_DELAY);
	}

	char ip[32];
	if (esp01_get_current_ip(ip, sizeof(ip)) == ESP01_OK)
	{
		snprintf(msg, sizeof(msg), "IP: %s\r\n", ip);
		HAL_UART_Transmit(g_debug_uart, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
	}

	snprintf(msg, sizeof(msg), "Routes définies: %d/%d\r\n", g_route_count, ESP01_MAX_ROUTES);
	HAL_UART_Transmit(g_debug_uart, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);

	const char *footer = "==================\r\n";
	HAL_UART_Transmit(g_debug_uart, (uint8_t *)footer, strlen(footer), HAL_MAX_DELAY);
	_esp_logln("[STATUS] Statut ESP01 affiché sur terminal");
	return ESP01_OK;
}

// Traite les requêtes HTTP reçues (dispatcher)
void esp01_process_requests(void)
{
	if (g_processing_request)
		return; // Protection basique
	g_processing_request = true;

	uint8_t buffer[ESP01_DMA_RX_BUF_SIZE];
	uint16_t len = esp01_get_new_data(buffer, sizeof(buffer));
	if (len == 0)
	{
		g_processing_request = false;
		return;
	}

	// Ajoute les nouveaux octets à l'accumulateur global
	if (g_acc_len + len < sizeof(g_accumulator) - 1)
	{
		memcpy(g_accumulator + g_acc_len, buffer, len);
		g_acc_len += len;
		g_accumulator[g_acc_len] = '\0';
	}
	else
	{
		// Débordement : reset complet
		g_acc_len = 0;
		g_accumulator[0] = '\0';
		g_parse_state = PARSE_STATE_SEARCHING_IPD;
		g_processing_request = false;
		return;
	}

	bool keep_processing = true;
	while (keep_processing)
	{
		switch (g_parse_state)
		{
		case PARSE_STATE_SEARCHING_IPD:
		{
			char *ipd_start = _find_next_ipd(g_accumulator, g_acc_len);
			if (!ipd_start)
			{
				keep_processing = false;
				break;
			}
			if (ipd_start != g_accumulator)
			{
				int shift = ipd_start - g_accumulator;
				memmove(g_accumulator, ipd_start, g_acc_len - shift);
				g_acc_len -= shift;
				g_accumulator[g_acc_len] = '\0';
			}
			g_parse_state = PARSE_STATE_READING_HEADER;
			// Pas de break, on enchaîne
		}
		case PARSE_STATE_READING_HEADER:
		{
			char *headers_end = strstr(g_accumulator, "\r\n\r\n");
			if (!headers_end)
			{
				keep_processing = false;
				break;
			}
			http_request_t req = parse_ipd_header(g_accumulator);
			if (!req.is_valid)
			{
				// IPD non valide, resynchronisation
				char *next_ipd = _find_next_ipd(g_accumulator + IPD_HEADER_MIN_LEN, g_acc_len - IPD_HEADER_MIN_LEN);
				if (next_ipd)
				{
					int shift = next_ipd - g_accumulator;
					memmove(g_accumulator, next_ipd, g_acc_len - shift);
					g_acc_len -= shift;
					g_accumulator[g_acc_len] = '\0';
				}
				else
				{
					g_acc_len = 0;
					g_accumulator[0] = '\0';
				}
				g_parse_state = PARSE_STATE_SEARCHING_IPD;
				keep_processing = false;
				break;
			}
			g_parse_state = PARSE_STATE_READING_PAYLOAD;
			// Pas de break, on enchaîne
		}
		case PARSE_STATE_READING_PAYLOAD:
		{
			http_request_t req = parse_ipd_header(g_accumulator);
			int ipd_payload_len = req.content_length;
			char *colon_pos = strchr(g_accumulator, ':');
			if (!colon_pos)
			{
				g_parse_state = PARSE_STATE_SEARCHING_IPD;
				keep_processing = false;
				break;
			}
			colon_pos++;
			int received_payload = g_acc_len - (colon_pos - g_accumulator);
			if (received_payload < ipd_payload_len)
			{
				// Attendre plus de données
				keep_processing = false;
				break;
			}

			// === AJOUT POUR COPIER L'IP DU CLIENT DANS g_connections ===
			int idx = -1;
			// Cherche une connexion existante
			for (int i = 0; i < g_connection_count; ++i)
			{
				if (g_connections[i].conn_id == req.conn_id)
				{
					idx = i;
					break;
				}
			}
			// Sinon, cherche un slot inactif à réutiliser
			if (idx == -1)
			{
				for (int i = 0; i < g_connection_count; ++i)
				{
					if (!g_connections[i].is_active)
					{
						idx = i;
						g_connections[idx].conn_id = req.conn_id;
						break;
					}
				}
			}
			// Sinon, ajoute un nouveau slot si possible
			if (idx == -1 && g_connection_count < ESP01_MAX_CONNECTIONS)
			{
				idx = g_connection_count++;
				g_connections[idx].conn_id = req.conn_id;
				char dbg[64];
				snprintf(dbg, sizeof(dbg), "[DEBUG] Nouvelle connexion TCP id=%d", req.conn_id);
				_esp_logln(dbg);
			}
			if (idx != -1)
			{
				g_connections[idx].last_activity = HAL_GetTick();
				g_connections[idx].is_active = true;
				if (req.has_ip)
				{
					strncpy(g_connections[idx].client_ip, req.client_ip, ESP01_MAX_IP_LEN);
					char dbg[64];
					snprintf(dbg, sizeof(dbg), "[DEBUG] IP client détectée : %s", req.client_ip);
					_esp_logln(dbg);
				}
				else
				{
					strcpy(g_connections[idx].client_ip, "N/A");
					_esp_logln("[DEBUG] IP client non transmise par l'ESP01");
				}
				g_connections[idx].server_port = req.client_port > 0 ? req.client_port : 80;
				char dbg[64];
				snprintf(dbg, sizeof(dbg), "[DEBUG] Port client : %u", g_connections[idx].server_port);
				_esp_logln(dbg);
			}
			// === FIN AJOUT DEBUG ===

			// Parse la requête HTTP
			http_parsed_request_t parsed = {0};
			if (esp01_parse_http_request(colon_pos, &parsed) == ESP01_OK && parsed.is_valid)
			{
				// --- Ajout : Parsing des headers HTTP ---
				const char *first_line_end = strstr(colon_pos, "\r\n");
				if (first_line_end)
				{
					const char *headers_start = first_line_end + 2;
					const char *headers_end = strstr(headers_start, "\r\n\r\n");
					if (headers_end)
					{
						size_t headers_len = headers_end - headers_start;
						if (headers_len < sizeof(parsed.headers_buf))
						{
							memcpy(parsed.headers_buf, headers_start, headers_len);
							parsed.headers_buf[headers_len] = '\0';
						}
						else
						{
							parsed.headers_buf[0] = '\0';
						}
					}
					else
					{
						parsed.headers_buf[0] = '\0';
					}
				}
				else
				{
					parsed.headers_buf[0] = '\0';
				}
				// --- Fin ajout ---

				esp01_route_handler_t handler = esp01_find_route_handler(parsed.path);
				if (handler)
				{
					handler(req.conn_id, &parsed);
				}
				else
				{
					// Réponse 204 No Content pour /favicon.ico
					if (strcmp(parsed.path, "/favicon.ico") == 0)
					{
						esp01_send_http_response(req.conn_id, 204, "image/x-icon", NULL, 0);
					}
					else
					{
						esp01_send_404_response(req.conn_id);
					}
				}
			}

			// Gère le body si nécessaire (POST/PUT)
			char *headers_end = strstr(colon_pos, "\r\n\r\n");
			int headers_len = headers_end ? (headers_end + 4 - colon_pos) : 0;
			int body_len = ipd_payload_len - headers_len;
			if ((strcmp(parsed.method, "POST") == 0 || strcmp(parsed.method, "PUT") == 0) && body_len > 0)
			{
				discard_http_payload(body_len);
			}

			// Retire la trame traitée de l'accumulateur
			int total_to_remove = (colon_pos - g_accumulator) + ipd_payload_len;
			if (g_acc_len > total_to_remove)
			{
				memmove(g_accumulator, g_accumulator + total_to_remove, g_acc_len - total_to_remove);
				g_acc_len -= total_to_remove;
				g_accumulator[g_acc_len] = '\0';
				g_parse_state = PARSE_STATE_SEARCHING_IPD;
				// On boucle pour traiter la prochaine trame si présente
			}
			else
			{
				g_acc_len = 0;
				g_accumulator[0] = '\0';
				g_parse_state = PARSE_STATE_SEARCHING_IPD;
				keep_processing = false;
			}
			break;
		}
		default:
			keep_processing = false;
			break;
		}
	}

	g_processing_request = false;
	// Nettoyage automatique des connexions inactives
	esp01_cleanup_inactive_connections();
}

// ==================== ROUTAGE HTTP ====================

// Efface toutes les routes HTTP enregistrées
void esp01_clear_routes(void)
{
	_esp_logln("[ROUTE] Effacement de toutes les routes HTTP");
	g_route_count = 0;
}

// Ajoute une route HTTP (chemin + handler)
ESP01_Status_t esp01_add_route(const char *path, esp01_route_handler_t handler)
{
	VALIDATE_PARAM(path, ESP01_FAIL);
	VALIDATE_PARAM(handler, ESP01_FAIL);
	VALIDATE_PARAM(g_route_count < ESP01_MAX_ROUTES, ESP01_FAIL);

	strncpy(g_routes[g_route_count].path, path, sizeof(g_routes[g_route_count].path) - 1);
	g_routes[g_route_count].path[sizeof(g_routes[g_route_count].path) - 1] = '\0';
	g_routes[g_route_count].handler = handler;
	g_route_count++;
	char dbg[80];
	snprintf(dbg, sizeof(dbg), "[WEB] Route ajoutée : %s (total %d)", path, g_route_count);
	_esp_logln(dbg);
	return ESP01_OK;
}

// Recherche le handler associé à un chemin donné
esp01_route_handler_t esp01_find_route_handler(const char *path)
{
	for (int i = 0; i < g_route_count; i++)
	{
		if (strcmp(g_routes[i].path, path) == 0)
		{
			char dbg[80];
			snprintf(dbg, sizeof(dbg), "[WEB] Route trouvée pour path : %s", path);
			_esp_logln(dbg);
			return g_routes[i].handler;
		}
	}
	char dbg[80];
	snprintf(dbg, sizeof(dbg), "[WEB] Aucune route trouvée pour path : %s", path);
	_esp_logln(dbg);
	return NULL;
}

// ==================== FONCTIONS DIVERSES & UTILITAIRES ====================

// Teste la communication AT avec l'ESP01
ESP01_Status_t esp01_test_at(uint8_t *dma_rx_buf, uint16_t dma_buf_size)
{
	char response[256];
	ESP01_Status_t status = esp01_send_raw_command_dma("AT", response, sizeof(response), "OK", 2000); // Test AT
	return status;
}

// Récupère l'adresse IP courante de l'ESP01
ESP01_Status_t esp01_get_current_ip(char *ip_buffer, size_t buffer_size)
{
	VALIDATE_PARAM(ip_buffer && buffer_size > 0, ESP01_FAIL);

	char response[512];
	ESP01_Status_t status = esp01_send_raw_command_dma("AT+CIFSR", response, sizeof(response), "OK", ESP01_TIMEOUT_LONG);

	if (status != ESP01_OK)
		return ESP01_FAIL;

	// Recherche STAIP (mode STA), accepte aussi +CIFSR:STAIP,"..."
	char *ip_line = strstr(response, "STAIP,\"");
	if (!ip_line)
		ip_line = strstr(response, "+CIFSR:STAIP,\"");
	if (!ip_line)
		ip_line = strstr(response, "APIP,\"");
	if (!ip_line)
		ip_line = strstr(response, "+CIFSR:APIP,\"");

	if (ip_line)
	{
		ip_line = strchr(ip_line, '"');
		if (ip_line)
		{
			ip_line++; // Passe le premier guillemet
			char *end_quote = strchr(ip_line, '"');
			if (end_quote)
			{
				size_t ip_len = end_quote - ip_line;
				if (ip_len < buffer_size)
				{
					strncpy(ip_buffer, ip_line, ip_len);
					ip_buffer[ip_len] = '\0';
					return ESP01_OK;
				}
			}
		}
	}

	_esp_logln("[IP] Adresse IP non trouvée dans la réponse ESP01");
	if (buffer_size > 0)
		ip_buffer[0] = '\0';

	return ESP01_FAIL;
}

// Envoie une requête HTTP GET et récupère la réponse
ESP01_Status_t esp01_http_get(const char *host, uint16_t port, const char *path, char *response, size_t response_size)
{
	char dbg[128];
	snprintf(dbg, sizeof(dbg), "esp01_http_get: GET http://%s:%u%s", host, port, path);
	_esp_logln(dbg);

	char cmd[ESP01_DMA_RX_BUF_SIZE];
	snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d", host, port);
	char resp[ESP01_DMA_RX_BUF_SIZE];
	if (esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 5000) != ESP01_OK)
	{
		_esp_logln("[HTTP] esp01_http_get: AT+CIPSTART échoué");
		return ESP01_FAIL;
	}

	char http_req[256];
	snprintf(http_req, sizeof(http_req),
			 "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host); // Prépare la requête GET

	snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d", (int)strlen(http_req)); // Prépare l'envoi
	if (esp01_send_raw_command_dma(cmd, resp, sizeof(resp), ">", 3000) != ESP01_OK)
		return ESP01_FAIL;

	if (esp01_send_raw_command_dma(http_req, resp, sizeof(resp), "CLOSED", 8000) != ESP01_OK)
		return ESP01_FAIL;
	// Copie la réponse dans le buffer utilisateur
	if (response && response_size > 0)
	{
		strncpy(response, resp, response_size - 1);
		response[response_size - 1] = '\0';
	}

	return ESP01_OK;
}

const char *esp01_get_error_string(ESP01_Status_t status)
{
	switch (status)
	{
	case ESP01_OK:
		return "OK";
	case ESP01_FAIL:
		return "Echec général";
	case ESP01_TIMEOUT:
		return "Timeout";
	case ESP01_NOT_INITIALIZED:
		return "Non initialisé";
	case ESP01_INVALID_PARAM:
		return "Paramètre invalide";
	case ESP01_BUFFER_OVERFLOW:
		return "Débordement de buffer";
	case ESP01_WIFI_NOT_CONNECTED:
		return "WiFi non connecté";
	case ESP01_HTTP_PARSE_ERROR:
		return "Erreur parsing HTTP";
	case ESP01_ROUTE_NOT_FOUND:
		return "Route non trouvée";
	case ESP01_CONNECTION_ERROR:
		return "Erreur de connexion";
	case ESP01_MEMORY_ERROR:
		return "Erreur mémoire";
	default:
		return "Code d'erreur inconnu";
	}
}

ESP01_Status_t esp01_get_at_version(char *version_buf, size_t buf_size)
{
	if (!version_buf || buf_size == 0)
		return ESP01_FAIL;

	char response[ESP01_DMA_RX_BUF_SIZE] = {0};
	ESP01_Status_t status = esp01_send_raw_command_dma("AT+GMR", response, sizeof(response), "OK", ESP01_TIMEOUT_SHORT);

	if (status != ESP01_OK)
	{
		version_buf[0] = '\0';
		return ESP01_FAIL;
	}

	// Cherche la première ligne contenant "AT version:" ou "SDK version:"
	char *start = strstr(response, "AT version:");
	if (!start)
		start = strstr(response, "SDK version:");
	if (!start)
		start = response;

	// Copie la première ligne trouvée
	char *end = strchr(start, '\r');
	if (!end)
		end = strchr(start, '\n');
	size_t len = end ? (size_t)(end - start) : strlen(start);
	if (len >= buf_size)
		len = buf_size - 1;
	strncpy(version_buf, start, len);
	version_buf[len] = '\0';

	return ESP01_OK;
}

// Parse les headers HTTP (après la première ligne), callback appelé pour chaque header trouvé
ESP01_Status_t parse_http_headers(const char *headers_start, void (*on_header)(http_header_kv_t *header, void *user), void *user)
{
	const char *p = headers_start;
	while (*p && !(p[0] == '\r' && p[1] == '\n'))
	{
		// Trouve la fin de la ligne
		const char *line_end = strstr(p, "\r\n");
		if (!line_end)
			break;

		// Trouve le séparateur ':'
		const char *colon = strchr(p, ':');
		if (colon && colon < line_end)
		{
			// Ignore les espaces après ':'
			const char *val_start = colon + 1;
			while (*val_start == ' ' && val_start < line_end)
				val_start++;

			http_header_kv_t header;
			header.key = p;
			header.key_len = colon - p;
			header.value = val_start;
			header.value_len = line_end - val_start;

			if (on_header)
				on_header(&header, user);
		}
		p = line_end + 2; // Passe à la ligne suivante
	}
	return ESP01_OK;
}

// ==================== GESTION DES CONNEXIONS INACTIVES ====================

#define ESP01_CONN_TIMEOUT_MS 30000 // 30 secondes d'inactivité

int esp01_get_active_connection_count(void)
{
	int count = 0;
	for (int i = 0; i < g_connection_count; ++i)
	{
		if (g_connections[i].is_active)
			count++;
	}
	return count;
}
