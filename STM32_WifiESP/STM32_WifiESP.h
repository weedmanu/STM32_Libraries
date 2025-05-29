#ifndef INC_STM32_WIFIESP_H_
#define INC_STM32_WIFIESP_H_

// ==================== Includes ====================
// Pour éviter les conflits de noms, on inclut main.h en premier
#include "main.h"
// Si stdio.h n'est pas déjà inclus, on l'inclut ici
#ifndef __STDIO_H
#define __STDIO_H
#include <stdio.h> // Pour printf et autres fonctions de sortie standard
#endif
// Si stdint.h n'est pas déjà inclus, on l'inclut ici
#ifndef __STDINT_H
#define __STDINT_H
#include <stdint.h> // Pour les types d'entiers standardisés (uint8_t, int32_t, etc.)
#endif
// Si stdbool.h n'est pas déjà inclus, on l'inclut ici
#ifndef __STDBOOL_H
#define __STDBOOL_H
#include <stdbool.h> // Pour les types booléens (true/false)
#endif
// Si stdlib.h n'est pas déjà inclus, on l'inclut ici
#ifndef __STDLIB_H
#define __STDLIB_H
#include <stdlib.h> // Pour les fonctions utilitaires comme malloc, free, etc.
#endif
// Si string.h n'est pas déjà inclus, on l'inclut ici
#ifndef __STRING_H
#define __STRING_H
#include <string.h> // Pour les fonctions de manipulation de chaînes
#endif

// ==================== Macros de configuration ====================

// Active/désactive les logs de debug sur l'UART de debug
#define ESP01_DEBUG 1

// Taille du buffer DMA pour la réception UART de l'ESP01
#define ESP01_DMA_RX_BUF_SIZE 512
// Nombre maximum de routes HTTP gérées par le serveur
#define ESP01_MAX_ROUTES 8
// Nombre maximum de connexions TCP simultanées
#define ESP01_MAX_CONNECTIONS 5

// Tailles de buffers et chaînes utilisées dans le driver
#define ESP01_FLUSH_TIMEOUT_MS 300	 // Timeout pour le flush du buffer RX (ms)
#define ESP01_SMALL_BUF_SIZE 128	 // Taille standard pour petits buffers temporaires
#define ESP01_DMA_TEST_BUF_SIZE 64	 // Taille pour les tests DMA
#define ESP01_SHORT_DELAY_MS 10		 // Délai court (ms)
#define ESP01_MAX_LOG_MSG 512		 // Taille max pour un message de log
#define ESP01_MAX_WARN_MSG 100		 // Taille max pour un message d'avertissement
#define ESP01_MAX_HEADER_LINE 256	 // Taille max pour une ligne d'en-tête HTTP
#define ESP01_MAX_TOTAL_HTTP 2048	 // Taille max pour une requête HTTP complète
#define ESP01_MAX_CIPSEND_CMD 64	 // Taille max pour une commande AT+CIPSEND
#define ESP01_MAX_IP_LEN 32			 // Taille max pour une adresse IP
#define ESP01_MAX_HTTP_METHOD_LEN 8	 // Taille max pour la méthode HTTP (GET, POST, etc.)
#define ESP01_MAX_HTTP_PATH_LEN 64	 // Taille max pour le chemin HTTP (/status, etc.)
#define ESP01_MAX_HTTP_QUERY_LEN 128 // Taille max pour la chaîne de requête (?a=1&b=2)

// Divers
#define IPD_HEADER_MIN_LEN 5 // Longueur minimale d'un header IPD

// ==================== Timeouts (en ms) ====================

#define ESP01_TIMEOUT_SHORT 1000  // Timeout court générique
#define ESP01_TIMEOUT_MEDIUM 3000 // Timeout moyen
#define ESP01_TIMEOUT_LONG 5000	  // Timeout long
#define ESP01_TIMEOUT_WIFI 15000  // Timeout pour la connexion WiFi

// ==================== Types et structures ====================

// --- Statuts de retour ---
// Codes de retour pour les fonctions du driver ESP01
typedef enum
{
	ESP01_OK = 0,				   // Succès
	ESP01_FAIL = -1,			   // Échec général
	ESP01_TIMEOUT = -2,			   // Opération expirée (timeout)
	ESP01_NOT_INITIALIZED = -3,	   // Driver non initialisé
	ESP01_INVALID_PARAM = -4,	   // Paramètre invalide
	ESP01_BUFFER_OVERFLOW = -5,	   // Débordement de buffer
	ESP01_WIFI_NOT_CONNECTED = -6, // Non connecté au WiFi
	ESP01_HTTP_PARSE_ERROR = -7,   // Erreur de parsing HTTP
	ESP01_ROUTE_NOT_FOUND = -8,	   // Route HTTP non trouvée
	ESP01_CONNECTION_ERROR = -9,   // Erreur de connexion TCP
	ESP01_MEMORY_ERROR = -10	   // Erreur d'allocation mémoire
} ESP01_Status_t;

// --- Modes WiFi ---
// Modes de fonctionnement du module ESP01
typedef enum
{
	ESP01_WIFI_MODE_STA = 1, // Station (client)
	ESP01_WIFI_MODE_AP = 2	 // Point d'accès
							 // ESP01_WIFI_MODE_STA_AP = 3 // SUPPRIMÉ : mode mixte non supporté
} ESP01_WifiMode_t;

// --- Statistiques ---
// Structure pour stocker les statistiques du serveur web
typedef struct
{
	uint32_t total_requests;		 // Nombre total de requêtes HTTP utiles reçues
	uint32_t successful_responses;	 // Nombre de réponses envoyées avec succès
	uint32_t failed_responses;		 // Nombre de réponses échouées (erreur d'envoi)
	uint32_t parse_errors;			 // Nombre d'erreurs de parsing HTTP
	uint32_t buffer_overflows;		 // Nombre de débordements de buffer
	uint32_t connection_timeouts;	 // Nombre de timeouts de connexion
	uint32_t avg_response_time_ms;	 // Temps moyen de réponse (ms)
	uint32_t total_response_time_ms; // Somme des temps de réponse (pour calculer la moyenne)
	uint32_t response_count;		 // Nombre total de réponses envoyées
} esp01_stats_t;

// --- Connexions TCP ---
// Informations sur une connexion TCP active
typedef struct
{
	int conn_id;					  // Identifiant de la connexion TCP
	uint32_t last_activity;			  // Timestamp de la dernière activité (ms)
	bool is_active;					  // Indique si la connexion est active (true/false)
	char client_ip[ESP01_MAX_IP_LEN]; // Adresse IP du client
	uint16_t server_port;			  // Port du serveur (pour les connexions entrantes)
} connection_info_t;

extern esp01_stats_t g_stats; // Statistiques globales accessibles partout
extern connection_info_t g_connections[ESP01_MAX_CONNECTIONS];
extern int g_connection_count;
extern uint16_t g_server_port;

// --- Trame IPD brute ---
// Informations extraites d'une trame IPD reçue de l'ESP01
typedef struct
{
	int conn_id;		  // ID de la connexion TCP
	int content_length;	  // Taille du payload reçu
	bool is_valid;		  // Trame IPD valide ?
	bool is_http_request; // Contient une requête HTTP ?
	char client_ip[16];
	int client_port;
	bool has_ip;
} http_request_t;

// --- Requête HTTP parsée ---
// Représente une requête HTTP décodée
typedef struct
{
	char method[ESP01_MAX_HTTP_METHOD_LEN];		 // Méthode HTTP (GET, POST, etc.)
	char path[ESP01_MAX_HTTP_PATH_LEN];			 // Chemin de la requête (ex: /status)
	char query_string[ESP01_MAX_HTTP_QUERY_LEN]; // Chaîne de requête (après '?')
	bool is_valid;								 // Parsing réussi ?
} http_parsed_request_t;

// --- Handler de route HTTP ---
// Prototype de fonction pour gérer une route HTTP
typedef void (*esp01_route_handler_t)(int conn_id, const http_parsed_request_t *request);

// --- Route HTTP ---
// Associe un chemin à un handler
typedef struct
{
	char path[ESP01_MAX_HTTP_PATH_LEN]; // Chemin de la route (ex: /status)
	esp01_route_handler_t handler;		// Pointeur vers la fonction handler
} esp01_route_t;

// --- Configuration ---
// Mode de fonctionnement du module ESP01
ESP01_Status_t esp01_connect_wifi_config(
	ESP01_WifiMode_t mode, // Mode WiFi (STA ou AP)
	const char *ssid,	   // SSID du réseau WiFi ou AP
	const char *password,  // Mot de passe du réseau WiFi ou AP
	bool use_dhcp,		   // Utilise DHCP pour l'IP ?
	const char *ip,		   // Adresse IP statique (si use_dhcp=false)
	const char *gateway,   // Passerelle par défaut (si use_dhcp=false)
	const char *netmask);  // Masque de sous-réseau (si use_dhcp=false)

ESP01_Status_t esp01_start_server_config(
	bool mode,		// Multi-connection TCP
	uint16_t port); // Port du serveur web

// ==================== Fonctions publiques ====================

// --- Gestion des erreurs ---
// Retourne une chaîne descriptive pour un code d'erreur
const char *esp01_get_error_string(ESP01_Status_t status);

// --- Initialisation et bas niveau ---
// Initialise le driver ESP01
ESP01_Status_t esp01_init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug,
						  uint8_t *dma_rx_buf, uint16_t dma_buf_size);
// Récupère les nouvelles données reçues par DMA
uint16_t esp01_get_new_data(uint8_t *buffer, uint16_t buffer_size);

// --- Commandes AT ---
// Envoie une commande AT brute et attend une réponse
ESP01_Status_t esp01_send_raw_command_dma(const char *cmd, char *response_buffer,
										  uint32_t max_response_size,
										  const char *expected_terminator,
										  uint32_t timeout_ms);
// Terminal interactif pour envoyer des commandes AT
ESP01_Status_t esp01_terminal_command(UART_HandleTypeDef *huart_terminal, uint32_t max_cmd_size,
									  uint32_t max_resp_size, uint32_t timeout_ms);

// --- Fonctions WiFi/Serveur ---
// Teste la communication avec l'ESP01
ESP01_Status_t esp01_test_at(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug, uint8_t *dma_rx_buf, uint16_t dma_buf_size);
// Définit le mode WiFi (STA/AP)
ESP01_Status_t esp01_set_wifi_mode(ESP01_WifiMode_t mode);
// Connecte au réseau WiFi
ESP01_Status_t esp01_connect_wifi(const char *ssid, const char *password);
// Démarre le serveur web sur le port spécifié
ESP01_Status_t esp01_start_web_server(uint16_t port);
// Arrête le serveur web
ESP01_Status_t esp01_stop_web_server(void);
// Vérifie l'état de connexion
ESP01_Status_t esp01_get_connection_status(void);
// Récupère l'adresse IP courante
ESP01_Status_t esp01_get_current_ip(char *ip_buf, size_t buf_len);

// --- Routage HTTP ---
// Efface toutes les routes HTTP
void esp01_clear_routes(void);
// Ajoute une route HTTP
ESP01_Status_t esp01_add_route(const char *path, esp01_route_handler_t handler);
// Recherche le handler associé à un chemin
esp01_route_handler_t esp01_find_route_handler(const char *path);

// --- Fonctions HTTP & utilitaires ---
// Parse un header IPD
http_request_t parse_ipd_header(const char *data);
// Ignore le payload HTTP restant
void discard_http_payload(int expected_length);
// Vide le buffer RX
void esp01_flush_rx_buffer(uint32_t timeout_ms);
// Parse une requête HTTP brute
ESP01_Status_t esp01_parse_http_request(const char *raw_request, http_parsed_request_t *parsed);
// Envoie une réponse HTTP générique
ESP01_Status_t esp01_send_http_response(int conn_id, int status_code, const char *content_type,
										const char *body, size_t body_len);
// Envoie une réponse JSON
ESP01_Status_t esp01_send_json_response(int conn_id, const char *json_data);
// Envoie une réponse 404
ESP01_Status_t esp01_send_404_response(int conn_id);
// Affiche le statut sur l'UART
void esp01_print_status(UART_HandleTypeDef *huart_output);
// Traite les requêtes HTTP reçues
void esp01_process_requests(void);

// --- Statistiques ---
// Récupère les statistiques courantes
void esp01_get_statistics(esp01_stats_t *stats);
// Réinitialise les statistiques
void esp01_reset_statistics(void);

// --- Divers ---
// Attend un motif dans le flux RX
ESP01_Status_t esp01_wait_for_pattern(const char *pattern, uint32_t timeout_ms);

#endif /* INC_STM32_WIFIESP_H_ */
