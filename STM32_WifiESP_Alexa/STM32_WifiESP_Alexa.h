#ifndef INC_STM32_WIFIESP_ALEXA_H_
#define INC_STM32_WIFIESP_ALEXA_H_

// ==================== INCLUDES ====================

#include "main.h"	 // Pour les définitions de HAL et les types de données STM32
#include <stdio.h>	 // Pour printf et autres fonctions de sortie standard
#include <stdint.h>	 // Pour les types d'entiers standardisés (uint8_t, int32_t, etc.)
#include <stdbool.h> // Pour les types booléens (true/false)
#include <stdlib.h>	 // Pour les fonctions utilitaires comme malloc, free, etc.
#include <string.h>	 // Pour les fonctions de manipulation de chaînes

// ==================== DÉFINITIONS & MACROS ====================

#define ESP01_DEBUG 1

// --- Buffers et tailles ---
#define ESP01_DMA_RX_BUF_SIZE 512
#define ESP01_MAX_ROUTES 8
#define ESP01_MAX_CONNECTIONS 5
#define ESP01_FLUSH_TIMEOUT_MS 300
#define ESP01_SMALL_BUF_SIZE 128
#define ESP01_DMA_TEST_BUF_SIZE 64
#define ESP01_SHORT_DELAY_MS 10
#define ESP01_MAX_LOG_MSG 512
#define ESP01_MAX_WARN_MSG 100
#define ESP01_MAX_HEADER_LINE 256
#define ESP01_MAX_TOTAL_HTTP 2048
#define ESP01_MAX_CIPSEND_CMD 64
#define ESP01_MAX_IP_LEN 32
#define ESP01_MAX_HTTP_METHOD_LEN 8
#define ESP01_MAX_HTTP_PATH_LEN 64
#define ESP01_MAX_HTTP_QUERY_LEN 128
#define ESP01_MAX_HEADER_BUF 512
#define ESP01_MAX_DBG_BUF 128
#define ESP01_MAX_ROUTE_DBG_BUF 80
#define ESP01_MAX_CMD_BUF 256
#define ESP01_MAX_RESP_BUF 2048
#define ESP01_MAX_CIPSEND_BUF 32
#define ESP01_MAX_HTTP_REQ_BUF 256
#define ESP01_TMP_BUF_SIZE 256
#define ESP01_CMD_RESP_BUF_SIZE 64
#define ESP01_AT_VERSION_BUF 128
#define ESP01_IP_BUF_SIZE 32
#define ESP01_MAX_BODY_PARAMS 8
#define ESP01_MAX_ROW_SIZE 128
#define ESP01_MAX_PARAMS_HTML 512
#define ESP01_MAX_HTML_SIZE 2048
#define ESP01_MAX_AT_VERSION 64

#define IPD_HEADER_MIN_LEN 5

// --- Timeouts ---
#define ESP01_TIMEOUT_SHORT 1000
#define ESP01_TIMEOUT_MEDIUM 3000
#define ESP01_TIMEOUT_LONG 5000
#define ESP01_TIMEOUT_WIFI 15000
#define ESP01_TERMINAL_TIMEOUT_MS 30000
#define ESP01_CONN_TIMEOUT_MS 30000

#define ESP01_MULTI_CONNECTION 1

// --- HTTP ---
#define ESP01_HTTP_404_BODY "<html><body><h1>404 - Page Not Found</h1></body></html>"
#define ESP01_HTTP_404_BODY_LEN (sizeof(ESP01_HTTP_404_BODY) - 1)
#define ESP01_HTTP_FAVICON_PATH "/favicon.ico"
#define ESP01_HTTP_FAVICON_CODE 204
#define ESP01_HTTP_FAVICON_TYPE "image/x-icon"
#define ESP01_HTTP_OK_CODE 200
#define ESP01_HTTP_OK_TYPE "application/json"
#define ESP01_HTTP_NOT_FOUND_CODE 404
#define ESP01_HTTP_NOT_FOUND_TYPE "text/html"
#define ESP01_HTTP_INTERNAL_ERR_CODE 500
#define ESP01_HTTP_UNKNOWN_CODE 0
#define ESP01_HTTP_CONN_CLOSE "Connection: close\r\n"
#define ESP01_HTTP_VERSION "HTTP/1.1"
#define ESP01_HTTP_CRLF "\r\n"
#define ESP01_HTTP_HEADER_END "\r\n\r\n"

// ==================== TYPES & STRUCTURES ====================

typedef enum
{
	ESP01_OK = 0,
	ESP01_FAIL = -1,
	ESP01_TIMEOUT = -2,
	ESP01_NOT_INITIALIZED = -3,
	ESP01_INVALID_PARAM = -4,
	ESP01_BUFFER_OVERFLOW = -5,
	ESP01_WIFI_NOT_CONNECTED = -6,
	ESP01_HTTP_PARSE_ERROR = -7,
	ESP01_ROUTE_NOT_FOUND = -8,
	ESP01_CONNECTION_ERROR = -9,
	ESP01_MEMORY_ERROR = -10,
	ESP01_EXIT = -100
} ESP01_Status_t;

typedef enum
{
	ESP01_WIFI_MODE_STA = 1,
	ESP01_WIFI_MODE_AP = 2
} ESP01_WifiMode_t;

typedef struct
{
	uint32_t total_requests;
	uint32_t successful_responses;
	uint32_t failed_responses;
	uint32_t parse_errors;
	uint32_t buffer_overflows;
	uint32_t connection_timeouts;
	uint32_t avg_response_time_ms;
	uint32_t total_response_time_ms;
	uint32_t response_count;
} esp01_stats_t;

typedef struct
{
	int conn_id;
	uint32_t last_activity;
	bool is_active;
	char client_ip[ESP01_MAX_IP_LEN];
	uint16_t server_port;
	uint16_t client_port;
} connection_info_t;

typedef struct
{
	int conn_id;
	int content_length;
	bool is_valid;
	bool is_http_request;
	char client_ip[16];
	int client_port;
	bool has_ip;
	int header_len_incl_colon; // Length of the +IPD header including the colon
} http_request_t;

typedef struct
{
	char method[ESP01_MAX_HTTP_METHOD_LEN];
	char path[ESP01_MAX_HTTP_PATH_LEN];
	char query_string[ESP01_MAX_HTTP_QUERY_LEN];
	char headers_buf[512]; // <-- Ajout pour stocker les headers HTTP bruts
	bool is_valid;
} http_parsed_request_t;

typedef void (*esp01_route_handler_t)(int conn_id, const http_parsed_request_t *request);

typedef struct
{
	char path[ESP01_MAX_HTTP_PATH_LEN];
	esp01_route_handler_t handler;
} esp01_route_t;

typedef struct
{
	const char *key;
	size_t key_len;
	const char *value;
	size_t value_len;
} http_header_kv_t;

// ==================== VARIABLES GLOBALES ====================

extern esp01_stats_t g_stats;
extern connection_info_t g_connections[ESP01_MAX_CONNECTIONS];
extern int g_connection_count;
extern uint16_t g_server_port;
extern const ESP01_WifiMode_t g_wifi_mode;
/******************** FONCTIONS HAUT NIVEAU ********************/
// ==================== DRIVER ====================

ESP01_Status_t
esp01_init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug,
		   uint8_t *dma_rx_buf, uint16_t dma_buf_size);

ESP01_Status_t esp01_flush_rx_buffer(uint32_t timeout_ms);

// ==================== COMMANDES AT GÉNÉRIQUES ====================

ESP01_Status_t esp01_send_raw_command_dma(const char *cmd, char *response_buffer,
										  uint32_t max_response_size,
										  const char *expected_terminator,
										  uint32_t timeout_ms);

ESP01_Status_t esp01_wait_for_pattern(const char *pattern, uint32_t timeout_ms);

// ==================== WIFI & SERVEUR ====================

ESP01_Status_t esp01_connect_wifi_config(
	ESP01_WifiMode_t mode,
	const char *ssid,
	const char *password,
	bool use_dhcp,
	const char *ip,
	const char *gateway,
	const char *netmask);

ESP01_Status_t esp01_start_server_config(
	bool multi_conn,
	uint16_t port);

ESP01_Status_t esp01_test_at(void);
ESP01_Status_t esp01_get_at_version(char *version_buf, size_t buf_size);
ESP01_Status_t esp01_stop_web_server(void);
ESP01_Status_t esp01_get_connection_status(void);
ESP01_Status_t esp01_get_current_ip(char *ip_buf, size_t buf_len);
ESP01_Status_t esp01_print_connection_status(void);

// ==================== ROUTAGE HTTP ====================

void esp01_clear_routes(void);
ESP01_Status_t esp01_add_route(const char *path, esp01_route_handler_t handler);
esp01_route_handler_t esp01_find_route_handler(const char *path);

// ==================== FONCTIONS DIVERSES & UTILITAIRES ====================

ESP01_Status_t esp01_parse_http_request(const char *raw_request, http_parsed_request_t *parsed);
ESP01_Status_t esp01_send_http_response(int conn_id, int status_code, const char *content_type,
										const char *body, size_t body_len);
ESP01_Status_t esp01_send_json_response(int conn_id, const char *json_data);
ESP01_Status_t esp01_send_404_response(int conn_id);
ESP01_Status_t esp01_http_get(const char *host, uint16_t port, const char *path, char *response, size_t response_size);

// ==================== GESTION DES CONNEXIONS ====================

const char *esp01_get_error_string(ESP01_Status_t status);
int esp01_get_active_connection_count(void);
void esp01_process_requests(void);
void esp01_cleanup_inactive_connections();

#endif /* INC_STM32_WIFIESP_ALEXA_H_ */
