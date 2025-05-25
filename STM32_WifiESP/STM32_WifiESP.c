/*
 * STM32_WifiESP.c
 * Driver STM32 <-> ESP01 (AT)
 * Version structurée avec sous-fonctions pour chaque étape
 * 2025 - weedm
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
                                          uint32_t max_response_size, const char *expected_terminator,
                                          uint32_t timeout_ms)
{
    if (!g_esp_uart || !cmd || !response_buffer)
    {
        _esp_logln("ESP01 Send Error: Invalid params");
        if (response_buffer)
            *response_buffer = NULL;
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
bool init_esp01_serveur_web(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug, uint8_t *dma_rx_buf, uint16_t dma_buf_size, const char *ssid, const char *password)
{
    char *response = NULL;
    ESP01_Status_t status;

    // 1. Initialiser le driver
    if (esp01_init(huart_esp, huart_debug, dma_rx_buf, dma_buf_size) != ESP01_OK)
        return false;
    HAL_Delay(1000); // Ajouté

    // 2. Test AT
    status = esp01_send_raw_command_dma("AT", &response, 256, "OK", 2000);
    if (status != ESP01_OK || !response || !strstr(response, "OK"))
    {
        _esp_logln("ERREUR: Test AT échoué");
        if (response)
            free(response);
        return false;
    }
    _esp_logln("Test AT: OK");
    free(response);
    HAL_Delay(1000); // Ajouté

    // 3. Mode station
    status = esp01_send_raw_command_dma("AT+CWMODE=1", &response, 256, "OK", 5000);
    if (status != ESP01_OK || !response || !strstr(response, "OK"))
    {
        _esp_logln("ERREUR: Mode station échoué");
        if (response)
            free(response);
        return false;
    }
    _esp_logln("Mode station: OK");
    free(response);
    HAL_Delay(1000); // Ajouté

    // 4. Connexion WiFi
    char wifi_cmd[512];
    snprintf(wifi_cmd, sizeof(wifi_cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
    status = esp01_send_raw_command_dma(wifi_cmd, &response, 512, "WIFI GOT IP", 15000);
    if (status != ESP01_OK || !response || !strstr(response, "WIFI GOT IP"))
    {
        _esp_logln("ERREUR: Connexion WiFi échouée");
        if (response)
            free(response);
        return false;
    }
    _esp_logln("Connexion WiFi: OK");
    free(response);
    HAL_Delay(1000); // Ajouté

    // 5. Mode multi-connexion
    status = esp01_send_raw_command_dma("AT+CIPMUX=1", &response, 512, "OK", 5000);
    if (status != ESP01_OK || !response || !strstr(response, "OK"))
    {
        _esp_logln("ERREUR: Mode multi-connexion échoué");
        if (response)
            free(response);
        return false;
    }
    _esp_logln("Mode multi-connexion: OK");
    free(response);
    HAL_Delay(1000); // Ajouté

    // 6. Démarrer le serveur
    status = esp01_send_raw_command_dma("AT+CIPSERVER=1,80", &response, 512, "OK", 2000);
    if (status != ESP01_OK && (!response || (!strstr(response, "OK") && !strstr(response, "no change"))))
    {
        _esp_logln("Echec démarrage serveur, tentative d'arrêt puis redémarrage...");
        if (response)
        {
            free(response);
            response = NULL;
        }
        // Tenter d'arrêter le serveur
        status = esp01_send_raw_command_dma("AT+CIPSERVER=0", &response, 256, "OK", 2000);
        if (response)
        {
            free(response);
            response = NULL;
        }
        HAL_Delay(1500); // <-- Mets un délai plus long ici
        // Retenter de démarrer le serveur
        status = esp01_send_raw_command_dma("AT+CIPSERVER=1,80", &response, 256, "OK", 2000);
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
    HAL_Delay(1000); // Ajouté

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

// Fonction corrigée pour vider complètement le buffer avant CIPSEND
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
            // Ajout : si il reste très peu à lire, tente de forcer la lecture
            if (remaining <= 2)
            {
                uint16_t extra = esp01_get_new_data((uint8_t *)discard_buf, remaining);
                if (extra > 0)
                    remaining -= extra;
            }
            HAL_Delay(10);
        }
    }

    // Vider complètement le buffer DMA et l'accumulateur
    HAL_Delay(100);
    char temp_buf[ESP01_DMA_RX_BUF_SIZE];
    int flush_count = 0;
    // Nouvelle boucle : on force la lecture de tous les octets restants
    while (remaining > 0 && flush_count < 10)
    {
        uint16_t extra = esp01_get_new_data((uint8_t *)temp_buf, remaining);
        if (extra > 0)
        {
            remaining -= extra;
            if (ESP01_DEBUG)
            {
                char debug_msg[100];
                snprintf(debug_msg, sizeof(debug_msg), "Flush final: ignoré %u octets, reste %d", (unsigned int)extra, remaining);
                _esp_logln(debug_msg);
            }
        }
        else
        {
            HAL_Delay(10);
        }
        flush_count++;
    }

    // Dernière chance : vider tous les octets restants, même s'il n'en reste qu'un
    if (remaining > 0)
    {
        char temp_buf[1];
        uint16_t extra = esp01_get_new_data((uint8_t *)temp_buf, 1);
        if (extra > 0)
        {
            remaining -= extra;
            if (ESP01_DEBUG)
            {
                char debug_msg[100];
                snprintf(debug_msg, sizeof(debug_msg), "Flush ultime: ignoré %u octet, reste %d", (unsigned int)extra, remaining);
                _esp_logln(debug_msg);
            }
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
    snprintf(cipsend_cmd, sizeof(cipsend_cmd), "AT+CIPSEND=%d,%d\r\n", conn_id, page_len);

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
}

// Fonction esp01_server_handle corrigée avec gestion des requêtes complètes
void esp01_server_handle(void)
{
    char rx_buffer[ESP01_DMA_RX_BUF_SIZE];
    char path[32] = "/";
    extern char page[];
    extern int page_len;

    uint16_t data_len = 0;
    int max_tries = 100; // timeout ~1s
    bool found = false;
    do
    {
        data_len = esp01_get_new_data((uint8_t *)rx_buffer, sizeof(rx_buffer));
        if (data_len > 0 && strstr(rx_buffer, "+IPD,") && (strstr(rx_buffer, "GET ") || strstr(rx_buffer, "POST ")))
        {
            found = true;
            break;
        }
        HAL_Delay(10);
    } while (--max_tries);

    if (found)
    {
        printf("DEBUG RX BUFFER: '%.*s'\r\n", data_len, rx_buffer);

        http_request_t req = parse_ipd_header(rx_buffer);

        // Cherche le début du payload HTTP après le ':'
        char *payload = strstr(rx_buffer, ":");
        if (payload)
            payload++;
        else
            payload = rx_buffer;
        extract_http_path(payload, path, sizeof(path));

        esp01_server_page_with_connid(req.conn_id, path, page, page_len);
    }
}

ESP01_Status_t esp01_get_current_ip(char *ip_buf, size_t buf_len)
{
    if (!ip_buf || buf_len < 8)
        return ESP01_INVALID_RESPONSE;

    char *resp = NULL;
    ESP01_Status_t st = esp01_send_raw_command_dma("AT+CIFSR", &resp, 256, "OK", 2000);
    if (st == ESP01_OK && resp)
    {
        char *staip = strstr(resp, "+CIFSR:STAIP,\"");
        if (staip)
        {
            staip += strlen("+CIFSR:STAIP,\"");
            char *end = strchr(staip, '"');
            if (end && (size_t)(end - staip) < buf_len)
            {
                strncpy(ip_buf, staip, end - staip);
                ip_buf[end - staip] = '\0';
                free(resp);
                return ESP01_OK;
            }
        }
        free(resp);
    }
    strncpy(ip_buf, "N/A", buf_len);
    return ESP01_ERROR_AT;
}

void esp01_server_page(const char *path, const char *page, size_t page_len)
{
    esp01_server_page_with_connid(0, path, page, page_len);
}

void extract_http_path(const char *http_data, char *path_buf, size_t buf_size)
{
    // Affiche les 80 premiers caractères pour debug
    printf("DEBUG extract_http_path: '%.*s'\r\n", 80, http_data);

    // Recherche la première ligne qui commence par GET ou POST
    const char *p = http_data;
    while (*p)
    {
        // Passe les caractères de début de ligne
        while (*p == '\r' || *p == '\n')
            p++;
        if (strncmp(p, "GET ", 4) == 0 || strncmp(p, "POST ", 5) == 0)
        {
            // On a trouvé la ligne HTTP
            const char *get = p + ((p[0] == 'G') ? 4 : 5);
            const char *space = strchr(get, ' ');
            if (!space)
                break;
            size_t len = (size_t)(space - get);
            if (len >= buf_size)
                len = buf_size - 1;
            strncpy(path_buf, get, len);
            path_buf[len] = '\0';
            // Enlève le slash final si présent
            len = strlen(path_buf);
            if (len > 1 && path_buf[len - 1] == '/')
                path_buf[len - 1] = '\0';
            return;
        }
        // Passe à la ligne suivante
        while (*p && *p != '\n')
            p++;
        if (*p == '\n')
            p++;
    }
    // Si rien trouvé
    strncpy(path_buf, "/", buf_size);
    path_buf[buf_size - 1] = 0;
}

void esp01_add_route(const char *path, esp01_route_callback_t callback)
{
    if (g_route_count < ESP01_MAX_ROUTES)
    {
        g_routes[g_route_count].path = path;
        g_routes[g_route_count].callback = callback;
        g_route_count++;
    }
}

void esp01_server_page_with_connid(int conn_id, const char *path, const char *page, size_t page_len)
{
    printf("DEBUG: path reçu = '%s'\r\n", path);

    // Enlève le slash final si présent (hors racine)
    size_t len = strlen(path);
    char clean_path[32];
    strncpy(clean_path, path, sizeof(clean_path));
    clean_path[sizeof(clean_path) - 1] = 0;
    if (len > 1 && clean_path[len - 1] == '/')
        clean_path[len - 1] = '\0';

    for (int i = 0; i < g_route_count; ++i)
    {
        if (strcmp(clean_path, g_routes[i].path) == 0)
        {
            g_routes[i].callback(conn_id);
            return;
        }
    }
    handle_http_request(conn_id, page, page_len);
}
