#include "udp_ssdp.h"
#include "alexa_device.h"
#include <stdio.h>
#include <string.h>

// Déclaration externe pour accéder à l'UUID du premier appareil
extern const char *alexa_get_first_device_uuid(void);

// Variables globales
static bool g_udp_initialized = false;

// Déclaration externe de l'UART ESP01
extern UART_HandleTypeDef *g_esp_uart;
extern ESP01_Status_t esp01_wait_for_pattern(const char *pattern, uint32_t timeout_ms);

// Commandes AT pour UDP
#define UDP_CONN_ID 0 // ID de connexion UDP

// Initialisation du module UDP pour SSDP
ESP01_Status_t udp_ssdp_init(void)
{
    char cmd[128];
    char resp[256];
    ESP01_Status_t status;

    // Créer une connexion UDP
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=%d,\"UDP\",\"%s\",%d,%d,2", // Open UDP socket on conn ID 0
             UDP_CONN_ID, SSDP_MULTICAST_IP, SSDP_PORT, SSDP_PORT);

    status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_MEDIUM);
    if (status != ESP01_OK)
    {
        printf("[UDP] Erreur lors de la création de la connexion UDP: %s\r\n", resp);
        return status;
    }

    // Activer la réception des paquets multicast
    // snprintf(cmd, sizeof(cmd), "AT+CIPMUX=1");
    // status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), "OK", ESP01_TIMEOUT_MEDIUM);

    g_udp_initialized = true;
    printf("[UDP] Module UDP initialisé pour SSDP\r\n");
    HAL_Delay(100); // Small delay after init

    // Envoyer immédiatement une annonce SSDP pour être découvert
    char ip_buf[32];
    if (esp01_get_current_ip(ip_buf, sizeof(ip_buf)) == ESP01_OK)
    {
        const char *device_uuid = alexa_get_first_device_uuid();
        char uuid[ALEXA_UUID_LEN] = "38323636-4558-4dda-9188-cda0e6123456"; // UUID par défaut
        if (device_uuid)
        {
            strncpy(uuid, device_uuid, sizeof(uuid) - 1);
            uuid[sizeof(uuid) - 1] = '\0';
        }

        char response[512];
        snprintf(response, sizeof(response),
                 "NOTIFY * HTTP/1.1\r\n"
                 "HOST: 239.255.255.250:1900\r\n"
                 "CACHE-CONTROL: max-age=86400\r\n"
                 "LOCATION: http://%s:80/setup.xml\r\n"
                 "NT: upnp:rootdevice\r\n"
                 "NTS: ssdp:alive\r\n"
                 "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
                 "USN: uuid:%s::upnp:rootdevice\r\n"
                 "X-User-Agent: redsonic\r\n\r\n",
                 ip_buf, uuid);

        udp_ssdp_send_response(SSDP_MULTICAST_IP, SSDP_PORT, response);
        printf("[UDP] Annonce SSDP envoyée\r\n");
    }
    return ESP01_OK;
}

// Handle a received UDP packet payload
void udp_ssdp_handle_packet(int conn_id, const char *client_ip, uint16_t client_port, const char *payload, int payload_len)
{
    if (!g_udp_initialized)
    {
        printf("[UDP] Received UDP packet but module not initialized\r\n");
        return;
    }

    // Ensure it's the correct connection ID for SSDP
    if (conn_id != UDP_CONN_ID)
    {
        printf("[UDP] Received UDP packet on unexpected connection ID %d\r\n", conn_id);
        return;
    }

    // Null-terminate the payload for string functions (make a temporary copy)
    char temp_payload[SSDP_MAX_PACKET_SIZE + 1]; // +1 for null terminator
    int copy_len = (payload_len < SSDP_MAX_PACKET_SIZE) ? payload_len : SSDP_MAX_PACKET_SIZE;
    memcpy(temp_payload, payload, copy_len);
    temp_payload[copy_len] = '\0';

    // Vérifier si c'est une requête SSDP M-SEARCH
    if (strstr(temp_payload, "M-SEARCH") && strstr(temp_payload, "ssdp:discover"))
    {
        printf("[UDP] Requête M-SEARCH reçue\r\n");

        // Check the ST (Search Target) header to decide if we should respond
        const char *st_header = strstr(temp_payload, "\r\nST:");
        bool respond_to_st = false;
        if (st_header)
        {
            st_header += 6; // Skip "\r\nST:"

            // Check for specific ST values Alexa might send
            if (strstr(st_header, "upnp:rootdevice") ||
                strstr(st_header, "urn:Belkin:device:**") ||
                strstr(st_header, "ssdp:all"))
            {
                respond_to_st = true;
            }
        }
        else
        {
            // If no ST header, assume it's a general search (ssdp:all)
            respond_to_st = true;
        }

        if (respond_to_st)
        {
            // Générer et envoyer une réponse SSDP
            char ip_buf[32];
            if (esp01_get_current_ip(ip_buf, sizeof(ip_buf)) == ESP01_OK)
            {
                // Récupérer l'UUID du premier appareil
                const char *device_uuid = alexa_get_first_device_uuid();
                char uuid[64] = "38323636-4558-4dda-9188-cda0e6123456"; // UUID par défaut
                if (device_uuid)
                {
                    strncpy(uuid, device_uuid, sizeof(uuid) - 1);
                    uuid[sizeof(uuid) - 1] = '\0';
                }

                char response[512];
                snprintf(response, sizeof(response),
                         "HTTP/1.1 200 OK\r\n"
                         "CACHE-CONTROL: max-age=86400\r\n"
                         "EXT:\r\n"
                         "LOCATION: http://%s:80/setup.xml\r\n"
                         "OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n"
                         "01-NLS: %s\r\n"
                         "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
                         "ST: urn:Belkin:device:**\r\n"
                         "USN: uuid:%s::urn:Belkin:device:**\r\n"
                         "X-User-Agent: redsonic\r\n\r\n",
                         ip_buf, uuid, uuid);

                // Send the response directly to the source IP and port provided in the +IPD header
                if (client_ip && client_port > 0 && strcmp(client_ip, "N/A") != 0)
                {
                    udp_ssdp_send_response(client_ip, client_port, response);
                    printf("[UDP] Réponse SSDP envoyée à %s:%d\r\n", client_ip, client_port);
                }
                else
                {
                    printf("[UDP] Could not send SSDP response, source IP/Port unknown.\r\n");
                }
            }
        }
        else
        {
            printf("[UDP] M-SEARCH received, but ST header does not match supported types.\r\n");
        }
    }
    else
    {
        // Not an M-SEARCH, maybe other UDP traffic? Log it.
        printf("[UDP] Received non-M-SEARCH UDP packet on conn_id %d, len %d:\r\n", conn_id, payload_len);
        // Optionally print payload for debugging specific non-M-SEARCH packets if needed
        // printf("Payload: %.*s\r\n", payload_len, temp_payload);
    }
}

// Envoyer une réponse UDP SSDP
ESP01_Status_t udp_ssdp_send_response(const char *ip, uint16_t port, const char *response)
{
    if (!g_udp_initialized || !ip || !response)
    {
        return ESP01_INVALID_PARAM;
    }

    char cmd[64];
    char resp[128];
    ESP01_Status_t status;

    // Envoyer les données UDP en spécifiant l'IP et le port de destination
    int len = strlen(response);
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d,%d,\"%s\",%d", UDP_CONN_ID, len, ip, port); // Use CIPSEND with IP and Port

    status = esp01_send_raw_command_dma(cmd, resp, sizeof(resp), ">", ESP01_TIMEOUT_SHORT);
    if (status != ESP01_OK)
    {
        printf("[UDP] Erreur lors de la préparation de l'envoi UDP: %s\r\n", resp);
        return status;
    }

    // Envoyer le contenu
    HAL_UART_Transmit(g_esp_uart, (uint8_t *)response, len, HAL_MAX_DELAY);

    // Attendre la confirmation
    status = esp01_wait_for_pattern("SEND OK", ESP01_TIMEOUT_MEDIUM);
    if (status != ESP01_OK)
    {
        printf("[UDP] Erreur lors de l'envoi UDP\r\n");
        return status;
    }

    return ESP01_OK;
}