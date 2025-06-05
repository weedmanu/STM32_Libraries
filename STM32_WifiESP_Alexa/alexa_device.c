#include "alexa_device.h"
#include <stdio.h>
#include <string.h>

// Variables globales
static alexa_device_t g_devices[ALEXA_MAX_DEVICES];
static int g_device_count = 0;

// Templates pour les réponses HTTP/SOAP
static const char *SETUP_XML_TEMPLATE =
    "<?xml version=\"1.0\"?>\r\n"
    "<root xmlns=\"urn:Belkin:device-1-0\">\r\n"
    "  <specVersion>\r\n"
    "    <major>1</major>\r\n"
    "    <minor>0</minor>\r\n"
    "  </specVersion>\r\n"
    "  <device>\r\n"
    "    <deviceType>urn:Belkin:device:controllee:1</deviceType>\r\n"
    "    <friendlyName>%s</friendlyName>\r\n"
    "    <manufacturer>Belkin International Inc.</manufacturer>\r\n"
    "    <modelName>Socket</modelName>\r\n"
    "    <modelNumber>1.0</modelNumber>\r\n"
    "    <UDN>uuid:%s</UDN>\r\n"
    "    <serviceList>\r\n"
    "      <service>\r\n"
    "        <serviceType>urn:Belkin:service:basicevent:1</serviceType>\r\n"
    "        <serviceId>urn:Belkin:serviceId:basicevent1</serviceId>\r\n"
    "        <controlURL>/upnp/control/basicevent1</controlURL>\r\n"
    "        <eventSubURL>/upnp/event/basicevent1</eventSubURL>\r\n"
    "        <SCPDURL>/eventservice.xml</SCPDURL>\r\n"
    "      </service>\r\n"
    "    </serviceList>\r\n"
    "  </device>\r\n"
    "</root>\r\n";

static const char *SOAP_RESPONSE_ON =
    "<?xml version=\"1.0\"?>\r\n"
    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
    "  <s:Body>\r\n"
    "    <u:SetBinaryStateResponse xmlns:u=\"urn:Belkin:service:basicevent:1\">\r\n"
    "      <BinaryState>1</BinaryState>\r\n"
    "    </u:SetBinaryStateResponse>\r\n"
    "  </s:Body>\r\n"
    "</s:Envelope>\r\n";

static const char *SOAP_RESPONSE_OFF =
    "<?xml version=\"1.0\"?>\r\n"
    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
    "  <s:Body>\r\n"
    "    <u:SetBinaryStateResponse xmlns:u=\"urn:Belkin:service:basicevent:1\">\r\n"
    "      <BinaryState>0</BinaryState>\r\n"
    "    </u:SetBinaryStateResponse>\r\n"
    "  </s:Body>\r\n"
    "</s:Envelope>\r\n";

static const char *EVENT_SERVICE_XML =
    "<?xml version=\"1.0\"?>\r\n"
    "<scpd xmlns=\"urn:Belkin:service-1-0\">\r\n"
    "  <actionList>\r\n"
    "    <action>\r\n"
    "      <name>SetBinaryState</name>\r\n"
    "      <argumentList>\r\n"
    "        <argument>\r\n"
    "          <name>BinaryState</name>\r\n"
    "          <relatedStateVariable>BinaryState</relatedStateVariable>\r\n"
    "          <direction>in</direction>\r\n"
    "        </argument>\r\n"
    "      </argumentList>\r\n"
    "    </action>\r\n"
    "    <action>\r\n"
    "      <name>GetBinaryState</name>\r\n"
    "      <argumentList>\r\n"
    "        <argument>\r\n"
    "          <name>BinaryState</name>\r\n"
    "          <relatedStateVariable>BinaryState</relatedStateVariable>\r\n"
    "          <direction>out</direction>\r\n"
    "        </argument>\r\n"
    "      </argumentList>\r\n"
    "    </action>\r\n"
    "  </actionList>\r\n"
    "  <serviceStateTable>\r\n"
    "    <stateVariable sendEvents=\"yes\">\r\n"
    "      <name>BinaryState</name>\r\n"
    "      <dataType>Boolean</dataType>\r\n"
    "      <defaultValue>0</defaultValue>\r\n"
    "    </stateVariable>\r\n"
    "  </serviceStateTable>\r\n"
    "</scpd>\r\n";

// Initialisation du module Alexa
void alexa_init(void)
{
    // Initialiser le générateur de nombres aléatoires
    srand(HAL_GetTick());

    // Effacer les routes existantes
    esp01_clear_routes();

    // Ajouter les routes pour Alexa
    esp01_add_route("/setup.xml", alexa_handle_setup_xml);
    esp01_add_route("/upnp/control/basicevent1", alexa_handle_upnp_control);
    esp01_add_route("/eventservice.xml", alexa_handle_event_service);
    esp01_add_route("/", alexa_handle_setup_xml);
    esp01_add_route("/debug", alexa_handle_debug);
}

// Tâche périodique pour Alexa (à appeler régulièrement)
void alexa_task(void)
{
    // Cette fonction est vide car nous utilisons le serveur HTTP existant
}

// Ajouter un appareil Alexa
ESP01_Status_t alexa_add_device(const char *name, GPIO_TypeDef *gpio_port, uint16_t gpio_pin)
{
    if (g_device_count >= ALEXA_MAX_DEVICES)
    {
        return ESP01_FAIL;
    }

    // Configurer le nouvel appareil
    alexa_device_t *device = &g_devices[g_device_count];
    strncpy(device->name, name, ALEXA_DEVICE_NAME_MAX_LEN - 1);
    device->name[ALEXA_DEVICE_NAME_MAX_LEN - 1] = '\0';

    // UUID fixe pour Alexa
    strcpy(device->uuid, "38323636-4558-4dda-9188-cda0e6123456");

    // Configurer le GPIO
    device->gpio_port = gpio_port;
    device->gpio_pin = gpio_pin;
    device->state = false; // État initial: éteint

    g_device_count++;

    return ESP01_OK;
}

// Définir l'état d'un appareil
void alexa_set_device_state(int device_idx, bool state)
{
    if (device_idx < 0 || device_idx >= g_device_count)
    {
        return;
    }

    alexa_device_t *device = &g_devices[device_idx];
    device->state = state;

    // Contrôler le GPIO
    if (state)
    {
        HAL_GPIO_WritePin(device->gpio_port, device->gpio_pin, GPIO_PIN_SET);
        printf("[ALEXA] Appareil %s allumé\r\n", device->name);
    }
    else
    {
        HAL_GPIO_WritePin(device->gpio_port, device->gpio_pin, GPIO_PIN_RESET);
        printf("[ALEXA] Appareil %s éteint\r\n", device->name);
    }
}

// Obtenir l'état d'un appareil
bool alexa_get_device_state(int device_idx)
{
    if (device_idx < 0 || device_idx >= g_device_count)
    {
        return false;
    }

    return g_devices[device_idx].state;
}

// Obtenir l'UUID du premier appareil
const char *alexa_get_first_device_uuid(void)
{
    if (g_device_count > 0)
    {
        return g_devices[0].uuid;
    }
    return NULL;
}

// Obtenir le nom du premier appareil
const char *alexa_get_first_device_name(void)
{
    if (g_device_count > 0)
    {
        return g_devices[0].name;
    }
    return NULL;
}

// Gestionnaire pour /setup.xml
void alexa_handle_setup_xml(int conn_id, const http_parsed_request_t *request)
{
    if (g_device_count == 0)
    {
        esp01_send_404_response(conn_id);
        return;
    }

    // Utiliser le premier appareil configuré
    alexa_device_t *device = &g_devices[0];

    // Générer le XML de configuration
    char setup_xml[1024];
    snprintf(setup_xml, sizeof(setup_xml), SETUP_XML_TEMPLATE,
             device->name, device->uuid);

    // Envoyer la réponse
    esp01_send_http_response(conn_id, 200, "text/xml", setup_xml, strlen(setup_xml));
    printf("[ALEXA] Envoi setup.xml pour %s\r\n", device->name);
}

// Gestionnaire pour /upnp/control/basicevent1
void alexa_handle_upnp_control(int conn_id, const http_parsed_request_t *request)
{
    if (g_device_count == 0)
    {
        esp01_send_404_response(conn_id);
        return;
    }

    // Afficher les headers pour debug
    printf("[ALEXA] Headers reçus: %s\r\n", request->headers_buf);

    // Chercher "SetBinaryState" et la valeur dans les headers
    if (strstr(request->headers_buf, "SetBinaryState") != NULL)
    {
        // Chercher la valeur (0 ou 1)
        const char *state_str = strstr(request->headers_buf, "<BinaryState>");
        if (state_str)
        {
            state_str += 13; // Longueur de "<BinaryState>"
            int state = (*state_str == '1') ? 1 : 0;

            // Mettre à jour l'état de l'appareil
            alexa_set_device_state(0, state);

            // Envoyer la réponse appropriée
            const char *response = state ? SOAP_RESPONSE_ON : SOAP_RESPONSE_OFF;
            esp01_send_http_response(conn_id, 200, "text/xml", response, strlen(response));

            printf("[ALEXA] Commande reçue: %s -> %s\r\n",
                   g_devices[0].name, state ? "ON" : "OFF");
            return;
        }
    }
    
    // Répondre à GetBinaryState
    if (strstr(request->headers_buf, "GetBinaryState") != NULL)
    {
        bool state = alexa_get_device_state(0);
        char response[512];
        snprintf(response, sizeof(response),
                "<?xml version=\"1.0\"?>\r\n"
                "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
                "  <s:Body>\r\n"
                "    <u:GetBinaryStateResponse xmlns:u=\"urn:Belkin:service:basicevent:1\">\r\n"
                "      <BinaryState>%d</BinaryState>\r\n"
                "    </u:GetBinaryStateResponse>\r\n"
                "  </s:Body>\r\n"
                "</s:Envelope>\r\n",
                state ? 1 : 0);
        
        esp01_send_http_response(conn_id, 200, "text/xml", response, strlen(response));
        printf("[ALEXA] État demandé: %s -> %s\r\n", g_devices[0].name, state ? "ON" : "OFF");
        return;
    }

    // Répondre avec un message générique pour les autres commandes
    const char* response = SOAP_RESPONSE_OFF;
    esp01_send_http_response(conn_id, 200, "text/xml", response, strlen(response));
}

// Gestionnaire pour /eventservice.xml
void alexa_handle_event_service(int conn_id, const http_parsed_request_t *request)
{
    esp01_send_http_response(conn_id, 200, "text/xml", EVENT_SERVICE_XML, strlen(EVENT_SERVICE_XML));
    printf("[ALEXA] Envoi eventservice.xml\r\n");
}

// Gestionnaire pour /debug
void alexa_handle_debug(int conn_id, const http_parsed_request_t *request)
{
    char debug_info[512];
    snprintf(debug_info, sizeof(debug_info),
             "<html><body>"
             "<h1>Debug Info</h1>"
             "<p>Device count: %d</p>"
             "<p>Device name: %s</p>"
             "<p>Device UUID: %s</p>"
             "<p>Device state: %s</p>"
             "<p><a href='http://192.168.1.149/setup.xml'>View setup.xml</a></p>"
             "</body></html>",
             g_device_count,
             g_device_count > 0 ? g_devices[0].name : "None",
             g_device_count > 0 ? g_devices[0].uuid : "None",
             g_device_count > 0 ? (g_devices[0].state ? "ON" : "OFF") : "None");
    
    esp01_send_http_response(conn_id, 200, "text/html", debug_info, strlen(debug_info));
}