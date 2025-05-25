/*
 * STM32_WifiESP.c
 * Driver STM32 <-> ESP01 (AT)
 * Version structurée avec sous-fonctions pour chaque étape
 * 2025 - weedm
 */

#include "STM32_WifiESP.h"

// Variables globales internes
static UART_HandleTypeDef *g_esp_uart = NULL;
static UART_HandleTypeDef *g_debug_uart = NULL;
static uint8_t *g_dma_rx_buf = NULL;
static uint16_t g_dma_buf_size = 0;
static volatile uint16_t g_rx_last_pos = 0;
static char g_accumulator[ESP01_DMA_RX_BUF_SIZE * 2];
static uint16_t g_acc_len = 0;
static char ip_address[32] = "N/A";

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

// --- Initialisation du driver UART/DMA ---
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

// --- Calcule la position actuelle d'écriture du DMA ---
static uint16_t _get_dma_write_pos(void)
{
    return g_dma_buf_size - __HAL_DMA_GET_COUNTER(g_esp_uart->hdmarx);
}

// --- Calcule le nombre de nouveaux octets disponibles ---
static uint16_t _get_available_bytes(void)
{
    uint16_t write_pos = _get_dma_write_pos();
    if (write_pos >= g_rx_last_pos)
        return write_pos - g_rx_last_pos;
    else
        return (g_dma_buf_size - g_rx_last_pos) + write_pos;
}

// --- Lit les nouvelles données du buffer DMA ---
uint16_t esp01_get_new_data(uint8_t *buffer, uint16_t buffer_size)
{
    if (!g_esp_uart || !g_dma_rx_buf || !buffer || buffer_size == 0)
    {
        if (buffer && buffer_size > 0) buffer[0] = '\0';
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

// --- Accumule les données et cherche un pattern ---
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
        uint16_t new_data = esp01_get_new_data((uint8_t*)temp_buf, sizeof(temp_buf));
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

// --- Envoi d'une commande AT ---
ESP01_Status_t esp01_send_raw_command_dma(const char *cmd, char **response_buffer,
                                         uint32_t max_response_size, const char *expected_terminator,
                                         uint32_t timeout_ms)
{
    if (!g_esp_uart || !cmd || !response_buffer)
    {
        _esp_logln("ESP01 Send Error: Invalid params");
        if (response_buffer) *response_buffer = NULL;
        return ESP01_NOT_INITIALIZED;
    }

    *response_buffer = (char *)malloc(max_response_size);
    if (!*response_buffer)
    {
        _esp_logln("ESP01 Send Error: Malloc failed");
        return ESP01_BUFFER_TOO_SMALL;
    }

    const char *terminator = expected_terminator ? expected_terminator : "OK";
    bool send_command = (strlen(cmd) > 0);

    if (send_command)
    {
        char cmd_with_crlf[256];
        snprintf(cmd_with_crlf, sizeof(cmd_with_crlf), "%s\r\n", cmd);
        if (ESP01_DEBUG)
        {
            _esp_logln("ESP01 TX:");
            _esp_logln(cmd_with_crlf);
        }
        if (HAL_UART_Transmit(g_esp_uart, (uint8_t *)cmd_with_crlf, strlen(cmd_with_crlf), 1000) != HAL_OK)
        {
            _esp_logln("ESP01 TX Error");
            free(*response_buffer);
            *response_buffer = NULL;
            return ESP01_FAIL;
        }
    }

    if (ESP01_DEBUG)
    {
        char msg[128];
        snprintf(msg, sizeof(msg), "ESP01 RX: Waiting for '%s'...", terminator);
        _esp_logln(msg);
    }

    if (_accumulate_and_search(terminator, timeout_ms, send_command))
    {
        uint16_t len_to_copy = (g_acc_len < max_response_size - 1) ? g_acc_len : max_response_size - 1;
        memcpy(*response_buffer, g_accumulator, len_to_copy);
        (*response_buffer)[len_to_copy] = '\0';

        if (ESP01_DEBUG)
        {
            _esp_logln("ESP01 RX: Pattern found");
            _esp_logln(*response_buffer);
        }

        if (strstr(*response_buffer, "ERROR"))
            return ESP01_ERROR_AT;

        return ESP01_OK;
    }
    else
    {
        if (ESP01_DEBUG)
        {
            _esp_logln("ESP01 RX: Timeout");
            if (g_acc_len > 0)
            {
                _esp_logln("Partial data:");
                _esp_logln(g_accumulator);
            }
        }

        if (g_acc_len > 0)
        {
            uint16_t len_to_copy = (g_acc_len < max_response_size - 1) ? g_acc_len : max_response_size - 1;
            memcpy(*response_buffer, g_accumulator, len_to_copy);
            (*response_buffer)[len_to_copy] = '\0';
        }
        else
        {
            free(*response_buffer);
            *response_buffer = NULL;
        }

        return ESP01_TIMEOUT;
    }
}

// --- Terminal debug (optionnel) ---
ESP01_Status_t esp01_terminal_command(UART_HandleTypeDef *huart_terminal, uint32_t max_cmd_size,
                                    uint32_t max_resp_size, uint32_t timeout_ms)
{
    if (!huart_terminal) return ESP01_FAIL;

    char cmd_buf[128] = {0};
    char *resp_buf = NULL;

    const char *prompt = "\r\nCommande AT (ou 'exit'): ";
    HAL_UART_Transmit(huart_terminal, (uint8_t *)prompt, strlen(prompt), HAL_MAX_DELAY);

    uint32_t idx = 0;
    uint8_t c = 0;
    while (idx < sizeof(cmd_buf) - 1)
    {
        if (HAL_UART_Receive(huart_terminal, &c, 1, HAL_MAX_DELAY) == HAL_OK)
        {
            if (c == '\r' || c == '\n')
            {
                if (idx > 0) break;
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

    if (strlen(cmd_buf) == 0) return ESP01_OK;
    if (strcmp(cmd_buf, "exit") == 0) return ESP01_FAIL;

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

// --- Sous-fonctions haut niveau ---

ESP01_Status_t esp01_test_at(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug, uint8_t *dma_rx_buf, uint16_t dma_buf_size)
{
    char *response = NULL;
    ESP01_Status_t status = esp01_init(huart_esp, huart_debug, dma_rx_buf, dma_buf_size);
    if (status != ESP01_OK) return status;
    status = esp01_send_raw_command_dma("AT", &response, 256, "OK", 2000);
    if (response) free(response);
    return status;
}

ESP01_Status_t esp01_set_wifi_mode(ESP01_WifiMode_t mode)
{
    char *response = NULL;
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CWMODE=%d", (int)mode);
    ESP01_Status_t status = esp01_send_raw_command_dma(cmd, &response, 128, "OK", 2000);
    if (response) free(response);
    return status;
}

ESP01_Status_t esp01_connect_wifi(const char *ssid, const char *password)
{
    char *response = NULL;
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
    ESP01_Status_t status = esp01_send_raw_command_dma(cmd, &response, 512, "WIFI GOT IP", 15000);
    if (response) free(response);
    return status;
}

ESP01_Status_t esp01_start_web_server(uint16_t port)
{
    char *resp = NULL;
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CIPSERVER=1,%u", port);
    ESP01_Status_t status = esp01_send_raw_command_dma(cmd, &resp, 128, "OK", 3000);
    if (resp) free(resp);
    return status;
}

// --- Fonctions utilitaires HTTP (parse, discard, handle) ---
http_request_t parse_ipd_header(const char *data)
{
    http_request_t request = {0};

    char *ipd_start = strstr(data, "+IPD,");
    if (!ipd_start)
    {
        return request; // is_valid reste false
    }

    // Parser: +IPD,conn_id,length:
    int parsed = sscanf(ipd_start, "+IPD,%d,%d:", &request.conn_id, &request.content_length);

    if (parsed == 2 && request.conn_id >= 0 && request.content_length >= 0)
    {
        request.is_valid = true;

        // Vérifier si c'est une requête HTTP (chercher "GET " ou "POST ")
        char *payload_start = strchr(ipd_start, ':');
        if (payload_start)
        {
            payload_start++; // Passer le ':'
            if (strstr(payload_start, "GET ") || strstr(payload_start, "POST "))
            {
                request.is_http_request = true;
            }
        }
    }

    return request;
}

void discard_http_payload(int expected_length)
{
    char discard_buf[128];
    int remaining = expected_length;
    uint32_t timeout_start = HAL_GetTick();

    while (remaining > 0 && (HAL_GetTick() - timeout_start) < 2000)
    {
        int to_read = (remaining < sizeof(discard_buf)) ? remaining : sizeof(discard_buf);
        uint16_t read_len = esp01_get_new_data((uint8_t *)discard_buf, to_read);

        if (read_len > 0)
        {
            remaining -= read_len;
        }
        else
        {
            HAL_Delay(10);
        }
    }

    if (remaining > 0)
    {
        _esp_logln("AVERTISSEMENT: Timeout lors de l'ignore du payload HTTP");
    }
}

void handle_http_request(int conn_id, const char *ip_address)
{
    char *response = NULL;

    // Préparer la réponse HTTP
    char http_body[512];
    snprintf(http_body, sizeof(http_body),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<!DOCTYPE html>"
        "<html><head><title>STM32 ESP01 Server</title></head>"
        "<body style='font-family:Arial;text-align:center;padding:50px;'>"
        "<h1>🚀 Hello from STM32!</h1>"
        "<p><strong>ESP01 IP:</strong> %s</p>"
        "<p><strong>Temps:</strong> %lu ms</p>"
        "<p>Serveur web fonctionnel sur STM32 + ESP01</p>"
        "</body></html>\r\n",
        ip_address, HAL_GetTick());

    int body_length = strlen(http_body);

    // Commande CIPSEND
    char cipsend_cmd[32];
    snprintf(cipsend_cmd, sizeof(cipsend_cmd), "AT+CIPSEND=%d,%d", conn_id, body_length);

    ESP01_Status_t status = esp01_send_raw_command_dma(cipsend_cmd, &response, 256, ">", 5000);

    if (status == ESP01_OK && response && strstr(response, ">"))
    {
        _esp_logln("Prompt '>' reçu, envoi réponse HTTP...");
        if (response) { free(response); response = NULL; }

        // Envoyer le corps HTTP
        HAL_UART_Transmit(g_esp_uart, (uint8_t *)http_body, body_length, 2000);

        // Attendre "SEND OK"
        status = esp01_send_raw_command_dma("", &response, 256, "SEND OK", 5000);
        if (status == ESP01_OK && response && strstr(response, "SEND OK"))
        {
            _esp_logln("Réponse HTTP envoyée avec succès");
        }
        else
        {
            _esp_logln("ERREUR: SEND OK non reçu");
        }
    }
    else
    {
        _esp_logln("ERREUR: Prompt '>' non reçu pour CIPSEND");
    }

    if (response) { free(response); response = NULL; }

    // Fermer la connexion
    char cipclose_cmd[32];
    snprintf(cipclose_cmd, sizeof(cipclose_cmd), "AT+CIPCLOSE=%d", conn_id);
    status = esp01_send_raw_command_dma(cipclose_cmd, &response, 256, "OK", 2000);

    if (status == ESP01_OK && response && strstr(response, "OK"))
    {
        _esp_logln("Connexion fermée avec succès");
    }
    else
    {
        _esp_logln("AVERTISSEMENT: Fermeture connexion échouée");
    }

    if (response) free(response);
}

void try_get_ip_address(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug, char *ip_address)
{
    char *response = NULL;
    ESP01_Status_t status;

    // Essayer AT+CIFSR
    status = esp01_send_raw_command_dma("AT+CIFSR", &response, 512, "OK", 3000);
    if (status == ESP01_OK && response && !strstr(response, "busy"))
    {
        char *ip_start = strstr(response, "+CIFSR:STAIP,\"");
        if (ip_start)
        {
            ip_start += strlen("+CIFSR:STAIP,\"");
            char *ip_end = strchr(ip_start, '"');
            if (ip_end && (ip_end - ip_start) < 32 - 1)
            {
                strncpy(ip_address, ip_start, ip_end - ip_start);
                ip_address[ip_end - ip_start] = '\0';
                char msg[64];
                snprintf(msg, sizeof(msg), "IP récupérée: %s", ip_address);
                _esp_logln(msg);
            }
        }
    }

    if (response) free(response);
}

/**
 * @brief Gère l'initialisation complète de l'ESP01 et la connexion WiFi
 * @param huart_esp: Pointeur vers la structure UART pour l'ESP
 * @param huart_debug: Pointeur vers la structure UART pour le debug
 * @param dma_rx_buf: Buffer DMA pour la réception
 * @param dma_buf_size: Taille du buffer DMA
 * @param ssid: SSID du réseau WiFi
 * @param password: Mot de passe du réseau WiFi
 * @retval true si succès, false sinon
 */
bool init_esp01_wifi(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug, uint8_t *dma_rx_buf, uint16_t dma_buf_size, const char *ssid, const char *password)
{
    char *response = NULL;
    ESP01_Status_t status;

    // 1. Initialiser le driver
    if (esp01_init(huart_esp, huart_debug, dma_rx_buf, dma_buf_size) != ESP01_OK)
    {
        return false;
    }

    // 2. Test AT
    status = esp01_send_raw_command_dma("AT", &response, 256, "OK", 2000);
    if (status != ESP01_OK || !response || !strstr(response, "OK"))
    {
        _esp_logln("ERREUR: Test AT échoué");
        if (response) free(response);
        return false;
    }
    _esp_logln("Test AT: OK");
    free(response);

    // 3. Mode station
    status = esp01_send_raw_command_dma("AT+CWMODE=1", &response, 256, "OK", 5000);
    if (status != ESP01_OK || !response || !strstr(response, "OK"))
    {
        _esp_logln("ERREUR: Mode station échoué");
        if (response) free(response);
        return false;
    }
    _esp_logln("Mode station: OK");
    free(response);

    // 4. Connexion WiFi
    char wifi_cmd[128];
    snprintf(wifi_cmd, sizeof(wifi_cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
    status = esp01_send_raw_command_dma(wifi_cmd, &response, 512, "WIFI GOT IP", 15000);
    if (status != ESP01_OK || !response || !strstr(response, "WIFI GOT IP"))
    {
        _esp_logln("ERREUR: Connexion WiFi échouée");
        if (response) free(response);
        return false;
    }
    _esp_logln("Connexion WiFi: OK");
    free(response);

    // 5. Obtenir l'adresse IP (avec retry si busy)
    for (int retry = 0; retry < 3; retry++)
    {
        HAL_Delay(1000); // Attendre que l'ESP01 soit prêt
        status = esp01_send_raw_command_dma("AT+CIFSR", &response, 512, "OK", 5000);

        if (status == ESP01_OK && response)
        {
            // Chercher l'IP dans la réponse
            char *ip_start = strstr(response, "+CIFSR:STAIP,\"");
            if (ip_start)
            {
                ip_start += strlen("+CIFSR:STAIP,\"");
                char *ip_end = strchr(ip_start, '"');
                if (ip_end && (ip_end - ip_start) < sizeof(ip_address) - 1)
                {
                    strncpy(ip_address, ip_start, ip_end - ip_start);
                    ip_address[ip_end - ip_start] = '\0';
                    char msg[64];
                    snprintf(msg, sizeof(msg), "Adresse IP: %s", ip_address);
                    _esp_logln(msg);
                    if (response) free(response);
                    break; // IP trouvée, sortir de la boucle
                }
            }

            // Si pas d'IP trouvée mais pas "busy", essayer une méthode alternative
            if (!strstr(response, "busy") && retry == 2)
            {
                _esp_logln("IP non trouvée, utilisation méthode alternative...");
                if (response) { free(response); response = NULL; }

                // Essayer AT+CWJAP?
                status = esp01_send_raw_command_dma("AT+CWJAP?", &response, 512, "OK", 3000);
                if (status == ESP01_OK && response)
                {
                    _esp_logln("Statut WiFi obtenu, IP sera détectée lors des connexions");
                }
            }
        }

        if (response) { free(response); response = NULL; }

        // Si IP trouvée, sortir
        if (strcmp(ip_address, "N/A") != 0)
        {
            break;
        }

        _esp_logln("Retry obtention IP...");
    }

    // 6. Mode multi-connexion
    status = esp01_send_raw_command_dma("AT+CIPMUX=1", &response, 256, "OK", 2000);
    if (status != ESP01_OK || !response || !strstr(response, "OK"))
    {
        _esp_logln("ERREUR: Mode multi-connexion échoué");
        if (response) free(response);
        return false;
    }
    _esp_logln("Mode multi-connexion: OK");
    free(response);

    // 7. Démarrer le serveur
    status = esp01_send_raw_command_dma("AT+CIPSERVER=1,80", &response, 256, "OK", 2000);
    if (status != ESP01_OK || !response || !strstr(response, "OK"))
    {
        _esp_logln("ERREUR: Démarrage serveur échoué");
        if (response) free(response);
        return false;
    }
    _esp_logln("Serveur web: OK");
    free(response);

    return true;
}
