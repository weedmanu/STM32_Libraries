#ifndef INC_STM32_WIFIESP_H_
#define INC_STM32_WIFIESP_H_

#include "main.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// ==================== Macros de configuration ====================

// Active/désactive les logs de debug (1 = actif)
#define ESP01_DEBUG 1

// Taille du buffer DMA pour la réception UART
#define ESP01_DMA_RX_BUF_SIZE 512

// Nombre maximum de routes HTTP gérées
#define ESP01_MAX_ROUTES 8

// Longueur minimale d'un header +IPD
#define IPD_HEADER_MIN_LEN 5

// ==================== Timeouts (en ms) ====================

// Timeout court pour les commandes rapides
#define ESP01_TIMEOUT_SHORT 1000
// Timeout moyen pour la plupart des échanges
#define ESP01_TIMEOUT_MEDIUM 3000
// Timeout long pour les opérations lentes (ex: envoi de page)
#define ESP01_TIMEOUT_LONG 5000
// Timeout très long pour la connexion WiFi
#define ESP01_TIMEOUT_WIFI 15000

// ==================== Types et structures ====================

// Statuts de retour des fonctions du driver
typedef enum
{
    ESP01_OK = 0,              // Succès
    ESP01_FAIL = 1,            // Échec générique
    ESP01_TIMEOUT = 2,         // Timeout atteint
    ESP01_NOT_INITIALIZED = 3, // Driver non initialisé
    ESP01_BUFFER_TOO_SMALL = 4 // Buffer trop petit
} ESP01_Status_t;

// Modes WiFi supportés par l'ESP01
typedef enum
{
    ESP01_WIFI_MODE_STA = 1,   // Station (client)
    ESP01_WIFI_MODE_AP = 2,    // Point d'accès
    ESP01_WIFI_MODE_STA_AP = 3 // Les deux
} ESP01_WifiMode_t;

// Structure représentant une trame IPD brute reçue de l'ESP01
typedef struct
{
    int conn_id;          // ID de la connexion TCP
    int content_length;   // Taille du payload
    bool is_valid;        // Trame IPD valide ?
    bool is_http_request; // Contient une requête HTTP ?
} http_request_t;

// Structure représentant une requête HTTP parsée
typedef struct
{
    char method[8];         // Méthode HTTP (GET, POST, etc.)
    char path[64];          // Chemin de la ressource demandée
    char query_string[128]; // Chaîne de paramètres GET (après '?')
    bool is_valid;          // Parsing réussi ?
} http_parsed_request_t;

// Prototype de handler de route HTTP
typedef void (*esp01_route_handler_t)(int conn_id, const http_parsed_request_t *request);

// Structure d'une route HTTP (chemin + handler associé)
typedef struct
{
    char path[64];                 // Chemin de la route (ex: "/led")
    esp01_route_handler_t handler; // Fonction à appeler
} esp01_route_t;


// ==================== Fonctions publiques ====================

// ----------- Fonctions bas niveau (initialisation, DMA, etc.) -----------

// Initialise le driver ESP01 (UART, debug UART, buffer DMA, taille buffer)
ESP01_Status_t esp01_init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug,
                          uint8_t *dma_rx_buf, uint16_t dma_buf_size);

// Récupère les nouveaux octets reçus via DMA UART
uint16_t esp01_get_new_data(uint8_t *buffer, uint16_t buffer_size);

// ----------- Commandes AT génériques -----------

// Envoie une commande AT et récupère la réponse (DMA, timeout, etc.)
ESP01_Status_t esp01_send_raw_command_dma(const char *cmd, char **response_buffer,
                                          uint32_t max_response_size,
                                          const char *expected_terminator,
                                          uint32_t timeout_ms);

// Permet d'envoyer une commande AT depuis un terminal UART
ESP01_Status_t esp01_terminal_command(UART_HandleTypeDef *huart_terminal, uint32_t max_cmd_size,
                                      uint32_t max_resp_size, uint32_t timeout_ms);

// ----------- Fonctions WiFi/Serveur -----------

// Teste la communication AT avec l'ESP01
ESP01_Status_t esp01_test_at(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug, uint8_t *dma_rx_buf, uint16_t dma_buf_size);

// Définit le mode WiFi de l'ESP01
ESP01_Status_t esp01_set_wifi_mode(ESP01_WifiMode_t mode);

// Connecte l'ESP01 à un réseau WiFi
ESP01_Status_t esp01_connect_wifi(const char *ssid, const char *password);

// Démarre le serveur web sur le port spécifié
ESP01_Status_t esp01_start_web_server(uint16_t port);

// ----------- Fonctions HTTP & utilitaires -----------

// Parse un header IPD brut pour extraire les infos de connexion
http_request_t parse_ipd_header(const char *data);

// Ignore/consomme un payload HTTP restant (ex: body POST non traité)
void discard_http_payload(int expected_length);

// Vide le buffer RX UART/DMA pendant un certain temps
void esp01_flush_rx_buffer(uint32_t timeout_ms);

// Parse une requête HTTP (GET/POST) et remplit la structure correspondante
ESP01_Status_t esp01_parse_http_request(const char *raw_request, http_parsed_request_t *parsed);

// Envoie une réponse HTTP générique (code, type, body)
ESP01_Status_t esp01_send_http_response(int conn_id, int status_code, const char *content_type,
                                        const char *body, size_t body_len);

// Envoie une réponse HTTP JSON (200 OK)
ESP01_Status_t esp01_send_json_response(int conn_id, const char *json_data);

// Envoie une réponse HTTP 404 Not Found
ESP01_Status_t esp01_send_404_response(int conn_id);

// Vérifie si l'ESP01 est connecté au WiFi
ESP01_Status_t esp01_get_connection_status(void);

// Arrête le serveur web (ferme le port TCP)
ESP01_Status_t esp01_stop_web_server(void);

// Affiche le statut de l'ESP01 sur un terminal UART
void esp01_print_status(UART_HandleTypeDef *huart_output);

// Traite les requêtes HTTP reçues (dispatcher)
void esp01_process_requests(void);

// ----------- Routage HTTP -----------

// Efface toutes les routes HTTP enregistrées
void esp01_clear_routes(void);

// Ajoute une route HTTP (chemin + handler)
ESP01_Status_t esp01_add_route(const char *path, esp01_route_handler_t handler);

// Recherche le handler associé à un chemin donné
esp01_route_handler_t esp01_find_route_handler(const char *path);

// ----------- Fonctions diverses -----------

// Attend un motif précis dans le flux RX (avec timeout)
ESP01_Status_t esp01_wait_for_pattern(const char *pattern, uint32_t timeout_ms);

// Récupère l'adresse IP courante de l'ESP01
ESP01_Status_t esp01_get_current_ip(char *ip_buf, size_t buf_len);

#endif /* INC_STM32_WIFIESP_H_ */
