/*
 * STM32_WifiESP.h
 *
 *  Driver STM32 <-> ESP01 (AT)
 *  Version structurée avec routage HTTP
 *  2025 - weedm
 */

#ifndef INC_STM32_WIFIESP_H_
#define INC_STM32_WIFIESP_H_

#include "main.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// ==================== Macros & Config ====================

#define ESP01_DEBUG 1
#define ESP01_DMA_RX_BUF_SIZE 512

// ==================== Types ====================

typedef enum
{
    ESP01_OK = 0,
    ESP01_ERROR_AT,
    ESP01_TIMEOUT,
    ESP01_FAIL,
    ESP01_BUSY,
    ESP01_INVALID_RESPONSE,
    ESP01_BUFFER_TOO_SMALL,
    ESP01_NOT_INITIALIZED,
    ESP01_WIFI_CONNECT_FAIL
} ESP01_Status_t;

typedef enum
{
    ESP01_WIFI_MODE_STA = 1,
    ESP01_WIFI_MODE_AP = 2,
    ESP01_WIFI_MODE_STA_AP = 3
} ESP01_WifiMode_t;

// Structure pour requête IPD brute
typedef struct
{
    int conn_id;
    int content_length;
    bool is_valid;
    bool is_http_request;
} http_request_t;

// Structure pour requête HTTP parsée
typedef struct
{
    char method[8];
    char path[128];
    char query_string[256];
    bool is_valid;
} http_parsed_request_t;

// Prototype de handler de route HTTP
typedef void (*esp01_route_handler_t)(int conn_id, const http_parsed_request_t *request);

// Structure pour une route HTTP
typedef struct
{
    char path[64];
    esp01_route_handler_t handler;
} esp01_route_t;

// ==================== Fonctions bas niveau (driver) ====================

ESP01_Status_t esp01_init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug, uint8_t *dma_rx_buf, uint16_t dma_buf_size);
uint16_t esp01_get_new_data(uint8_t *buffer, uint16_t buffer_size);

// ==================== Commandes AT génériques ====================

ESP01_Status_t esp01_send_raw_command_dma(const char *cmd, char **response_buffer, uint32_t max_response_size, const char *expected_terminator, uint32_t timeout_ms);
ESP01_Status_t esp01_terminal_command(UART_HandleTypeDef *huart_terminal, uint32_t max_cmd_size, uint32_t max_resp_size, uint32_t timeout_ms);

// ==================== Fonctions haut niveau (WiFi & serveur) ====================

bool init_esp01_serveur_web(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug, uint8_t *dma_rx_buf, uint16_t dma_buf_size, const char *ssid, const char *password);
ESP01_Status_t esp01_test_at(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug, uint8_t *dma_rx_buf, uint16_t dma_buf_size);
ESP01_Status_t esp01_set_wifi_mode(ESP01_WifiMode_t mode);
ESP01_Status_t esp01_connect_wifi(const char *ssid, const char *password);
ESP01_Status_t esp01_start_web_server(uint16_t port);
ESP01_Status_t esp01_get_current_ip(char *ip_buf, size_t buf_len);
ESP01_Status_t esp01_disconnect_wifi(void);
ESP01_Status_t esp01_reset(void);
ESP01_Status_t esp01_stop_web_server(void);

// ==================== Fonctions HTTP & utilitaires ====================

http_request_t parse_ipd_header(const char *data);
void discard_http_payload(int expected_length);
void handle_http_request(int conn_id, const char *page, size_t page_len);

// ==================== Routage HTTP ====================

ESP01_Status_t esp01_add_route(const char *path, esp01_route_handler_t handler);
void esp01_clear_routes(void);
esp01_route_handler_t esp01_find_route_handler(const char *path);

// ==================== Parsing HTTP ====================

ESP01_Status_t esp01_parse_http_request(const char *raw_request, http_parsed_request_t *parsed);
ESP01_Status_t esp01_get_query_param(const char *query_string, const char *param_name, char *value_buffer, size_t buffer_size);

// ==================== Réponses HTTP ====================

ESP01_Status_t esp01_send_http_response(int conn_id, int status_code, const char *content_type, const char *body, size_t body_len);
ESP01_Status_t esp01_send_json_response(int conn_id, const char *json_data);
ESP01_Status_t esp01_send_404_response(int conn_id);

// ==================== Fonctions de gestion d'état ====================

ESP01_Status_t esp01_get_connection_status(void);

// ==================== Diagnostic ====================

void esp01_print_status(UART_HandleTypeDef *huart_output);

// ==================== Boucle principale ====================

void esp01_process_requests(void);

#endif /* INC_STM32_WIFIESP_H_ */
