/*
 * STM32_WifiESP.c
 * Driver STM32 <-> ESP01 (AT) Serveur web
 * Version structurée avec routage HTTP - CORRIGÉE
 * 2025 - manu
 */

#include "STM32_WifiESP.h"

static esp01_route_t g_routes[ESP01_MAX_ROUTES];
static int g_route_count = 0;

// ==================== Variables globales internes ====================
static UART_HandleTypeDef *g_esp_uart = NULL;
static UART_HandleTypeDef *g_debug_uart = NULL;
static uint8_t *g_dma_rx_buf = NULL;
static uint16_t g_dma_buf_size = 0;
static volatile uint16_t g_rx_last_pos = 0;

// UNIFIÉ : Un seul accumulateur global pour toutes les opérations
static char g_accumulator[ESP01_DMA_RX_BUF_SIZE * 2];
static uint16_t g_acc_len = 0;

// Prototypes des fonctions internes
static void _flush_rx_buffer(uint32_t timeout_ms);

// ==================== Fonctions internes (static) ====================

// --- Log debug ---
#define ESP01_DEBUG 1

static void _esp_logln(const char *msg)
{
    if (ESP01_DEBUG && g_debug_uart && msg)
    {
        HAL_UART_Transmit(g_debug_uart, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
        HAL_UART_Transmit(g_debug_uart, (uint8_t *)"\r\n", 2, HAL_MAX_DELAY);
    }
}

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
    _esp_logln("Initialisation UART/DMA pour ESP01");
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

    if (copied > 0)
    {
        char dbg[64];
        snprintf(dbg, sizeof(dbg), "esp01_get_new_data: %u octets reçus", copied);
        _esp_logln(dbg);
    }

    return copied;
}

// ==================== Commandes AT génériques ====================

ESP01_Status_t esp01_send_raw_command_dma(const char *cmd, char **response_buffer,
                                          uint32_t max_response_size,
                                          const char *expected_terminator,
                                          uint32_t timeout_ms)
{
    if (!g_esp_uart || !cmd || !response_buffer)
    {
        if (response_buffer)
            *response_buffer = NULL;
        return ESP01_NOT_INITIALIZED;
    }

    *response_buffer = (char *)calloc(max_response_size, 1);
    if (!*response_buffer)
    {
        _esp_logln("ESP01: Échec allocation mémoire");
        return ESP01_BUFFER_TOO_SMALL;
    }

    const char *terminator = expected_terminator ? expected_terminator : "OK";

    char full_cmd[256];
    int cmd_len = snprintf(full_cmd, sizeof(full_cmd), "%s\r\n", cmd);

    _flush_rx_buffer(100);

    if (HAL_UART_Transmit(g_esp_uart, (uint8_t *)full_cmd, cmd_len, ESP01_TIMEOUT_SHORT) != HAL_OK)
    {
        free(*response_buffer);
        *response_buffer = NULL;
        return ESP01_FAIL;
    }

    bool found = _accumulate_and_search(terminator, timeout_ms, true);

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

    _esp_logln("Commande AT reçue via terminal");
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
        if (strstr(resp, "OK") || strstr(resp, "no change"))
        {
            free(resp);
            return ESP01_OK;
        }
        free(resp);
    }
    return status;
}

// ==================== Fonctions HTTP & utilitaires ====================

http_request_t parse_ipd_header(const char *data)
{
    http_request_t request = {0};

    char *ipd_start = strstr(data, "+IPD,");
    if (!ipd_start)
        return request;

    int parsed = sscanf(ipd_start, "+IPD,%d,%d:", &request.conn_id, &request.content_length);

    if (parsed == 2 && request.conn_id >= 0 && request.content_length >= 0)
    {
        request.is_valid = true;

        char *colon_pos = strchr(ipd_start, ':');
        if (colon_pos)
        {
            colon_pos++;

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

    if (!request.is_http_request)
    {
        _esp_logln("Trame IPD reçue mais non HTTP");
    }

    return request;
}

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

void esp01_flush_rx_buffer(uint32_t timeout_ms)
{
    _flush_rx_buffer(timeout_ms);
}

ESP01_Status_t esp01_parse_http_request(const char *raw_request, http_parsed_request_t *parsed)
{
    if (!raw_request || !parsed)
    {
        _esp_logln("esp01_parse_http_request: pointeur NULL");
        return ESP01_FAIL;
    }

    memset(parsed, 0, sizeof(http_parsed_request_t));

    char *line_end = strstr(raw_request, "\r\n");
    if (!line_end)
    {
        _esp_logln("esp01_parse_http_request: pas de CRLF trouvé");
        return ESP01_FAIL;
    }

    size_t first_line_len = line_end - raw_request;
    if (first_line_len >= sizeof(parsed->method) + sizeof(parsed->path) + 20)
    {
        _esp_logln("esp01_parse_http_request: première ligne trop longue");
        return ESP01_FAIL;
    }

    char first_line[256];
    strncpy(first_line, raw_request, first_line_len);
    first_line[first_line_len] = '\0';

    char *space1 = strchr(first_line, ' ');
    if (!space1)
    {
        _esp_logln("esp01_parse_http_request: pas d'espace après méthode");
        return ESP01_FAIL;
    }

    size_t method_len = space1 - first_line;
    if (method_len >= sizeof(parsed->method))
    {
        _esp_logln("esp01_parse_http_request: méthode trop longue");
        return ESP01_FAIL;
    }

    strncpy(parsed->method, first_line, method_len);
    parsed->method[method_len] = '\0';

    char *space2 = strchr(space1 + 1, ' ');
    if (!space2)
    {
        _esp_logln("esp01_parse_http_request: pas d'espace après path");
        return ESP01_FAIL;
    }

    size_t path_len = space2 - space1 - 1;
    if (path_len >= sizeof(parsed->path))
    {
        _esp_logln("esp01_parse_http_request: path trop long");
        return ESP01_FAIL;
    }

    strncpy(parsed->path, space1 + 1, path_len);
    parsed->path[path_len] = '\0';

    char *query_start = strchr(parsed->path, '?');
    if (query_start)
    {
        *query_start = '\0';
        query_start++;
        strncpy(parsed->query_string, query_start, sizeof(parsed->query_string) - 1);
        parsed->query_string[sizeof(parsed->query_string) - 1] = '\0';
    }

    parsed->is_valid = true;
    char dbg[128];
    snprintf(dbg, sizeof(dbg), "Requête HTTP parsée : méthode=%s, path=%s", parsed->method, parsed->path);
    _esp_logln(dbg);
    return ESP01_OK;
}

ESP01_Status_t esp01_send_http_response(int conn_id, int status_code, const char *content_type,
                                        const char *body, size_t body_len)
{
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

    int total_len = header_len + (int)body_len;
    char *response = (char *)calloc(total_len + 1, 1);
    if (!response)
    {
        _esp_logln("esp01_send_http_response: allocation mémoire échouée");
        return ESP01_FAIL;
    }

    memcpy(response, header, header_len);
    if (body && body_len > 0)
    {
        memcpy(response + header_len, body, body_len);
    }

    char cipsend_cmd[32];
    snprintf(cipsend_cmd, sizeof(cipsend_cmd), "AT+CIPSEND=%d,%d", conn_id, total_len);

    char *resp = NULL;
    ESP01_Status_t st = esp01_send_raw_command_dma(cipsend_cmd, &resp, ESP01_DMA_RX_BUF_SIZE, ">", ESP01_TIMEOUT_LONG);
    if (resp)
        free(resp);
    if (st != ESP01_OK)
    {
        _esp_logln("esp01_send_http_response: AT+CIPSEND échoué");
        free(response);
        return st;
    }

    HAL_UART_Transmit(g_esp_uart, (uint8_t *)response, total_len, HAL_MAX_DELAY);

    st = esp01_wait_for_pattern("SEND OK", ESP01_TIMEOUT_LONG);
    char dbg[128];
    snprintf(dbg, sizeof(dbg), "Envoi réponse HTTP sur connexion %d, taille %d", conn_id, (int)body_len);
    _esp_logln(dbg);
    free(response);
    return st;
}

ESP01_Status_t esp01_send_json_response(int conn_id, const char *json_data)
{
    _esp_logln("Envoi d'une réponse JSON");
    return esp01_send_http_response(conn_id, 200, "application/json", json_data, strlen(json_data));
}

ESP01_Status_t esp01_send_404_response(int conn_id)
{
    _esp_logln("Envoi d'une réponse 404");
    const char *body = "<html><body><h1>404 - Page Not Found</h1></body></html>";
    return esp01_send_http_response(conn_id, 404, "text/html", body, strlen(body));
}

ESP01_Status_t esp01_get_connection_status(void)
{
    _esp_logln("Vérification du statut de connexion ESP01");
    char *response = NULL;
    ESP01_Status_t status = esp01_send_raw_command_dma("AT+CIPSTATUS", &response, 512, "OK", ESP01_TIMEOUT_MEDIUM);

    if (status == ESP01_OK && response)
    {
        if (strstr(response, "STATUS:2") || strstr(response, "STATUS:3"))
        {
            free(response);
            _esp_logln("ESP01 connecté au WiFi");
            return ESP01_OK;
        }
    }

    if (response)
        free(response);
    _esp_logln("ESP01 non connecté au WiFi");
    return ESP01_FAIL;
}

ESP01_Status_t esp01_stop_web_server(void)
{
    _esp_logln("Arrêt du serveur web ESP01");
    char *response = NULL;
    ESP01_Status_t status = esp01_send_raw_command_dma("AT+CIPSERVER=0", &response, 256, "OK", ESP01_TIMEOUT_MEDIUM);
    if (response)
        free(response);
    return status;
}

void esp01_print_status(UART_HandleTypeDef *huart_output)
{
    if (!huart_output)
    {
        _esp_logln("esp01_print_status: huart_output NULL");
        return;
    }

    const char *header = "\r\n=== STATUS ESP01 ===\r\n";
    HAL_UART_Transmit(huart_output, (uint8_t *)header, strlen(header), HAL_MAX_DELAY);

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

    char ip[32];
    if (esp01_get_current_ip(ip, sizeof(ip)) == ESP01_OK)
    {
        snprintf(msg, sizeof(msg), "IP: %s\r\n", ip);
        HAL_UART_Transmit(huart_output, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
    }

    snprintf(msg, sizeof(msg), "Routes définies: %d/%d\r\n", g_route_count, ESP01_MAX_ROUTES);
    HAL_UART_Transmit(huart_output, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);

    const char *footer = "==================\r\n";
    HAL_UART_Transmit(huart_output, (uint8_t *)footer, strlen(footer), HAL_MAX_DELAY);
    _esp_logln("Statut ESP01 affiché sur terminal");
}

// CORRECTION 2: Simplification de discard_http_payload
void discard_http_payload(int expected_length)
{
    char discard_buf[256];
    int remaining = expected_length;
    uint32_t timeout_start = HAL_GetTick();
    const uint32_t timeout_ms = ESP01_TIMEOUT_MEDIUM;

    _esp_logln("Vidage du payload HTTP...");

    while (remaining > 0 && (HAL_GetTick() - timeout_start) < timeout_ms)
    {
        int to_read = (remaining < sizeof(discard_buf)) ? remaining : sizeof(discard_buf);
        uint16_t read_len = esp01_get_new_data((uint8_t *)discard_buf, to_read);

        if (read_len > 0)
        {
            remaining -= read_len;
            char debug_msg[50];
            snprintf(debug_msg, sizeof(debug_msg), "Ignoré %d octets, reste %d", read_len, remaining);
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
        snprintf(warn_msg, sizeof(warn_msg), "AVERTISSEMENT: %d octets non lus", remaining);
        _esp_logln(warn_msg);
    }
    else
    {
        _esp_logln("Payload HTTP complètement vidé");
    }
}

// CORRECTION 3: Fonction pour trouver le prochain +IPD valide
static char *_find_next_ipd(char *buffer, int buffer_len)
{
    char *search_pos = buffer;
    char *ipd_pos;

    while ((ipd_pos = strstr(search_pos, "+IPD,")) != NULL)
    {
        // Vérifier si l'IPD a une structure valide basique
        char *colon = strchr(ipd_pos, ':');
        if (colon && (colon - buffer) < buffer_len - 1)
        {
            return ipd_pos;
        }
        // Continuer la recherche après ce +IPD invalide
        search_pos = ipd_pos + IPD_HEADER_MIN_LEN;
    }
    return NULL;
}

// CORRECTION 4: Processus de requêtes avec récupération d'erreur améliorée
void esp01_process_requests(void)
{
    uint8_t buffer[ESP01_DMA_RX_BUF_SIZE];
    uint16_t len = esp01_get_new_data(buffer, sizeof(buffer));
    if (len == 0)
    {
        return;
    }

    // Ajouter à l'accumulateur global (un seul accumulateur maintenant)
    if (g_acc_len + len < sizeof(g_accumulator) - 1)
    {
        memcpy(g_accumulator + g_acc_len, buffer, len);
        g_acc_len += len;
        g_accumulator[g_acc_len] = '\0';
        char dbg[64];
        snprintf(dbg, sizeof(dbg), "Accumulateur += %u octets (total %d)", len, g_acc_len);
        _esp_logln(dbg);
    }
    else
    {
        _esp_logln("Débordement accumulateur, recherche du prochain +IPD valide");
        // CORRECTION: Ne pas tout effacer, chercher le prochain +IPD valide
        char *next_ipd = _find_next_ipd(g_accumulator, g_acc_len);
        if (next_ipd)
        {
            int shift = next_ipd - g_accumulator;
            memmove(g_accumulator, next_ipd, g_acc_len - shift);
            g_acc_len -= shift;
            g_accumulator[g_acc_len] = '\0';
            _esp_logln("Repositionné sur prochain +IPD valide");
        }
        else
        {
            g_acc_len = 0;
            g_accumulator[0] = '\0';
            _esp_logln("Aucun +IPD valide trouvé, reset complet");
        }
        return;
    }

    // Chercher un IPD valide
    char *ipd_start = _find_next_ipd(g_accumulator, g_acc_len);
    if (!ipd_start)
    {
        _esp_logln("Pas de +IPD valide trouvé, attente...");
        return;
    }

    // Synchroniser sur le début de l'IPD si nécessaire
    if (ipd_start != g_accumulator)
    {
        int shift = ipd_start - g_accumulator;
        memmove(g_accumulator, ipd_start, g_acc_len - shift);
        g_acc_len -= shift;
        g_accumulator[g_acc_len] = '\0';
        _esp_logln("Synchronisation sur +IPD valide");
    }

    // Vérifier si on a les headers complets
    char *headers_end = strstr(g_accumulator, "\r\n\r\n");
    if (!headers_end)
    {
        _esp_logln("Headers incomplets, attente...");
        return;
    }

    // Parser l'en-tête IPD
    http_request_t req = parse_ipd_header(g_accumulator);
    if (!req.is_valid)
    {
        _esp_logln("IPD non valide, recherche du suivant");
        // CORRECTION: Chercher le prochain IPD au lieu de tout effacer
        char *next_ipd = _find_next_ipd(g_accumulator + IPD_HEADER_MIN_LEN, g_acc_len - IPD_HEADER_MIN_LEN);
        if (next_ipd)
        {
            int shift = next_ipd - g_accumulator;
            memmove(g_accumulator, next_ipd, g_acc_len - shift);
            g_acc_len -= shift;
            g_accumulator[g_acc_len] = '\0';
            _esp_logln("Repositionné sur prochain +IPD");
        }
        else
        {
            g_acc_len = 0;
            g_accumulator[0] = '\0';
        }
        return;
    }

    int ipd_payload_len = req.content_length;
    char *colon_pos = strchr(g_accumulator, ':');
    if (!colon_pos)
    {
        _esp_logln("':' non trouvé, recherche du prochain IPD");
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
        return;
    }

    colon_pos++;
    int received_payload = g_acc_len - (colon_pos - g_accumulator);
    if (received_payload < ipd_payload_len)
    {
        _esp_logln("Payload incomplet, attente...");
        return;
    }

    // Parser la requête HTTP
    http_parsed_request_t parsed = {0};
    if (esp01_parse_http_request(colon_pos, &parsed) == ESP01_OK && parsed.is_valid)
    {
        char dbg[128];
        snprintf(dbg, sizeof(dbg), "Handler pour path '%s'", parsed.path);
        _esp_logln(dbg);

        esp01_route_handler_t handler = esp01_find_route_handler(parsed.path);
        if (handler)
        {
            _esp_logln("Appel du handler utilisateur");
            handler(req.conn_id, &parsed);
        }
        else
        {
            _esp_logln("Handler non trouvé, envoi 404");
            esp01_send_404_response(req.conn_id);
        }
    }
    else
    {
        _esp_logln("Parsing HTTP échoué");
    }

    // Gérer le body si nécessaire
    int headers_len = headers_end + 4 - colon_pos;
    int body_len = ipd_payload_len - headers_len;
    if ((strcmp(parsed.method, "POST") == 0 || strcmp(parsed.method, "PUT") == 0) && body_len > 0)
    {
        _esp_logln("Vidage du body HTTP (POST/PUT)");
        discard_http_payload(body_len);
    }

    _esp_logln("Requête traitée, accumulateur reset");
    g_acc_len = 0;
    g_accumulator[0] = '\0';
}

ESP01_Status_t esp01_wait_for_pattern(const char *pattern, uint32_t timeout_ms)
{
    if (_accumulate_and_search(pattern, timeout_ms, false))
        return ESP01_OK;
    return ESP01_TIMEOUT;
}

void esp01_clear_routes(void)
{
    _esp_logln("Effacement de toutes les routes HTTP");
    g_route_count = 0;
}

ESP01_Status_t esp01_add_route(const char *path, esp01_route_handler_t handler)
{
    if (!path || !handler || g_route_count >= ESP01_MAX_ROUTES)
    {
        _esp_logln("Erreur ajout de route : paramètre invalide ou trop de routes");
        return ESP01_FAIL;
    }
    strncpy(g_routes[g_route_count].path, path, sizeof(g_routes[g_route_count].path) - 1);
    g_routes[g_route_count].path[sizeof(g_routes[g_route_count].path) - 1] = '\0';
    g_routes[g_route_count].handler = handler;
    g_route_count++;
    char dbg[80];
    snprintf(dbg, sizeof(dbg), "Route ajoutée : %s (total %d)", path, g_route_count);
    _esp_logln(dbg);
    return ESP01_OK;
}

esp01_route_handler_t esp01_find_route_handler(const char *path)
{
    for (int i = 0; i < g_route_count; i++)
    {
        if (strcmp(g_routes[i].path, path) == 0)
        {
            char dbg[80];
            snprintf(dbg, sizeof(dbg), "Route trouvée pour path : %s", path);
            _esp_logln(dbg);
            return g_routes[i].handler;
        }
    }
    char dbg[80];
    snprintf(dbg, sizeof(dbg), "Aucune route trouvée pour path : %s", path);
    _esp_logln(dbg);
    return NULL;
}

ESP01_Status_t esp01_get_current_ip(char *ip_buffer, size_t buffer_size)
{
    char *response = NULL;
    ESP01_Status_t status = esp01_send_raw_command_dma("AT+CIFSR", &response, 512, "OK", ESP01_TIMEOUT_LONG);

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
