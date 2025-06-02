/*
 * STM32_WifiESP.c
 * Driver STM32 <-> ESP01 (AT)
 * Version Final V1.0
 * 2025 - manu
 */

// ==================== INCLUDES ====================

#include "STM32_WifiESP.h" // Fichier d'en-tête principal du driver

// ==================== DÉFINITIONS & MACROS ====================

#ifndef VALIDATE_PARAM
#define VALIDATE_PARAM(expr, ret) \
	do                            \
	{                             \
		if (!(expr))              \
			return (ret);         \
	} while (0)
#endif

#define ESP01_CONN_TIMEOUT_MS 30000 // Timeout pour les connexions (30 secondes)

// ==================== VARIABLES GLOBALES ====================

esp01_stats_t g_stats = {0};								  // Statistiques globales du driver
connection_info_t g_connections[ESP01_MAX_CONNECTIONS] = {0}; // Tableau des connexions actives
int g_connection_count = 0;									  // Nombre de connexions actives
uint16_t g_server_port = 80;								  // Port du serveur web

// ==================== VARIABLES INTERNES ====================
static UART_HandleTypeDef *g_esp_uart = NULL;		  // UART principal pour l'ESP01
static UART_HandleTypeDef *g_debug_uart = NULL;		  // UART pour le debug
static uint8_t *g_dma_rx_buf = NULL;				  // Buffer DMA RX
static uint16_t g_dma_buf_size = 0;					  // Taille du buffer DMA RX
static volatile uint16_t g_rx_last_pos = 0;			  // Dernière position lue dans le buffer DMA RX
static char g_accumulator[ESP01_DMA_RX_BUF_SIZE * 2]; // Accumulateur global pour les données reçues
static uint16_t g_acc_len = 0;						  // Longueur actuelle de l'accumulateur
static volatile bool g_processing_request = false;	  // Indique si une requête est en cours de traitement
static esp01_route_t g_routes[ESP01_MAX_ROUTES];	  // Tableau des routes HTTP enregistrées
static int g_route_count = 0;						  // Nombre de routes HTTP enregistrées

// Etat de parsing des données reçues
typedef enum
{
	PARSE_STATE_SEARCHING_IPD,
	PARSE_STATE_READING_HEADER,
	PARSE_STATE_READING_PAYLOAD
} parse_state_t;

// État de parsing actuel
static parse_state_t g_parse_state = PARSE_STATE_SEARCHING_IPD;

/***************************************************************/
// Fonctions internes utilisées par le driver ESP01
/***************************************************************/
/**
 * @brief Affiche un message de debug sur l'UART de debug si activé.
 * @param msg Chaîne C terminée par '\0' à afficher.
 */
static void _esp_logln(const char *msg)
{
	if (ESP01_DEBUG && g_debug_uart && msg) // Si debug activé et UART debug disponible
	{
		HAL_UART_Transmit(g_debug_uart, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY); // Envoie le message
		HAL_UART_Transmit(g_debug_uart, (uint8_t *)"\r\n", 2, HAL_MAX_DELAY);		 // Ajoute un retour à la ligne
	}
}

/**
 * @brief Calcule la position actuelle d'écriture du DMA dans le buffer circulaire.
 * @return Position d'écriture (offset dans le buffer DMA).
 */
static uint16_t _get_dma_write_pos(void)
{
	return g_dma_buf_size - __HAL_DMA_GET_COUNTER(g_esp_uart->hdmarx); // Position d'écriture dans le buffer circulaire
}

/**
 * @brief Calcule le nombre de nouveaux octets disponibles dans le buffer DMA.
 * @return Nombre d'octets disponibles à lire.
 */
static uint16_t _get_available_bytes(void)
{
	uint16_t write_pos = _get_dma_write_pos();				 // Récupère la position actuelle d'écriture du DMA
	if (write_pos >= g_rx_last_pos)							 // Si la position d'écriture est après ou égale à la dernière position lue
		return write_pos - g_rx_last_pos;					 //   -> Nombre d'octets disponibles = différence directe
	else													 // Sinon (le buffer a "tourné" en mode circulaire)
		return (g_dma_buf_size - g_rx_last_pos) + write_pos; //   -> Nombre d'octets = fin du buffer + début du buffer
}

/**
 * @brief Récupère les nouveaux octets reçus via DMA UART.
 * @param buffer Buffer de destination.
 * @param buffer_size Taille du buffer de destination.
 * @return Nombre d'octets copiés dans le buffer.
 */
static uint16_t esp01_get_new_data(uint8_t *buffer, uint16_t buffer_size)
{
	if (!g_esp_uart || !g_dma_rx_buf || !buffer || buffer_size == 0) // Vérifie les pointeurs et la taille du buffer
	{
		if (buffer && buffer_size > 0)
			buffer[0] = '\0'; // Met fin de chaîne si possible
		return 0;			  // Rien à lire
	}

	uint16_t available = _get_available_bytes(); // Récupère le nombre d'octets disponibles dans le buffer DMA
	if (available == 0)
	{
		buffer[0] = '\0'; // Met fin de chaîne
		return 0;		  // Aucun octet à lire
	}

	uint16_t to_copy = (available < buffer_size - 1) ? available : buffer_size - 1; // Nombre d'octets à copier (en laissant la place pour '\0')
	uint16_t copied = 0;															// Compteur d'octets copiés
	uint16_t write_pos = _get_dma_write_pos();										// Position d'écriture actuelle du DMA

	// Cas où les données ne sont pas fragmentées dans le buffer circulaire
	if (write_pos > g_rx_last_pos)
	{
		memcpy(buffer, &g_dma_rx_buf[g_rx_last_pos], to_copy); // Copie directe
		copied = to_copy;
	}
	else
	{
		// Cas où les données sont fragmentées (fin du buffer circulaire)
		uint16_t first_chunk = g_dma_buf_size - g_rx_last_pos; // Taille du premier morceau (jusqu'à la fin du buffer)
		if (first_chunk >= to_copy)
		{
			memcpy(buffer, &g_dma_rx_buf[g_rx_last_pos], to_copy); // Tout tient dans le premier morceau
			copied = to_copy;
		}
		else
		{
			memcpy(buffer, &g_dma_rx_buf[g_rx_last_pos], first_chunk); // Copie la fin du buffer
			copied = first_chunk;
			uint16_t remaining = to_copy - first_chunk; // Reste à copier depuis le début du buffer
			if (remaining > 0)
			{
				memcpy(buffer + first_chunk, &g_dma_rx_buf[0], remaining); // Copie le début du buffer
				copied += remaining;
			}
		}
	}

	buffer[copied] = '\0';									   // Ajoute le caractère de fin de chaîne
	g_rx_last_pos = (g_rx_last_pos + copied) % g_dma_buf_size; // Met à jour la position de lecture dans le buffer circulaire

	if (copied > 0)
	{
		char dbg[64];
		snprintf(dbg, sizeof(dbg), "[GET NEW DATA]  %u octets reçus", copied); // Message de debug
		_esp_logln(dbg);
	}

	return copied; // Retourne le nombre d'octets copiés
}

/**
 * @brief Accumule les données reçues et cherche un motif (pattern) dans l'accumulateur.
 * @param pattern Motif à rechercher.
 * @param timeout_ms Timeout en millisecondes.
 * @param clear_first Si vrai, vide l'accumulateur avant de commencer.
 * @return true si le motif est trouvé, false sinon (timeout).
 */
static bool _accumulate_and_search(const char *pattern, uint32_t timeout_ms, bool clear_first)
{
	uint32_t start_tick = HAL_GetTick(); // Début du timeout
	char temp_buf[256];					 // Buffer temporaire pour les nouveaux octets

	if (clear_first) // Si on doit vider l'accumulateur
	{
		g_acc_len = 0;			 // Vide la longueur de l'accumulateur
		g_accumulator[0] = '\0'; // Vide le contenu de l'accumulateur
	}

	while ((HAL_GetTick() - start_tick) < timeout_ms) // Boucle jusqu'au timeout
	{
		uint16_t new_data = esp01_get_new_data((uint8_t *)temp_buf, sizeof(temp_buf)); // Récupère les nouveaux octets
		if (new_data > 0)															   // Si des octets ont été reçus
		{
			uint16_t space_left = sizeof(g_accumulator) - g_acc_len - 1;	   // Espace restant dans l'accumulateur
			uint16_t to_add = (new_data < space_left) ? new_data : space_left; // Nombre d'octets à ajouter
			if (to_add > 0)													   // Si on peut ajouter des octets
			{
				memcpy(&g_accumulator[g_acc_len], temp_buf, to_add); // Ajoute les octets reçus à l'accumulateur
				g_acc_len += to_add;								 // Met à jour la longueur de l'accumulateur
				g_accumulator[g_acc_len] = '\0';					 // Termine la chaîne
			}
			if (strstr(g_accumulator, pattern)) // Cherche le motif dans l'accumulateur
				return true;					// Motif trouvé, retourne vrai
		}
		else // Si aucun nouvel octet reçu
		{
			HAL_Delay(10); // Petite pause avant de réessayer
		}
	}
	return false; // Timeout sans trouver le motif
}

/**
 * @brief Vide le buffer RX UART/DMA pendant un certain temps (utilisé pour nettoyer avant une commande AT)
 * @param timeout_ms Durée en millisecondes
 */
static void _flush_rx_buffer(uint32_t timeout_ms)
{
	char temp_buf[ESP01_DMA_RX_BUF_SIZE]; // Buffer temporaire pour vider les octets
	uint32_t start_time = HAL_GetTick();  // Début du timeout

	while ((HAL_GetTick() - start_time) < timeout_ms) // Boucle jusqu'à la fin du timeout
	{
		uint16_t len = esp01_get_new_data((uint8_t *)temp_buf, sizeof(temp_buf)); // Vide les nouveaux octets reçus
		if (len == 0)															  // Si aucun octet reçu
		{
			HAL_Delay(10); // Petite pause avant de réessayer
		}
		else // Si des octets ont été lus
		{
			if (ESP01_DEBUG) // Si le debug est activé
			{
				temp_buf[len] = '\0';		// Termine la chaîne pour affichage
				_esp_logln("Buffer vidé:"); // Message debug
				_esp_logln(temp_buf);		// Affiche le contenu vidé
			}
		}
	}
}

/**
 * @brief Cherche le prochain +IPD valide dans un buffer (pour resynchroniser en cas de trame corrompue)
 * @param buffer Buffer à analyser
 * @param buffer_len Taille du buffer
 * @return Pointeur sur le début du prochain +IPD, ou NULL si non trouvé
 */
static char *_find_next_ipd(char *buffer, int buffer_len)
{
	char *search_pos = buffer; // Position de recherche actuelle dans le buffer
	char *ipd_pos;			   // Pointeur temporaire pour le prochain "+IPD,"

	while ((ipd_pos = strstr(search_pos, "+IPD,")) != NULL) // Recherche la prochaine occurrence de "+IPD,"
	{
		// Vérifie la présence du ':' après "+IPD,"
		char *colon = strchr(ipd_pos, ':');				// Cherche le caractère ':' après "+IPD,"
		if (colon && (colon - buffer) < buffer_len - 1) // Vérifie que ':' est bien dans la fenêtre du buffer
		{
			return ipd_pos; // Si trouvé, retourne le pointeur sur "+IPD,"
		}
		// Continue la recherche après ce "+IPD," invalide
		search_pos = ipd_pos + IPD_HEADER_MIN_LEN; // Avance la position de recherche d'au moins la taille minimale d'un header IPD
	}
	return NULL; // Aucun "+IPD," valide trouvé dans le buffer
}

/**
 * @brief Parse un header IPD brut pour extraire les infos de connexion
 * @param data Chaîne contenant le header IPD
 * @return Structure http_request_t remplie
 */
static http_request_t parse_ipd_header(const char *data)
{
	http_request_t request = {0}; // Initialise la structure à zéro

	char *ipd_start = strstr(data, "+IPD,"); // Cherche le début du header +IPD
	if (!ipd_start)							 // Si non trouvé
	{
		_esp_logln("parse_ipd_header: +IPD non trouvé"); // Log d'erreur
		return request;									 // Retourne une structure invalide
	}

	// Tente de parser le format complet : +IPD,<id>,<len>,"<ip>",<port>:
	int parsed = sscanf(ipd_start, "+IPD,%d,%d,\"%15[0-9.]\",%d:",
						&request.conn_id,		 // ID de connexion
						&request.content_length, // Longueur du payload
						request.client_ip,		 // IP du client
						&request.client_port);	 // Port du client
	if (parsed == 4)							 // Si tous les champs sont trouvés
	{
		request.is_valid = true; // Marque la structure comme valide
		request.has_ip = true;	 // Indique que l'IP est présente
		char dbg[80];
		snprintf(dbg, sizeof(dbg),
				 "[PARSE] parse_ipd_header: IPD avec IP %s:%d, conn_id=%d, len=%d",
				 request.client_ip, request.client_port, request.conn_id, request.content_length); // Prépare un message de debug
		_esp_logln(dbg);																		   // Affiche le message de debug
	}
	else
	{
		// Tente de parser le format court : +IPD,<id>,<len>:
		parsed = sscanf(ipd_start, "+IPD,%d,%d:",
						&request.conn_id,		  // ID de connexion
						&request.content_length); // Longueur du payload
		if (parsed == 2)						  // Si les deux champs sont trouvés
		{
			request.is_valid = true; // Marque la structure comme valide
			request.has_ip = false;	 // Indique que l'IP n'est pas présente
			char dbg[80];
			snprintf(dbg, sizeof(dbg),
					 "[PARSE] parse_ipd_header: IPD sans IP, conn_id=%d, len=%d",
					 request.conn_id, request.content_length); // Prépare un message de debug
			_esp_logln(dbg);								   // Affiche le message de debug
		}
		else
		{
			_esp_logln("[PARSE] parse_ipd_header: format IPD non reconnu"); // Format non reconnu
		}
	}
	return request; // Retourne la structure remplie (ou invalide)
}

/**
 * @brief Ignore/consomme un payload HTTP restant (ex: body POST non traité)
 * @param expected_length Nombre d'octets à ignorer
 */
static void discard_http_payload(int expected_length)
{
	char discard_buf[256];							  // Buffer temporaire pour lire et ignorer les données
	int remaining = expected_length;				  // Nombre d'octets restant à ignorer
	uint32_t timeout_start = HAL_GetTick();			  // Timestamp de début pour le timeout
	const uint32_t timeout_ms = ESP01_TIMEOUT_MEDIUM; // Timeout maximum pour l'opération

	_esp_logln("[HTTP] discard_http_payload: début vidage payload HTTP"); // Log de début

	while (remaining > 0 && (HAL_GetTick() - timeout_start) < timeout_ms) // Boucle tant qu'il reste des octets à ignorer et pas de timeout
	{
		int to_read = (remaining < sizeof(discard_buf)) ? remaining : sizeof(discard_buf); // Détermine combien d'octets lire (max taille du buffer)
		uint16_t read_len = esp01_get_new_data((uint8_t *)discard_buf, to_read);		   // Lit les nouveaux octets reçus

		if (read_len > 0) // Si des octets ont été lus
		{
			remaining -= read_len; // Décrémente le nombre d'octets restant à ignorer
			char debug_msg[50];
			snprintf(debug_msg, sizeof(debug_msg), "[HTTP] Ignoré %d octets, reste %d", read_len, remaining); // Message de debug
			_esp_logln(debug_msg);																			  // Affiche le message de debug
		}
		else // Si rien n'a été lu
		{
			HAL_Delay(10); // Attend un court instant avant de réessayer
		}
	}

	if (remaining > 0) // Si tous les octets n'ont pas pu être ignorés avant le timeout
	{
		char warn_msg[100];
		snprintf(warn_msg, sizeof(warn_msg), "[HTTP] AVERTISSEMENT: %d octets non lus", remaining); // Message d'avertissement
		_esp_logln(warn_msg);																		// Affiche l'avertissement
	}
	else // Si tout le payload a été ignoré avec succès
	{
		_esp_logln("[HTTP] Payload HTTP complètement vidé"); // Log de succès
	}
}

/**
 * @brief Définit le mode WiFi (STA/AP)
 * @param mode Mode WiFi à utiliser
 * @return ESP01_OK si succès, code d'erreur sinon
 */
static ESP01_Status_t esp01_set_wifi_mode(ESP01_WifiMode_t mode)
{
	char cmd[32], resp[64];														// Buffers pour la commande AT et la réponse
	snprintf(cmd, sizeof(cmd), "AT+CWMODE=%d", mode);							// Prépare la commande AT pour définir le mode WiFi
	ESP01_Status_t status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), // Envoie la commande AT et attend "OK"
													   "OK", ESP01_TIMEOUT_SHORT);
	return status; // Retourne le statut de l'opération
}

/**
 * @brief Connecte l'ESP01 à un réseau WiFi (STA)
 * @param ssid SSID du réseau WiFi
 * @param password Mot de passe du réseau WiFi
 * @return ESP01_OK si succès, code d'erreur sinon
 */
static ESP01_Status_t esp01_connect_wifi(const char *ssid, const char *password)
{
	char cmd[128], resp[128];											  // Buffers pour la commande AT et la réponse
	snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password); // Prépare la commande AT pour rejoindre le réseau WiFi
	ESP01_Status_t status = esp01_send_raw_command_dma(					  // Envoie la commande AT et attend la réponse
		cmd,															  // Commande AT à envoyer
		resp,															  // Buffer pour la réponse
		sizeof(resp),													  // Taille du buffer réponse
		"OK",															  // Motif attendu pour succès
		ESP01_TIMEOUT_WIFI);											  // Timeout pour la connexion WiFi
	return status;														  // Retourne le statut de la connexion
}

/**
 * @brief Attend un motif précis dans le flux RX (avec timeout)
 * @param pattern Motif à attendre
 * @param timeout_ms Timeout en millisecondes
 * @return ESP01_OK si trouvé, ESP01_TIMEOUT sinon
 */
static ESP01_Status_t esp01_wait_for_pattern(const char *pattern, uint32_t timeout_ms)
{
	// Appelle la fonction interne pour accumuler et rechercher le motif dans le flux RX
	if (_accumulate_and_search(pattern, timeout_ms, false)) // Si le motif est trouvé dans le délai imparti
		return ESP01_OK;									//   -> Retourne OK
	return ESP01_TIMEOUT;									// Sinon, retourne TIMEOUT
}

/***************************************************************/
// Fonctions utilisateur pour le driver ESP01
/***************************************************************/

// ==================== DRIVER ====================
/**
 * @brief Vide le buffer RX UART/DMA pendant un certain temps (externe)
 * @param timeout_ms Durée en millisecondes
 * @return ESP01_OK toujours
 */
ESP01_Status_t esp01_flush_rx_buffer(uint32_t timeout_ms)
{
	_flush_rx_buffer(timeout_ms);		  // Appelle la fonction interne pour vider le buffer RX
	_esp_logln("[ESP01] Buffer RX vidé"); // Log de succès
	return ESP01_OK;					  // Retourne OK
}

/**
 * @brief Initialise le driver ESP01 (UART, debug UART, buffer DMA, taille buffer)
 * @param huart_esp UART pour l'ESP01
 * @param huart_debug UART pour debug
 * @param dma_rx_buf Buffer DMA RX
 * @param dma_buf_size Taille du buffer DMA RX
 * @return ESP01_OK si succès, code d'erreur sinon
 */
ESP01_Status_t esp01_init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug,
						  uint8_t *dma_rx_buf, uint16_t dma_buf_size)
{
	_esp_logln("[ESP01] Initialisation UART/DMA pour ESP01");
	g_esp_uart = huart_esp;					   // UART pour l'ESP01
	g_debug_uart = huart_debug;				   // UART pour le debug
	g_dma_rx_buf = dma_rx_buf;				   // Buffer DMA
	g_dma_buf_size = dma_buf_size;			   // Taille buffer
	g_rx_last_pos = 0;						   // Reset position
	g_acc_len = 0;							   // Reset accumulateur
	g_accumulator[0] = '\0';				   // Vide l'accumulateur
	g_parse_state = PARSE_STATE_SEARCHING_IPD; // Reset l'état de parsing

	if (!g_esp_uart || !g_dma_rx_buf || g_dma_buf_size == 0) // Vérifie les pointeurs et la taille du buffer
	{
		_esp_logln("[ESP01] ESP01 Init Error: Invalid params"); // Log d'erreur
		return ESP01_NOT_INITIALIZED;							// Retourne erreur d'initialisation
	}

	// Lance la réception DMA
	if (HAL_UART_Receive_DMA(g_esp_uart, g_dma_rx_buf, g_dma_buf_size) != HAL_OK)
	{
		_esp_logln("[ESP01] ESP01 Init Error: HAL_UART_Receive_DMA failed"); // Log d'erreur
		return ESP01_FAIL;													 // Retourne échec de l'initialisation
	}

	_esp_logln("[ESP01] --- ESP01 Driver Init OK ---"); // Log de succès
	HAL_Delay(100);										// Court délai pour stabiliser l'initialisation
	return ESP01_OK;									// Retourne succès
}

/**
 * @brief Envoie une commande AT et récupère la réponse (DMA, timeout, etc.)
 * @param cmd Commande AT à envoyer
 * @param response_buffer Buffer pour la réponse
 * @param max_response_size Taille max du buffer réponse
 * @param expected_terminator Motif attendu pour la fin de réponse
 * @param timeout_ms Timeout en millisecondes
 * @return ESP01_OK si motif trouvé, ESP01_TIMEOUT sinon
 */
ESP01_Status_t esp01_send_raw_command_dma(const char *cmd, char *response_buffer,
										  uint32_t max_response_size,
										  const char *expected_terminator,
										  uint32_t timeout_ms)
{
	if (!g_esp_uart || !cmd || !response_buffer) // Vérifie les pointeurs nécessaires
		return ESP01_NOT_INITIALIZED;

	const char *terminator = expected_terminator ? expected_terminator : "OK"; // Définit le motif de terminaison attendu

	char full_cmd[ESP01_DMA_RX_BUF_SIZE];							   // Buffer pour la commande complète
	int cmd_len = snprintf(full_cmd, sizeof(full_cmd), "%s\r\n", cmd); // Prépare la commande AT avec retour chariot

	_flush_rx_buffer(100); // Vide le buffer RX avant d'envoyer

	if (HAL_UART_Transmit(g_esp_uart, (uint8_t *)full_cmd, cmd_len, ESP01_TIMEOUT_SHORT) != HAL_OK) // Envoie la commande AT
		return ESP01_FAIL;

	bool found = _accumulate_and_search(terminator, timeout_ms, true); // Attend la réponse contenant le motif attendu

	uint32_t copy_len = (g_acc_len < max_response_size - 1) ? g_acc_len : max_response_size - 1; // Détermine la taille à copier
	memcpy(response_buffer, g_accumulator, copy_len);											 // Copie la réponse dans le buffer utilisateur
	response_buffer[copy_len] = '\0';															 // Termine la chaîne

	return found ? ESP01_OK : ESP01_TIMEOUT; // Retourne le statut selon la présence du motif
}

/**
 * @brief Configure la connexion WiFi (mode, SSID, mot de passe, DHCP ou IP statique)
 * @param mode Mode WiFi (STA/AP)
 * @param ssid SSID du réseau
 * @param password Mot de passe
 * @param use_dhcp true pour DHCP, false pour IP statique
 * @param ip Adresse IP statique (si DHCP désactivé)
 * @param gateway Passerelle (si IP statique)
 * @param netmask Masque réseau (si IP statique)
 * @return ESP01_OK si succès, code d'erreur sinon
 */
ESP01_Status_t esp01_connect_wifi_config(
	ESP01_WifiMode_t mode, // Mode WiFi (STA ou AP)
	const char *ssid,	   // SSID du réseau
	const char *password,  // Mot de passe du réseau
	bool use_dhcp,		   // true = DHCP, false = IP statique
	const char *ip,		   // IP statique (si utilisé)
	const char *gateway,   // Passerelle (si IP statique)
	const char *netmask)   // Masque réseau (si IP statique)
{
	ESP01_Status_t status;			  // Code de retour
	char cmd[ESP01_DMA_RX_BUF_SIZE];  // Buffer pour commandes AT
	char resp[ESP01_DMA_RX_BUF_SIZE]; // Buffer pour réponses AT

	_esp_logln("[WIFI] === Début configuration WiFi ==="); // Log début config

	// 1. Définir le mode WiFi (STA ou AP uniquement)
	_esp_logln("[WIFI] -> Définition du mode WiFi..."); // Log changement mode
	status = esp01_set_wifi_mode(mode);					// Définit le mode WiFi
	if (status != ESP01_OK)								// Si erreur
	{
		_esp_logln("[WIFI] !! ERREUR: esp01_set_wifi_mode"); // Log erreur
		return status;										 // Retourne erreur
	}
	HAL_Delay(300); // Délai stabilisation

	// 2. Si AP, configure le point d'accès
	if (mode == ESP01_WIFI_MODE_AP) // Si mode AP
	{
		_esp_logln("[WIFI] -> Configuration du point d'accès (AP)...");			  // Log config AP
		snprintf(cmd, sizeof(cmd), "AT+CWSAP=\"%s\",\"%s\",5,3", ssid, password); // Prépare commande AP
		status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 2000); // Envoie commande AP
		_esp_logln(resp);														  // Log réponse
		if (status != ESP01_OK)													  // Si erreur config AP
		{
			_esp_logln("[WIFI] !! ERREUR: Configuration AP"); // Log erreur
			return status;									  // Retourne erreur
		}
		HAL_Delay(300); // Délai après config AP
	}

	// 3. Configurer DHCP ou IP statique
	if (use_dhcp) // Si DHCP activé
	{
		if (mode == ESP01_WIFI_MODE_STA) // Si mode STA
		{
			_esp_logln("[WIFI] -> Activation du DHCP STA...");									  // Log DHCP STA
			status = esp01_send_raw_command_dma("AT+CWDHCP=1,1", resp, sizeof(resp), "OK", 2000); // Active DHCP STA
		}
		else if (mode == ESP01_WIFI_MODE_AP) // Si mode AP
		{
			_esp_logln("[WIFI] -> Activation du DHCP AP...");									  // Log DHCP AP
			status = esp01_send_raw_command_dma("AT+CWDHCP=2,1", resp, sizeof(resp), "OK", 2000); // Active DHCP AP
		}
		_esp_logln(resp);		// Log réponse
		if (status != ESP01_OK) // Si erreur DHCP
		{
			_esp_logln("[WIFI] !! ERREUR: Activation DHCP"); // Log erreur
			return status;									 // Retourne erreur
		}
	}
	else if (ip && gateway && netmask && mode == ESP01_WIFI_MODE_STA) // Si IP statique en mode STA
	{
		_esp_logln("[WIFI] -> Déconnexion du WiFi (CWQAP)...");					// Log déconnexion WiFi
		esp01_send_raw_command_dma("AT+CWQAP", resp, sizeof(resp), "OK", 2000); // Déconnecte WiFi

		_esp_logln("[WIFI] -> Désactivation du DHCP client...");							  // Log désactivation DHCP
		status = esp01_send_raw_command_dma("AT+CWDHCP=0,1", resp, sizeof(resp), "OK", 2000); // Désactive DHCP
		_esp_logln(resp);																	  // Log réponse
		if (status != ESP01_OK)																  // Si erreur DHCP off
		{
			_esp_logln("[WIFI] !! ERREUR: Désactivation DHCP"); // Log erreur
			return status;										// Retourne erreur
		}
		_esp_logln("[WIFI] -> Configuration IP statique...");								// Log config IP statique
		snprintf(cmd, sizeof(cmd), "AT+CIPSTA=\"%s\",\"%s\",\"%s\"", ip, gateway, netmask); // Prépare commande IP statique
		status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 2000);			// Envoie commande IP statique
		_esp_logln(resp);																	// Log réponse
		if (status != ESP01_OK)																// Si erreur IP statique
		{
			_esp_logln("[WIFI] !! ERREUR: Configuration IP statique"); // Log erreur
			return status;											   // Retourne erreur
		}
	}

	// 4. Connexion au réseau WiFi (STA uniquement)
	if (mode == ESP01_WIFI_MODE_STA) // Si le mode est STA (client WiFi)
	{
		_esp_logln("[WIFI] -> Connexion au réseau WiFi..."); // Log : début de la connexion WiFi
		status = esp01_connect_wifi(ssid, password);		 // Appelle la fonction de connexion WiFi
		if (status != ESP01_OK)								 // Si la connexion échoue
		{
			_esp_logln("[WIFI] !! ERREUR: Connexion WiFi (CWJAP)"); // Log : erreur de connexion WiFi
			return status;											// Retourne le code d'erreur
		}
		HAL_Delay(300); // Délai après la connexion pour stabiliser
	}

	// 5. Activation de l'affichage IP client dans +IPD
	_esp_logln("[WIFI] -> Activation de l'affichage IP client dans +IPD (AT+CIPDINFO=1)..."); // Log : activation de l'affichage IP client
	status = esp01_send_raw_command_dma("AT+CIPDINFO=1", resp, sizeof(resp), "OK", 2000);	  // Envoie la commande AT pour activer CIPDINFO
	_esp_logln(resp);																		  // Log : affiche la réponse reçue
	if (status != ESP01_OK)																	  // Si la commande échoue
	{
		_esp_logln("[WIFI] !! ERREUR: AT+CIPDINFO=1"); // Log : erreur d'activation CIPDINFO
		return status;								   // Retourne le code d'erreur
	}

	_esp_logln("[WIFI] === Configuration WiFi terminée ==="); // Log : fin de la configuration WiFi
	return ESP01_OK;										  // Retourne OK
}

/**
 * @brief Démarre le serveur web sur le port spécifié
 * @param multi_conn true pour multi-connexion, false pour mono
 * @param port Port du serveur web
 * @return ESP01_OK si succès, code d'erreur sinon
 */
ESP01_Status_t esp01_start_server_config(
	bool multi_conn, // true = multi-connexion (CIPMUX=1), false = mono (CIPMUX=0)
	uint16_t port)	 // Port du serveur web
{
	g_server_port = port;			  // Stocke le port du serveur web
	ESP01_Status_t status;			  // Variable pour le code de retour
	char resp[ESP01_DMA_RX_BUF_SIZE]; // Buffer pour la réponse AT
	char cmd[ESP01_DMA_RX_BUF_SIZE];  // Buffer pour la commande AT

	// 1. Configure le mode multi-connexion
	snprintf(cmd, sizeof(cmd), "AT+CIPMUX=%d", multi_conn ? 1 : 0);			  // Prépare la commande pour activer/désactiver le multi-connexion
	status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 3000); // Envoie la commande AT+CIPMUX
	if (status != ESP01_OK)													  // Si la commande échoue
	{
		_esp_logln("[WEB] ERREUR: AT+CIPMUX"); // Log : erreur AT+CIPMUX
		return status;						   // Retourne le code d'erreur
	}

	// 2. Démarre le serveur web sur le port demandé
	snprintf(cmd, sizeof(cmd), "AT+CIPSERVER=1,%u", port);					  // Prépare la commande pour démarrer le serveur web
	status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 5000); // Envoie la commande AT+CIPSERVER
	if (status != ESP01_OK && !strstr(resp, "no change"))					  // Si la commande échoue et que la réponse n'est pas "no change"
	{
		_esp_logln("[WEB] ERREUR: AT+CIPSERVER"); // Log : erreur AT+CIPSERVER
		return status;							  // Retourne le code d'erreur
	}

	_esp_logln("[WEB] Serveur web démarré avec succès"); // Log : serveur web démarré
	return ESP01_OK;									 // Retourne OK
}

/**
 * @brief Parse une requête HTTP (GET/POST) et remplit la structure correspondante
 * @param raw_request Chaîne contenant la requête HTTP brute
 * @param parsed Structure de sortie
 * @return ESP01_OK si succès, code d'erreur sinon
 */
ESP01_Status_t esp01_parse_http_request(const char *raw_request, http_parsed_request_t *parsed)
{
	VALIDATE_PARAM(raw_request, ESP01_FAIL); // Vérifie que la requête n'est pas NULL
	VALIDATE_PARAM(parsed, ESP01_FAIL);		 // Vérifie que la structure de sortie n'est pas NULL

	memset(parsed, 0, sizeof(http_parsed_request_t)); // Initialise la structure de sortie à zéro

	// Machine à états pour parser la première ligne HTTP
	const char *p = raw_request;					   // Pointeur de parcours de la requête
	const char *method_start = p, *method_end = NULL;  // Début et fin de la méthode HTTP
	const char *path_start = NULL, *path_end = NULL;   // Début et fin du chemin HTTP
	const char *query_start = NULL, *query_end = NULL; // Début et fin de la query string
	const char *line_end = strstr(p, "\r\n");		   // Cherche la fin de la première ligne
	if (!line_end)									   // Si pas de fin de ligne trouvée
		return ESP01_FAIL;							   // Retourne une erreur

	// État 1 : méthode
	while (p < line_end && *p != ' ')								 // Parcourt jusqu'à l'espace (fin de la méthode)
		p++;														 // Avance le pointeur
	method_end = p;													 // Marque la fin de la méthode
	if (method_end - method_start >= ESP01_MAX_HTTP_METHOD_LEN)		 // Vérifie la taille de la méthode
		return ESP01_FAIL;											 // Retourne une erreur si trop long
	memcpy(parsed->method, method_start, method_end - method_start); // Copie la méthode dans la structure
	parsed->method[method_end - method_start] = '\0';				 // Termine la chaîne

	// État 2 : chemin
	p++;													 // Passe l'espace
	path_start = p;											 // Début du chemin
	while (p < line_end && *p != ' ' && *p != '?')			 // Parcourt jusqu'à l'espace ou '?'
		p++;												 // Avance le pointeur
	path_end = p;											 // Fin du chemin
	if (path_end - path_start >= ESP01_MAX_HTTP_PATH_LEN)	 // Vérifie la taille du chemin
		return ESP01_FAIL;									 // Retourne une erreur si trop long
	memcpy(parsed->path, path_start, path_end - path_start); // Copie le chemin dans la structure
	parsed->path[path_end - path_start] = '\0';				 // Termine la chaîne

	// État 3 : query string (optionnel)
	if (*p == '?') // Si un '?' est présent (query string)
	{
		p++;											 // Passe le '?'
		query_start = p;								 // Début de la query string
		while (p < line_end && *p != ' ')				 // Parcourt jusqu'à l'espace
			p++;										 // Avance le pointeur
		query_end = p;									 // Fin de la query string
		size_t qlen = query_end - query_start;			 // Calcule la longueur de la query string
		if (qlen >= ESP01_MAX_HTTP_QUERY_LEN)			 // Vérifie la taille maximale
			qlen = ESP01_MAX_HTTP_QUERY_LEN - 1;		 // Tronque si nécessaire
		memcpy(parsed->query_string, query_start, qlen); // Copie la query string
		parsed->query_string[qlen] = '\0';				 // Termine la chaîne
	}
	else
	{
		parsed->query_string[0] = '\0'; // Pas de query string
	}

	parsed->is_valid = true; // Marque la structure comme valide
	return ESP01_OK;		 // Retourne OK
}

/**
 * @brief Envoie une réponse HTTP générique (code, type, body)
 * @param conn_id ID de la connexion
 * @param status_code Code HTTP (ex: 200, 404)
 * @param content_type Type MIME
 * @param body Corps de la réponse
 * @param body_len Taille du corps
 * @return ESP01_OK si succès, code d'erreur sinon
 */
ESP01_Status_t esp01_send_http_response(int conn_id, int status_code, const char *content_type,
										const char *body, size_t body_len)
{
	VALIDATE_PARAM(conn_id >= 0, ESP01_FAIL);							 // Vérifie que l'ID de connexion est valide
	VALIDATE_PARAM(status_code >= 100 && status_code < 600, ESP01_FAIL); // Vérifie que le code HTTP est valide
	VALIDATE_PARAM(body || body_len == 0, ESP01_FAIL);					 // Vérifie que le body est non NULL si body_len > 0

	uint32_t start = HAL_GetTick();				 // Sauvegarde le temps de début
	g_stats.total_requests++;					 // Incrémente le nombre total de requêtes
	g_stats.response_count++;					 // Incrémente le nombre de réponses
	if (status_code >= 200 && status_code < 300) // Si code 2xx
	{
		g_stats.successful_responses++; // Incrémente les réponses réussies
	}
	else if (status_code >= 400) // Si code 4xx ou 5xx
	{
		g_stats.failed_responses++; // Incrémente les réponses échouées
	}

	char header[256];				// Buffer pour l'entête HTTP
	const char *status_text = "OK"; // Texte du statut HTTP
	switch (status_code)			// Sélectionne le texte selon le code
	{
	case 200:
		status_text = "OK"; // 200 -> OK
		break;
	case 404:
		status_text = "Not Found"; // 404 -> Not Found
		break;
	case 500:
		status_text = "Internal Server Error"; // 500 -> Internal Server Error
		break;
	default:
		status_text = "Unknown"; // Autres -> Unknown
		break;
	}

	int header_len = snprintf(header, sizeof(header), // Génère l'entête HTTP
							  "HTTP/1.1 %d %s\r\n"
							  "Content-Type: %s\r\n"
							  "Content-Length: %d\r\n"
							  "Connection: close\r\n"
							  "\r\n",
							  status_code, status_text, content_type ? content_type : "text/html", (int)body_len);

	// Utilise un buffer fixe de 2048 octets pour la réponse HTTP
	char response[2048];							 // Buffer pour la réponse complète
	if ((header_len + body_len) >= sizeof(response)) // Vérifie la taille totale
	{
		_esp_logln("[HTTP] esp01_send_http_response: réponse trop grande"); // Log si trop grand
		return ESP01_FAIL;													// Retourne erreur
	}

	memcpy(response, header, header_len);			   // Copie l'entête dans le buffer
	if (body && body_len > 0)						   // Si body présent
		memcpy(response + header_len, body, body_len); // Copie le body après l'entête

	int total_len = header_len + (int)body_len; // Calcule la taille totale

	char cipsend_cmd[32];																// Buffer pour la commande AT+CIPSEND
	snprintf(cipsend_cmd, sizeof(cipsend_cmd), "AT+CIPSEND=%d,%d", conn_id, total_len); // Prépare la commande

	char resp[64];																							  // Buffer pour la réponse AT
	ESP01_Status_t st = esp01_send_raw_command_dma(cipsend_cmd, resp, sizeof(resp), ">", ESP01_TIMEOUT_LONG); // Envoie AT+CIPSEND
	if (st != ESP01_OK)																						  // Si échec
	{
		_esp_logln("[HTTP] esp01_send_http_response: AT+CIPSEND échoué"); // Log erreur
		return st;														  // Retourne le code d'erreur
	}

	HAL_UART_Transmit(g_esp_uart, (uint8_t *)response, total_len, HAL_MAX_DELAY); // Envoie la réponse HTTP

	st = esp01_wait_for_pattern("SEND OK", ESP01_TIMEOUT_LONG); // Attend la confirmation d'envoi
	char dbg[128];												// Buffer debug
	snprintf(dbg, sizeof(dbg), "[HTTP] Envoi réponse HTTP sur connexion %d, taille de la page HTML : %d octets", conn_id, (int)body_len);
	_esp_logln(dbg); // Log debug

	// À la toute fin, après l'envoi :
	uint32_t elapsed = HAL_GetTick() - start;																			   // Calcule le temps écoulé
	g_stats.total_response_time_ms += elapsed;																			   // Ajoute au temps total
	g_stats.avg_response_time_ms = g_stats.response_count ? (g_stats.total_response_time_ms / g_stats.response_count) : 0; // Met à jour la moyenne

	return st; // Retourne le statut
}

/**
 * @brief Envoie une réponse HTTP JSON (200 OK)
 * @param conn_id ID de la connexion
 * @param json_data Chaîne JSON à envoyer
 * @return ESP01_OK si succès, code d'erreur sinon
 */
ESP01_Status_t esp01_send_json_response(int conn_id, const char *json_data)
{
	_esp_logln("[HTTP] Envoi d'une réponse JSON");													 // Log debug
	return esp01_send_http_response(conn_id, 200, "application/json", json_data, strlen(json_data)); // Envoie la réponse JSON
}

/**
 * @brief Envoie une réponse HTTP 404 Not Found
 * @param conn_id ID de la connexion
 * @return ESP01_OK si succès, code d'erreur sinon
 */
ESP01_Status_t esp01_send_404_response(int conn_id)
{
	_esp_logln("[HTTP] Envoi d'une réponse 404");									// Log debug
	const char *body = "<html><body><h1>404 - Page Not Found</h1></body></html>";	// Corps HTML 404
	return esp01_send_http_response(conn_id, 404, "text/html", body, strlen(body)); // Envoie la réponse 404
}

/**
 * @brief Vérifie si l'ESP01 est connecté au WiFi
 * @return ESP01_OK si connecté, ESP01_FAIL sinon
 */
ESP01_Status_t esp01_get_connection_status(void)
{
	_esp_logln("[STATUS] Vérification du statut de connexion ESP01");															// Log debug
	char response[512];																											// Buffer pour la réponse AT
	ESP01_Status_t status = esp01_send_raw_command_dma("AT+CIPSTATUS", response, sizeof(response), "OK", ESP01_TIMEOUT_MEDIUM); // Envoie AT+CIPSTATUS

	if (status == ESP01_OK) // Si succès
	{
		if (strstr(response, "STATUS:2") || strstr(response, "STATUS:3")) // Si status connecté
		{
			_esp_logln("[STATUS] ESP01 connecté au WiFi"); // Log connecté
			return ESP01_OK;							   // Retourne OK
		}
	}

	_esp_logln("[STATUS] ESP01 non connecté au WiFi"); // Log non connecté
	return ESP01_FAIL;								   // Retourne FAIL
}
/**
 * @brief Arrête le serveur web (ferme le port TCP)
 * @return ESP01_OK si succès, code d'erreur sinon
 */
ESP01_Status_t esp01_stop_web_server(void)
{
	_esp_logln("[STATUS] Arrêt du serveur web ESP01");																			  // Log arrêt serveur
	char response[256];																											  // Buffer pour la réponse AT
	ESP01_Status_t status = esp01_send_raw_command_dma("AT+CIPSERVER=0", response, sizeof(response), "OK", ESP01_TIMEOUT_MEDIUM); // Envoie la commande AT pour arrêter le serveur
	return status;																												  // Retourne le statut
}

/**
 * @brief Affiche le statut de l'ESP01 sur un terminal UART
 * @return ESP01_OK si succès, code d'erreur sinon
 */
ESP01_Status_t esp01_print_connection_status(void)
{
	if (!g_debug_uart) // Vérifie que l'UART debug est initialisée
	{
		_esp_logln("[STATUS] esp01_print_connection_status: g_debug_uart NULL"); // Log erreur
		return ESP01_NOT_INITIALIZED;											 // Retourne erreur
	}

	const char *header = "\r\n=== STATUS ESP01 ===\r\n";							   // Entête pour affichage
	HAL_UART_Transmit(g_debug_uart, (uint8_t *)header, strlen(header), HAL_MAX_DELAY); // Affiche l'entête

	char response[256];																				  // Buffer pour la réponse AT
	ESP01_Status_t status = esp01_send_raw_command_dma("AT", response, sizeof(response), "OK", 2000); // Test AT

	char msg[128];																		 // Buffer pour messages
	snprintf(msg, sizeof(msg), "Test AT: %s\r\n", (status == ESP01_OK) ? "OK" : "FAIL"); // Prépare le message de test AT
	HAL_UART_Transmit(g_debug_uart, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);		 // Affiche le résultat du test AT

	status = esp01_send_raw_command_dma("AT+CWJAP?", response, sizeof(response), "OK", 3000); // Demande le statut WiFi
	if (status == ESP01_OK)																	  // Si succès
	{
		if (strstr(response, "No AP")) // Si non connecté
		{
			HAL_UART_Transmit(g_debug_uart, (uint8_t *)"WiFi: Non connecté\r\n", 20, HAL_MAX_DELAY); // Affiche non connecté
		}
		else
		{
			HAL_UART_Transmit(g_debug_uart, (uint8_t *)"WiFi: Connecté\r\n", 16, HAL_MAX_DELAY); // Affiche connecté
		}
	}
	else
	{
		HAL_UART_Transmit(g_debug_uart, (uint8_t *)"WiFi: Status inconnu\r\n", 22, HAL_MAX_DELAY); // Affiche statut inconnu
	}

	char ip[32];										  // Buffer pour l'IP
	if (esp01_get_current_ip(ip, sizeof(ip)) == ESP01_OK) // Récupère l'IP courante
	{
		snprintf(msg, sizeof(msg), "IP: %s\r\n", ip);								 // Prépare le message IP
		HAL_UART_Transmit(g_debug_uart, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY); // Affiche l'IP
	}

	snprintf(msg, sizeof(msg), "Routes définies: %d/%d\r\n", g_route_count, ESP01_MAX_ROUTES); // Prépare le message routes
	HAL_UART_Transmit(g_debug_uart, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);			   // Affiche le nombre de routes

	const char *footer = "==================\r\n";									   // Pied de page
	HAL_UART_Transmit(g_debug_uart, (uint8_t *)footer, strlen(footer), HAL_MAX_DELAY); // Affiche le pied de page
	_esp_logln("[STATUS] Statut ESP01 affiché sur terminal");						   // Log affichage terminé
	return ESP01_OK;																   // Retourne OK
}

/**
 * @brief Traite les requêtes HTTP reçues (dispatcher)
 */
void esp01_process_requests(void)
{
	if (g_processing_request)	 // Vérifie si déjà en traitement
		return;					 // Sort si déjà en cours
	g_processing_request = true; // Marque comme en traitement

	uint8_t buffer[ESP01_DMA_RX_BUF_SIZE];					   // Buffer pour les nouveaux octets reçus
	uint16_t len = esp01_get_new_data(buffer, sizeof(buffer)); // Récupère les nouveaux octets
	if (len == 0)											   // Si rien à lire
	{
		g_processing_request = false; // Libère le flag
		return;						  // Sort
	}

	// Ajoute les nouveaux octets à l'accumulateur global
	if (g_acc_len + len < sizeof(g_accumulator) - 1) // Si l'accumulateur n'est pas plein
	{
		memcpy(g_accumulator + g_acc_len, buffer, len); // Ajoute les octets reçus
		g_acc_len += len;								// Met à jour la longueur
		g_accumulator[g_acc_len] = '\0';				// Termine la chaîne
	}
	else
	{
		// Débordement : reset complet
		g_acc_len = 0;							   // Réinitialise la longueur
		g_accumulator[0] = '\0';				   // Vide l'accumulateur
		g_parse_state = PARSE_STATE_SEARCHING_IPD; // Réinitialise l'état de parsing
		g_processing_request = false;			   // Libère le flag
		return;									   // Sort
	}

	bool keep_processing = true; // Flag pour la boucle de traitement
	while (keep_processing)		 // Boucle de traitement
	{
		switch (g_parse_state) // Selon l'état de parsing
		{
		case PARSE_STATE_SEARCHING_IPD: // Recherche d'un +IPD
		{
			char *ipd_start = _find_next_ipd(g_accumulator, g_acc_len); // Cherche le prochain +IPD
			if (!ipd_start)												// Si non trouvé
			{
				keep_processing = false; // Arrête le traitement
				break;					 // Sort du switch
			}
			if (ipd_start != g_accumulator) // Si +IPD n'est pas au début
			{
				int shift = ipd_start - g_accumulator;				  // Calcule le décalage
				memmove(g_accumulator, ipd_start, g_acc_len - shift); // Décale l'accumulateur
				g_acc_len -= shift;									  // Met à jour la longueur
				g_accumulator[g_acc_len] = '\0';					  // Termine la chaîne
			}
			g_parse_state = PARSE_STATE_READING_HEADER; // Passe à la lecture des headers
														// Pas de break, on enchaîne
		}
		case PARSE_STATE_READING_HEADER: // Lecture des headers HTTP
		{
			char *headers_end = strstr(g_accumulator, "\r\n\r\n"); // Cherche la fin des headers
			if (!headers_end)									   // Si non trouvée
			{
				keep_processing = false; // Arrête le traitement
				break;					 // Sort du switch
			}
			http_request_t req = parse_ipd_header(g_accumulator); // Parse le header IPD
			if (!req.is_valid)									  // Si IPD non valide
			{
				// IPD non valide, resynchronisation
				char *next_ipd = _find_next_ipd(g_accumulator + IPD_HEADER_MIN_LEN, g_acc_len - IPD_HEADER_MIN_LEN); // Cherche le prochain +IPD
				if (next_ipd)																						 // Si trouvé
				{
					int shift = next_ipd - g_accumulator;				 // Calcule le décalage
					memmove(g_accumulator, next_ipd, g_acc_len - shift); // Décale l'accumulateur
					g_acc_len -= shift;									 // Met à jour la longueur
					g_accumulator[g_acc_len] = '\0';					 // Termine la chaîne
				}
				else
				{
					g_acc_len = 0;			 // Réinitialise la longueur
					g_accumulator[0] = '\0'; // Vide l'accumulateur
				}
				g_parse_state = PARSE_STATE_SEARCHING_IPD; // Retourne à la recherche d'un +IPD
				keep_processing = false;				   // Arrête le traitement
				break;									   // Sort du switch
			}
			g_parse_state = PARSE_STATE_READING_PAYLOAD; // Passe à la lecture du payload
														 // Pas de break, on enchaîne
		}
		case PARSE_STATE_READING_PAYLOAD: // Lecture du payload HTTP
		{
			http_request_t req = parse_ipd_header(g_accumulator); // Parse le header IPD
			int ipd_payload_len = req.content_length;			  // Récupère la taille du payload
			char *colon_pos = strchr(g_accumulator, ':');		  // Cherche le ':'
			if (!colon_pos)										  // Si non trouvé
			{
				g_parse_state = PARSE_STATE_SEARCHING_IPD; // Retourne à la recherche d'un +IPD
				keep_processing = false;				   // Arrête le traitement
				break;									   // Sort du switch
			}
			colon_pos++;													// Passe après le ':'
			int received_payload = g_acc_len - (colon_pos - g_accumulator); // Calcule la taille du payload reçu
			if (received_payload < ipd_payload_len)							// Si payload incomplet
			{
				// Attendre plus de données
				keep_processing = false; // Arrête le traitement
				break;					 // Sort du switch
			}

			// === AJOUT POUR COPIER L'IP DU CLIENT DANS g_connections ===
			int idx = -1; // Index de la connexion
			// Cherche une connexion existante
			for (int i = 0; i < g_connection_count; ++i) // Parcourt les connexions existantes
			{
				if (g_connections[i].conn_id == req.conn_id) // Si l'ID correspond
				{
					idx = i; // Sauvegarde l'index
					break;	 // Sort de la boucle
				}
			}
			// Sinon, cherche un slot inactif à réutiliser
			if (idx == -1) // Si non trouvé
			{
				for (int i = 0; i < g_connection_count; ++i) // Parcourt les connexions
				{
					if (!g_connections[i].is_active) // Si slot inactif
					{
						idx = i;								  // Sauvegarde l'index
						g_connections[idx].conn_id = req.conn_id; // Associe l'ID
						break;									  // Sort de la boucle
					}
				}
			}
			// Sinon, ajoute un nouveau slot si possible
			if (idx == -1 && g_connection_count < ESP01_MAX_CONNECTIONS) // Si pas trouvé et place dispo
			{
				idx = g_connection_count++;														 // Nouvel index
				g_connections[idx].conn_id = req.conn_id;										 // Associe l'ID
				char dbg[64];																	 // Buffer debug
				snprintf(dbg, sizeof(dbg), "[DEBUG] Nouvelle connexion TCP id=%d", req.conn_id); // Message debug
				_esp_logln(dbg);																 // Log debug
			}
			if (idx != -1) // Si un slot a été trouvé
			{
				g_connections[idx].last_activity = HAL_GetTick(); // Met à jour l'activité
				g_connections[idx].is_active = true;			  // Marque comme actif
				if (req.has_ip)									  // Si l'IP est connue
				{
					strncpy(g_connections[idx].client_ip, req.client_ip, ESP01_MAX_IP_LEN);		  // Copie l'IP client
					char dbg[64];																  // Buffer debug
					snprintf(dbg, sizeof(dbg), "[DEBUG] IP client détectée : %s", req.client_ip); // Message debug
					_esp_logln(dbg);															  // Log debug
				}
				else
				{
					strcpy(g_connections[idx].client_ip, "N/A");			   // Marque IP inconnue
					_esp_logln("[DEBUG] IP client non transmise par l'ESP01"); // Log debug
				}
				g_connections[idx].server_port = req.client_port > 0 ? req.client_port : 80;			// Met à jour le port
				char dbg[64];																			// Buffer debug
				snprintf(dbg, sizeof(dbg), "[DEBUG] Port client : %u", g_connections[idx].server_port); // Message debug
				_esp_logln(dbg);																		// Log debug
			}
			// === FIN AJOUT DEBUG ===

			// Parse la requête HTTP
			http_parsed_request_t parsed = {0};												 // Structure pour la requête parsée
			if (esp01_parse_http_request(colon_pos, &parsed) == ESP01_OK && parsed.is_valid) // Si parsing OK
			{
				// --- Ajout : Parsing des headers HTTP ---
				const char *first_line_end = strstr(colon_pos, "\r\n"); // Cherche la fin de la première ligne
				if (first_line_end)										// Si trouvée
				{
					const char *headers_start = first_line_end + 2;				 // Début des headers
					const char *headers_end = strstr(headers_start, "\r\n\r\n"); // Fin des headers
					if (headers_end)											 // Si trouvée
					{
						size_t headers_len = headers_end - headers_start; // Longueur des headers
						if (headers_len < sizeof(parsed.headers_buf))	  // Si la taille est correcte
						{
							memcpy(parsed.headers_buf, headers_start, headers_len); // Copie les headers
							parsed.headers_buf[headers_len] = '\0';					// Termine la chaîne
						}
						else
						{
							parsed.headers_buf[0] = '\0'; // Vide si trop long
						}
					}
					else
					{
						parsed.headers_buf[0] = '\0'; // Vide si non trouvé
					}
				}
				else
				{
					parsed.headers_buf[0] = '\0'; // Vide si non trouvé
				}
				// --- Fin ajout ---

				esp01_route_handler_t handler = esp01_find_route_handler(parsed.path); // Cherche le handler pour la route
				if (handler)														   // Si trouvé
				{
					handler(req.conn_id, &parsed); // Appelle le handler
				}
				else
				{
					// Réponse 204 No Content pour /favicon.ico
					if (strcmp(parsed.path, "/favicon.ico") == 0) // Si favicon
					{
						esp01_send_http_response(req.conn_id, 204, "image/x-icon", NULL, 0); // Réponse 204
					}
					else
					{
						esp01_send_404_response(req.conn_id); // Réponse 404
					}
				}
			}

			// Gère le body si nécessaire (POST/PUT)
			char *headers_end = strstr(colon_pos, "\r\n\r\n");											   // Cherche la fin des headers
			int headers_len = headers_end ? (headers_end + 4 - colon_pos) : 0;							   // Calcule la longueur des headers
			int body_len = ipd_payload_len - headers_len;												   // Calcule la longueur du body
			if ((strcmp(parsed.method, "POST") == 0 || strcmp(parsed.method, "PUT") == 0) && body_len > 0) // Si POST/PUT avec body
			{
				discard_http_payload(body_len); // Ignore le body
			}

			// Retire la trame traitée de l'accumulateur
			int total_to_remove = (colon_pos - g_accumulator) + ipd_payload_len; // Calcule la taille à retirer
			if (g_acc_len > total_to_remove)									 // Si d'autres données restent
			{
				memmove(g_accumulator, g_accumulator + total_to_remove, g_acc_len - total_to_remove); // Décale l'accumulateur
				g_acc_len -= total_to_remove;														  // Met à jour la longueur
				g_accumulator[g_acc_len] = '\0';													  // Termine la chaîne
				g_parse_state = PARSE_STATE_SEARCHING_IPD;											  // Retourne à la recherche d'un +IPD
																									  // On boucle pour traiter la prochaine trame si présente
			}
			else
			{
				g_acc_len = 0;							   // Réinitialise la longueur
				g_accumulator[0] = '\0';				   // Vide l'accumulateur
				g_parse_state = PARSE_STATE_SEARCHING_IPD; // Retourne à la recherche d'un +IPD
				keep_processing = false;				   // Arrête le traitement
			}
			break; // Sort du switch
		}
		default:					 // Cas par défaut
			keep_processing = false; // Arrête le traitement
			break;					 // Sort du switch
		}
	}

	g_processing_request = false; // Libère le flag de traitement
}

// ==================== ROUTAGE HTTP ====================

/**
 * @brief Efface toutes les routes HTTP
 */
void esp01_clear_routes(void)
{
	_esp_logln("[ROUTE] Effacement de toutes les routes HTTP"); // Log effacement des routes
	g_route_count = 0;											// Réinitialise le compteur de routes
}

/**
 * @brief Ajoute une route HTTP (chemin + handler)
 * @param path Chemin HTTP
 * @param handler Fonction handler associée
 * @return ESP01_OK si succès, code d'erreur sinon
 */
ESP01_Status_t esp01_add_route(const char *path, esp01_route_handler_t handler)
{
	VALIDATE_PARAM(path, ESP01_FAIL);							  // Vérifie que le chemin n'est pas NULL
	VALIDATE_PARAM(handler, ESP01_FAIL);						  // Vérifie que le handler n'est pas NULL
	VALIDATE_PARAM(g_route_count < ESP01_MAX_ROUTES, ESP01_FAIL); // Vérifie qu'il reste de la place pour une nouvelle route

	strncpy(g_routes[g_route_count].path, path, sizeof(g_routes[g_route_count].path) - 1);	// Copie le chemin dans la structure de route
	g_routes[g_route_count].path[sizeof(g_routes[g_route_count].path) - 1] = '\0';			// Termine la chaîne par '\0'
	g_routes[g_route_count].handler = handler;												// Associe le handler à la route
	g_route_count++;																		// Incrémente le compteur de routes
	char dbg[80];																			// Buffer pour message de debug
	snprintf(dbg, sizeof(dbg), "[WEB] Route ajoutée : %s (total %d)", path, g_route_count); // Prépare le message de debug
	_esp_logln(dbg);																		// Affiche le message de debug
	return ESP01_OK;																		// Retourne OK
}

/**
 * @brief Recherche le handler associé à un chemin donné
 * @param path Chemin HTTP
 * @return Pointeur sur le handler, ou NULL si non trouvé
 */
esp01_route_handler_t esp01_find_route_handler(const char *path)
{
	for (int i = 0; i < g_route_count; i++) // Parcourt toutes les routes enregistrées
	{
		if (strcmp(g_routes[i].path, path) == 0) // Compare le chemin avec celui recherché
		{
			char dbg[80];															// Buffer pour message de debug
			snprintf(dbg, sizeof(dbg), "[WEB] Route trouvée pour path : %s", path); // Prépare le message de debug
			_esp_logln(dbg);														// Affiche le message de debug
			return g_routes[i].handler;												// Retourne le handler trouvé
		}
	}
	char dbg[80];																   // Buffer pour message de debug
	snprintf(dbg, sizeof(dbg), "[WEB] Aucune route trouvée pour path : %s", path); // Prépare le message de debug
	_esp_logln(dbg);															   // Affiche le message de debug
	return NULL;																   // Retourne NULL si non trouvé
}

/**
 * @brief Teste la communication AT avec l'ESP01
 * @param dma_rx_buf Buffer DMA RX
 * @param dma_buf_size Taille du buffer DMA RX
 * @return ESP01_OK si succès, code d'erreur sinon
 */
ESP01_Status_t esp01_test_at(uint8_t *dma_rx_buf, uint16_t dma_buf_size)
{
	char response[256];																				  // Buffer pour la réponse AT
	ESP01_Status_t status = esp01_send_raw_command_dma("AT", response, sizeof(response), "OK", 2000); // Envoie la commande AT et attend "OK"
	return status;																					  // Retourne le statut
}

/**
 * @brief Récupère l'adresse IP courante de l'ESP01
 * @param ip_buffer Buffer de sortie
 * @param buffer_size Taille du buffer
 * @return ESP01_OK si succès, code d'erreur sinon
 */
ESP01_Status_t esp01_get_current_ip(char *ip_buffer, size_t buffer_size)
{
	VALIDATE_PARAM(ip_buffer && buffer_size > 0, ESP01_FAIL); // Vérifie les paramètres

	char response[512];																									  // Buffer pour la réponse AT
	ESP01_Status_t status = esp01_send_raw_command_dma("AT+CIFSR", response, sizeof(response), "OK", ESP01_TIMEOUT_LONG); // Envoie la commande AT+CIFSR

	if (status != ESP01_OK) // Si la commande a échoué
		return ESP01_FAIL;	// Retourne FAIL

	// Recherche STAIP (mode STA), accepte aussi +CIFSR:STAIP,"..."
	char *ip_line = strstr(response, "STAIP,\"");	   // Cherche "STAIP,\""
	if (!ip_line)									   // Si non trouvé
		ip_line = strstr(response, "+CIFSR:STAIP,\""); // Cherche "+CIFSR:STAIP,\""
	if (!ip_line)									   // Si non trouvé
		ip_line = strstr(response, "APIP,\"");		   // Cherche "APIP,\""
	if (!ip_line)									   // Si non trouvé
		ip_line = strstr(response, "+CIFSR:APIP,\"");  // Cherche "+CIFSR:APIP,\""

	if (ip_line) // Si une ligne IP a été trouvée
	{
		ip_line = strchr(ip_line, '"'); // Cherche le premier guillemet
		if (ip_line)					// Si trouvé
		{
			ip_line++;								// Passe le premier guillemet
			char *end_quote = strchr(ip_line, '"'); // Cherche le guillemet de fin
			if (end_quote)							// Si trouvé
			{
				size_t ip_len = end_quote - ip_line; // Calcule la longueur de l'IP
				if (ip_len < buffer_size)			 // Si la taille est correcte
				{
					strncpy(ip_buffer, ip_line, ip_len); // Copie l'IP dans le buffer
					ip_buffer[ip_len] = '\0';			 // Termine la chaîne
					return ESP01_OK;					 // Retourne OK
				}
			}
		}
	}

	_esp_logln("[IP] Adresse IP non trouvée dans la réponse ESP01"); // Log si IP non trouvée
	if (buffer_size > 0)											 // Si le buffer est valide
		ip_buffer[0] = '\0';										 // Met une chaîne vide

	return ESP01_FAIL; // Retourne FAIL
}

/**
 * @brief Envoie une requête HTTP GET et récupère la réponse
 * @param host Hôte distant
 * @param port Port distant
 * @param path Chemin HTTP
 * @param response Buffer pour la réponse
 * @param response_size Taille du buffer réponse
 * @return ESP01_OK si succès, code d'erreur sinon
 */
ESP01_Status_t esp01_http_get(const char *host, uint16_t port, const char *path, char *response, size_t response_size)
{
	char dbg[128];																		// Buffer pour message de debug
	snprintf(dbg, sizeof(dbg), "esp01_http_get: GET http://%s:%u%s", host, port, path); // Prépare le message de debug
	_esp_logln(dbg);																	// Affiche le message de debug

	char cmd[ESP01_DMA_RX_BUF_SIZE];												 // Buffer pour la commande AT
	snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d", host, port);		 // Prépare la commande AT+CIPSTART
	char resp[ESP01_DMA_RX_BUF_SIZE];												 // Buffer pour la réponse AT
	if (esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 5000) != ESP01_OK) // Envoie la commande et vérifie le succès
	{
		_esp_logln("[HTTP] esp01_http_get: AT+CIPSTART échoué"); // Log si échec
		return ESP01_FAIL;										 // Retourne FAIL
	}

	char http_req[256]; // Buffer pour la requête HTTP GET
	snprintf(http_req, sizeof(http_req),
			 "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host); // Prépare la requête GET

	snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d", (int)strlen(http_req));				// Prépare la commande AT+CIPSEND
	if (esp01_send_raw_command_dma(cmd, resp, sizeof(resp), ">", 3000) != ESP01_OK) // Envoie la commande et attend le prompt '>'
		return ESP01_FAIL;															// Retourne FAIL

	if (esp01_send_raw_command_dma(http_req, resp, sizeof(resp), "CLOSED", 8000) != ESP01_OK) // Envoie la requête HTTP et attend "CLOSED"
		return ESP01_FAIL;																	  // Retourne FAIL
	// Copie la réponse dans le buffer utilisateur
	if (response && response_size > 0) // Si le buffer de réponse est valide
	{
		strncpy(response, resp, response_size - 1); // Copie la réponse
		response[response_size - 1] = '\0';			// Termine la chaîne
	}

	return ESP01_OK; // Retourne OK
}

/**
 * @brief Retourne une chaîne descriptive pour un code d'erreur ESP01_Status_t
 * @param status Code d'erreur
 * @return Chaîne descriptive
 */
const char *esp01_get_error_string(ESP01_Status_t status)
{
	switch (status) // Sélectionne selon le code d'erreur
	{
	case ESP01_OK:
		return "OK"; // OK
	case ESP01_FAIL:
		return "Echec général"; // Échec général
	case ESP01_TIMEOUT:
		return "Timeout"; // Timeout
	case ESP01_NOT_INITIALIZED:
		return "Non initialisé"; // Non initialisé
	case ESP01_INVALID_PARAM:
		return "Paramètre invalide"; // Paramètre invalide
	case ESP01_BUFFER_OVERFLOW:
		return "Débordement de buffer"; // Débordement de buffer
	case ESP01_WIFI_NOT_CONNECTED:
		return "WiFi non connecté"; // WiFi non connecté
	case ESP01_HTTP_PARSE_ERROR:
		return "Erreur parsing HTTP"; // Erreur parsing HTTP
	case ESP01_ROUTE_NOT_FOUND:
		return "Route non trouvée"; // Route non trouvée
	case ESP01_CONNECTION_ERROR:
		return "Erreur de connexion"; // Erreur de connexion
	case ESP01_MEMORY_ERROR:
		return "Erreur mémoire"; // Erreur mémoire
	default:
		return "Code d'erreur inconnu"; // Code d'erreur inconnu
	}
}

/**
 * @brief Récupère la version AT du firmware ESP01
 * @param version_buf Buffer de sortie
 * @param buf_size Taille du buffer
 * @return ESP01_OK si succès, code d'erreur sinon
 */
ESP01_Status_t esp01_get_at_version(char *version_buf, size_t buf_size)
{
	if (!version_buf || buf_size == 0) // Vérifie les paramètres
		return ESP01_FAIL;			   // Retourne FAIL

	char response[ESP01_DMA_RX_BUF_SIZE] = {0};																			 // Buffer pour la réponse AT
	ESP01_Status_t status = esp01_send_raw_command_dma("AT+GMR", response, sizeof(response), "OK", ESP01_TIMEOUT_SHORT); // Envoie la commande AT+GMR

	if (status != ESP01_OK) // Si la commande a échoué
	{
		version_buf[0] = '\0'; // Met une chaîne vide
		return ESP01_FAIL;	   // Retourne FAIL
	}

	// Cherche la première ligne contenant "AT version:" ou "SDK version:"
	char *start = strstr(response, "AT version:"); // Cherche "AT version:"
	if (!start)									   // Si non trouvé
		start = strstr(response, "SDK version:");  // Cherche "SDK version:"
	if (!start)									   // Si toujours pas trouvé
		start = response;						   // Prend le début de la réponse

	// Copie la première ligne trouvée
	char *end = strchr(start, '\r');						  // Cherche '\r'
	if (!end)												  // Si non trouvé
		end = strchr(start, '\n');							  // Cherche '\n'
	size_t len = end ? (size_t)(end - start) : strlen(start); // Calcule la longueur de la ligne
	if (len >= buf_size)									  // Si trop long
		len = buf_size - 1;									  // Tronque
	strncpy(version_buf, start, len);						  // Copie la version
	version_buf[len] = '\0';								  // Termine la chaîne

	return ESP01_OK; // Retourne OK
}

/**
 * @brief Parse les headers HTTP (après la première ligne), callback appelé pour chaque header trouvé
 * @param headers_start Début des headers
 * @param on_header Callback appelé pour chaque header
 * @param user Pointeur utilisateur transmis au callback
 * @return ESP01_OK si succès
 */
ESP01_Status_t parse_http_headers(const char *headers_start, void (*on_header)(http_header_kv_t *header, void *user), void *user)
{
	const char *p = headers_start;				  // Pointeur de parcours
	while (*p && !(p[0] == '\r' && p[1] == '\n')) // Tant qu'on n'est pas à la fin des headers
	{
		// Trouve la fin de la ligne
		const char *line_end = strstr(p, "\r\n"); // Cherche la fin de la ligne
		if (!line_end)							  // Si non trouvée
			break;								  // Sort de la boucle

		// Trouve le séparateur ':'
		const char *colon = strchr(p, ':'); // Cherche ':'
		if (colon && colon < line_end)		// Si ':' trouvé avant la fin de ligne
		{
			// Ignore les espaces après ':'
			const char *val_start = colon + 1;				  // Début de la valeur
			while (*val_start == ' ' && val_start < line_end) // Ignore les espaces
				val_start++;								  // Avance

			http_header_kv_t header;				 // Structure pour le header
			header.key = p;							 // Pointeur sur la clé
			header.key_len = colon - p;				 // Longueur de la clé
			header.value = val_start;				 // Pointeur sur la valeur
			header.value_len = line_end - val_start; // Longueur de la valeur

			if (on_header)				  // Si callback défini
				on_header(&header, user); // Appelle le callback
		}
		p = line_end + 2; // Passe à la ligne suivante
	}
	return ESP01_OK; // Retourne OK
}

/**
 * @brief Ferme automatiquement les connexions inactives depuis plus de 30s
 */
void esp01_cleanup_inactive_connections(void)
{
	uint32_t now = HAL_GetTick();				 // Récupère le temps actuel
	for (int i = 0; i < g_connection_count; ++i) // Parcourt toutes les connexions
	{
		if (g_connections[i].is_active && (now - g_connections[i].last_activity > ESP01_CONN_TIMEOUT_MS)) // Si la connexion est inactive trop longtemps
		{
			// Ferme la connexion côté ESP01
			char cmd[32];															// Buffer pour la commande AT
			snprintf(cmd, sizeof(cmd), "AT+CIPCLOSE=%d", g_connections[i].conn_id); // Prépare la commande AT+CIPCLOSE
			char resp[64];															// Buffer pour la réponse AT
			esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", 2000);		// Envoie la commande AT+CIPCLOSE

			g_connections[i].is_active = false;					   // Marque la connexion comme inactive
			g_connections[i].conn_id = -1;						   // Réinitialise l'ID de connexion
			g_connections[i].client_ip[0] = '\0';				   // Vide l'IP client
			g_connections[i].server_port = 0;					   // Réinitialise le port serveur
			g_connections[i].client_port = 0;					   // Réinitialise le port client
			_esp_logln("[CONN] Connexion fermée pour inactivité"); // Log fermeture connexion
		}
	}
}

/**
 * @brief Retourne le nombre de connexions actives
 * @return Nombre de connexions actives
 */
int esp01_get_active_connection_count(void)
{
	int count = 0;								 // Compteur de connexions actives
	for (int i = 0; i < g_connection_count; ++i) // Parcourt toutes les connexions
	{
		if (g_connections[i].is_active) // Si la connexion est active
			count++;					// Incrémente le compteur
	}
	return count; // Retourne le nombre de connexions actives
}
