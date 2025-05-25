/*
 * STM32_WifiESP.h
 *
 *  Created on: May 24, 2025
 *      Author: weedm
 */

#ifndef INC_STM32_WIFIESP_H_
#define INC_STM32_WIFIESP_H_

#include "stm32l4xx_hal.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// Active/désactive les logs du driver
#define ESP01_DEBUG 1 // Mets à 0 pour désactiver les logs

#define ESP01_DMA_RX_BUF_SIZE 512

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

// Définition de la structure http_request_t
typedef struct
{
    int conn_id;
    int content_length;
    bool is_valid;
    bool is_http_request;
} http_request_t;

typedef enum
{
    ESP01_WIFI_MODE_STA = 1,
    ESP01_WIFI_MODE_AP = 2,
    ESP01_WIFI_MODE_STA_AP = 3
} ESP01_WifiMode_t;

// Initialisation du driver (appelle aussi la réception DMA)
ESP01_Status_t esp01_init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug, uint8_t *dma_rx_buf, uint16_t dma_buf_size);

// Lit les nouvelles données reçues dans le buffer DMA circulaire
uint16_t esp01_get_new_data(uint8_t *buffer, uint16_t buffer_size);

// Envoi d'une commande AT, réponse dans un buffer dynamique (à free par l'appelant)
ESP01_Status_t esp01_send_raw_command_dma(const char *cmd, char **response_buffer, uint32_t max_response_size, const char *expected_terminator, uint32_t timeout_ms);

// Envoi d'une commande depuis le terminal série (si vous l'utilisez toujours)
ESP01_Status_t esp01_terminal_command(UART_HandleTypeDef *huart_terminal, uint32_t max_cmd_size, uint32_t max_resp_size, uint32_t timeout_ms);

// Ajoutez les nouvelles déclarations de fonctions
bool init_esp01_wifi(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug, uint8_t *dma_rx_buf, uint16_t dma_buf_size, const char *ssid, const char *password);
ESP01_Status_t esp01_test_at(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug, uint8_t *dma_rx_buf, uint16_t dma_buf_size);
ESP01_Status_t esp01_set_wifi_mode(ESP01_WifiMode_t mode);
ESP01_Status_t esp01_connect_wifi(const char *ssid, const char *password);
ESP01_Status_t esp01_start_web_server(uint16_t port);

http_request_t parse_ipd_header(const char *data);
void discard_http_payload(int expected_length);
void handle_http_request(int conn_id, const char *ip_address);
void try_get_ip_address(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug, char *ip_address);

#endif /* INC_STM32_WIFIESP_H_ */
