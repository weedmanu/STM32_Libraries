/*
 * STM32_Alexa.c
 *
 *  Created on: Jun 3, 2025
 *      Author: weedm
 */

#include "STM32_Alexa.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Configuration
#define ALEXA_MAX_DEVICES 8
#define ALEXA_SSDP_PORT 1900
#define ALEXA_HTTP_PORT 80

// Variables globales
static alexa_device_t g_devices[ALEXA_MAX_DEVICES];
static int g_device_count = 0;
static char g_device_ip[32];

// Prototypes des fonctions internes
static void alexa_handle_ssdp(int conn_id, const http_parsed_request_t *request);
static void alexa_handle_description(int conn_id, const http_parsed_request_t *request);
static void alexa_handle_api(int conn_id, const http_parsed_request_t *request);
static void alexa_handle_lights(int conn_id, const http_parsed_request_t *request);
static void alexa_handle_light_control(int conn_id, const http_parsed_request_t *request);
static void alexa_update_device_state(int device_id, uint8_t state, uint8_t brightness);
static int alexa_find_device_by_id(const char *id);

// Initialisation du module Alexa
void alexa_init(void)
{
	// Récupère l'adresse IP
	esp01_get_current_ip(g_device_ip, sizeof(g_device_ip));

	// Enregistre les routes HTTP
	esp01_add_route("/description.xml", alexa_handle_description);
	esp01_add_route("/api", alexa_handle_api);
	esp01_add_route("/api/", alexa_handle_api);
	esp01_add_route("/api/lights", alexa_handle_lights);
	esp01_add_route("/api/lights/", alexa_handle_lights);
	esp01_add_route("/upnp/discovery", alexa_handle_ssdp);
	esp01_add_route("/upnp/control/basicevent1", alexa_handle_ssdp);
	esp01_add_route("/eventservice.xml", alexa_handle_description);
	esp01_add_route("/setup.xml", alexa_handle_description);

	// Ajout de routes pour les contrôles spécifiques
	esp01_add_route("/api/lights/1/state", alexa_handle_light_control);
	esp01_add_route("/api/lights/2/state", alexa_handle_light_control);
	esp01_add_route("/api/lights/3/state", alexa_handle_light_control);
	esp01_add_route("/api/lights/4/state", alexa_handle_light_control);
	esp01_add_route("/api/lights/5/state", alexa_handle_light_control);
	esp01_add_route("/api/lights/6/state", alexa_handle_light_control);
	esp01_add_route("/api/lights/7/state", alexa_handle_light_control);
	esp01_add_route("/api/lights/8/state", alexa_handle_light_control);

	// Réinitialise le tableau des périphériques
	memset(g_devices, 0, sizeof(g_devices));
	g_device_count = 0;

	alexa_logln("Module initialisé, IP: %s", g_device_ip);

	// Envoyer immédiatement des annonces de découverte
	alexa_handle_discovery();
}

// Ajout d'un périphérique GPIO (ON/OFF)
int alexa_add_gpio_device(const char *name, GPIO_TypeDef *port, uint16_t pin)
{
	if (g_device_count >= ALEXA_MAX_DEVICES)
		return -1;

	int id = g_device_count++;
	alexa_device_t *device = &g_devices[id];

	strncpy(device->name, name, sizeof(device->name) - 1);
	snprintf(device->uniqueid, sizeof(device->uniqueid), "00:17:88:01:00:%02X:%02X:%02X",
			 id, (unsigned)port & 0xFF, pin & 0xFF);

	device->type = ALEXA_DEVICE_ONOFF;
	device->gpio_port = port;
	device->gpio_pin = pin;
	device->state = (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET) ? 1 : 0;
	device->brightness = 255;

	alexa_logln("Périphérique GPIO ajouté: %s (ID: %d)", name, id);
	return id;
}

// Ajout d'un périphérique PWM (variable)
int alexa_add_pwm_device(const char *name, TIM_HandleTypeDef *htim, uint32_t channel)
{
	if (g_device_count >= ALEXA_MAX_DEVICES)
		return -1;

	int id = g_device_count++;
	alexa_device_t *device = &g_devices[id];

	strncpy(device->name, name, sizeof(device->name) - 1);
	snprintf(device->uniqueid, sizeof(device->uniqueid), "00:17:88:01:01:%02X:%02X:%02lX",
			 id, (unsigned)htim & 0xFF, channel & 0xFF);

	device->type = ALEXA_DEVICE_DIMMABLE;
	device->htim = htim;
	device->channel = channel;
	device->state = 0;
	device->brightness = 0;

	alexa_logln("Périphérique PWM ajouté: %s (ID: %d)", name, id);
	return id;
}

// Variables pour la gestion des annonces périodiques
static uint32_t g_last_discovery_time = 0;
#define ALEXA_DISCOVERY_INTERVAL 30000 // 30 secondes

// Traitement des requêtes Alexa
void alexa_process(void)
{
	// Vérifier s'il est temps d'envoyer une nouvelle annonce de découverte
	uint32_t current_time = HAL_GetTick();
	if (current_time - g_last_discovery_time > ALEXA_DISCOVERY_INTERVAL)
	{
		alexa_handle_discovery();
		g_last_discovery_time = current_time;
	}
}

// Gestionnaire pour la description du périphérique
static void alexa_handle_description(int conn_id, const http_parsed_request_t *request)
{
	char response[1024];
	snprintf(response, sizeof(response),
			 "<?xml version=\"1.0\"?>"
			 "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">"
			 "<specVersion><major>1</major><minor>0</minor></specVersion>"
			 "<URLBase>http://%s:%d/</URLBase>"
			 "<device>"
			 "<deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>"
			 "<friendlyName>%s</friendlyName>"
			 "<manufacturer>Belkin International Inc.</manufacturer>"
			 "<modelName>Emulated Wemo</modelName>"
			 "<modelNumber>3.1415</modelNumber>"
			 "<UDN>uuid:Socket-1_0-%s</UDN>"
			 "<serviceList>"
			 "<service>"
			 "<serviceType>urn:Belkin:service:basicevent:1</serviceType>"
			 "<serviceId>urn:Belkin:serviceId:basicevent1</serviceId>"
			 "<controlURL>/upnp/control/basicevent1</controlURL>"
			 "<eventSubURL>/upnp/event/basicevent1</eventSubURL>"
			 "<SCPDURL>/eventservice.xml</SCPDURL>"
			 "</service>"
			 "</serviceList>"
			 "</device>"
			 "</root>",
			 g_device_ip, ALEXA_HTTP_PORT,
			 g_devices[0].name,		 // Utilise le nom du premier périphérique
			 g_devices[0].uniqueid); // Utilise l'ID unique du premier périphérique

	esp01_send_http_response(conn_id, 200, "text/xml", response, strlen(response));
	alexa_logln("Description du périphérique envoyée");
}

static void alexa_handle_ssdp(int conn_id, const http_parsed_request_t *request)
{
	alexa_logln("Requête SSDP reçue: %s", request->headers_buf);

	// Vérifier si c'est une requête M-SEARCH
	if (strstr(request->headers_buf, "M-SEARCH") != NULL)
	{
		// Extraire l'adresse IP source et le port de la requête M-SEARCH
		char source_ip[16] = {0};
		uint16_t source_port = 0;

		// Extraire l'IP et le port du client depuis les en-têtes de la requête
		if (request->headers_buf[0] != '\0')
		{
			const char *ip_start = strstr(request->headers_buf, "\"");
			if (ip_start)
			{
				ip_start++; // Sauter le premier guillemet
				const char *ip_end = strstr(ip_start, "\"");
				if (ip_end)
				{
					size_t ip_len = ip_end - ip_start;
					if (ip_len < sizeof(source_ip))
					{
						strncpy(source_ip, ip_start, ip_len);
						source_ip[ip_len] = '\0';

						// Extraire le port
						const char *port_start = strstr(ip_end, ",");
						if (port_start)
						{
							port_start++; // Sauter la virgule
							source_port = atoi(port_start);
						}
					}
				}
			}
		}

		// Si on a réussi à extraire l'IP et le port
		if (source_ip[0] != '\0' && source_port > 0)
		{
			alexa_logln("Envoi réponses M-SEARCH à %s:%d", source_ip, source_port);
			
			char response[512];
			
			// Déterminer le type de recherche (ST)
			const char *st_header = strstr(request->headers_buf, "ST:");
			if (!st_header) st_header = strstr(request->headers_buf, "st:");
			
			if (st_header)
			{
				alexa_logln("ST header trouvé: %s", st_header);
				
				// Réponse pour rootdevice
				if (strstr(st_header, "upnp:rootdevice"))
				{
					snprintf(response, sizeof(response),
							"HTTP/1.1 200 OK\r\n"
							"CACHE-CONTROL: max-age=86400\r\n"
							"DATE: Sat, 01 Jan 2022 00:00:00 GMT\r\n"
							"EXT:\r\n"
							"LOCATION: http://%s:%d/description.xml\r\n"
							"SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
							"ST: upnp:rootdevice\r\n"
							"USN: uuid:Socket-1_0-%s::upnp:rootdevice\r\n\r\n",
							g_device_ip, ALEXA_HTTP_PORT, g_devices[0].uniqueid);
					esp01_send_udp(source_ip, source_port, response, strlen(response));
					alexa_logln("Réponse rootdevice envoyée");
				}
				// Réponse pour device
				else if (strstr(st_header, "urn:Belkin:device") || strstr(st_header, "ssdp:all"))
				{
					snprintf(response, sizeof(response),
							"HTTP/1.1 200 OK\r\n"
							"CACHE-CONTROL: max-age=86400\r\n"
							"DATE: Sat, 01 Jan 2022 00:00:00 GMT\r\n"
							"EXT:\r\n"
							"LOCATION: http://%s:%d/description.xml\r\n"
							"SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
							"ST: urn:Belkin:device:**\r\n"
							"USN: uuid:Socket-1_0-%s::urn:Belkin:device:**\r\n\r\n",
							g_device_ip, ALEXA_HTTP_PORT, g_devices[0].uniqueid);
					esp01_send_udp(source_ip, source_port, response, strlen(response));
					alexa_logln("Réponse device envoyée");
				}
				// Réponse pour UUID
				else if (strstr(st_header, "uuid:"))
				{
					snprintf(response, sizeof(response),
							"HTTP/1.1 200 OK\r\n"
							"CACHE-CONTROL: max-age=86400\r\n"
							"DATE: Sat, 01 Jan 2022 00:00:00 GMT\r\n"
							"EXT:\r\n"
							"LOCATION: http://%s:%d/description.xml\r\n"
							"SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
							"ST: uuid:Socket-1_0-%s\r\n"
							"USN: uuid:Socket-1_0-%s\r\n\r\n",
							g_device_ip, ALEXA_HTTP_PORT, g_devices[0].uniqueid, g_devices[0].uniqueid);
					esp01_send_udp(source_ip, source_port, response, strlen(response));
					alexa_logln("Réponse UUID envoyée");
				}
				// Réponse pour tout autre type de recherche
				else
				{
					// Envoyer toutes les réponses possibles
					// Réponse rootdevice
					snprintf(response, sizeof(response),
							"HTTP/1.1 200 OK\r\n"
							"CACHE-CONTROL: max-age=86400\r\n"
							"DATE: Sat, 01 Jan 2022 00:00:00 GMT\r\n"
							"EXT:\r\n"
							"LOCATION: http://%s:%d/description.xml\r\n"
							"SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
							"ST: upnp:rootdevice\r\n"
							"USN: uuid:Socket-1_0-%s::upnp:rootdevice\r\n\r\n",
							g_device_ip, ALEXA_HTTP_PORT, g_devices[0].uniqueid);
					esp01_send_udp(source_ip, source_port, response, strlen(response));
					HAL_Delay(20);
					
					// Réponse device
					snprintf(response, sizeof(response),
							"HTTP/1.1 200 OK\r\n"
							"CACHE-CONTROL: max-age=86400\r\n"
							"DATE: Sat, 01 Jan 2022 00:00:00 GMT\r\n"
							"EXT:\r\n"
							"LOCATION: http://%s:%d/description.xml\r\n"
							"SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
							"ST: urn:Belkin:device:**\r\n"
							"USN: uuid:Socket-1_0-%s::urn:Belkin:device:**\r\n\r\n",
							g_device_ip, ALEXA_HTTP_PORT, g_devices[0].uniqueid);
					esp01_send_udp(source_ip, source_port, response, strlen(response));
					HAL_Delay(20);
					
					// Réponse UUID
					snprintf(response, sizeof(response),
							"HTTP/1.1 200 OK\r\n"
							"CACHE-CONTROL: max-age=86400\r\n"
							"DATE: Sat, 01 Jan 2022 00:00:00 GMT\r\n"
							"EXT:\r\n"
							"LOCATION: http://%s:%d/description.xml\r\n"
							"SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
							"ST: uuid:Socket-1_0-%s\r\n"
							"USN: uuid:Socket-1_0-%s\r\n\r\n",
							g_device_ip, ALEXA_HTTP_PORT, g_devices[0].uniqueid, g_devices[0].uniqueid);
					esp01_send_udp(source_ip, source_port, response, strlen(response));
					
					alexa_logln("Toutes les réponses envoyées");
				}
			}
			else
			{
				// Si pas de ST header, envoyer toutes les réponses
				// Réponse rootdevice
				snprintf(response, sizeof(response),
						"HTTP/1.1 200 OK\r\n"
						"CACHE-CONTROL: max-age=86400\r\n"
						"DATE: Sat, 01 Jan 2022 00:00:00 GMT\r\n"
						"EXT:\r\n"
						"LOCATION: http://%s:%d/description.xml\r\n"
						"SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
						"ST: upnp:rootdevice\r\n"
						"USN: uuid:Socket-1_0-%s::upnp:rootdevice\r\n\r\n",
						g_device_ip, ALEXA_HTTP_PORT, g_devices[0].uniqueid);
				esp01_send_udp(source_ip, source_port, response, strlen(response));
				HAL_Delay(20);
				
				// Réponse device
				snprintf(response, sizeof(response),
						"HTTP/1.1 200 OK\r\n"
						"CACHE-CONTROL: max-age=86400\r\n"
						"DATE: Sat, 01 Jan 2022 00:00:00 GMT\r\n"
						"EXT:\r\n"
						"LOCATION: http://%s:%d/description.xml\r\n"
						"SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
						"ST: urn:Belkin:device:**\r\n"
						"USN: uuid:Socket-1_0-%s::urn:Belkin:device:**\r\n\r\n",
						g_device_ip, ALEXA_HTTP_PORT, g_devices[0].uniqueid);
				esp01_send_udp(source_ip, source_port, response, strlen(response));
				HAL_Delay(20);
				
				alexa_logln("Réponses par défaut envoyées");
			}
		}

		// Envoyer aussi une annonce de découverte générale
		alexa_handle_discovery();
	}
	else if (strstr(request->path, "/upnp/control/basicevent1") != NULL)
	{
		// Gérer les requêtes de contrôle UPnP
		esp01_send_http_response(conn_id, 200, "text/xml", 
			"<?xml version=\"1.0\"?>\r\n"
			"<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
			"<s:Body>\r\n"
			"<u:SetBinaryStateResponse xmlns:u=\"urn:Belkin:service:basicevent:1\">\r\n"
			"<BinaryState>1</BinaryState>\r\n"
			"</u:SetBinaryStateResponse>\r\n"
			"</s:Body>\r\n"
			"</s:Envelope>\r\n", 290);
		
		alexa_logln("Réponse basicevent envoyée");
	}
	else
	{
		esp01_send_http_response(conn_id, 200, "text/plain", "OK", 2);
	}

	alexa_logln("Requête SSDP traitée");
}

// Gestionnaire pour l'API Hue
static void alexa_handle_api(int conn_id, const http_parsed_request_t *request)
{
	alexa_logln("Requête API reçue: %s %s", request->method, request->path);

	// Si c'est une requête POST sur /api (authentification)
	if (strcmp(request->method, "POST") == 0 && strcmp(request->path, "/api") == 0)
	{
		// Simulation d'une authentification réussie
		char response[256];
		snprintf(response, sizeof(response),
				 "[{\"success\":{\"username\":\"STM32AlexaUser\"}}]");

		esp01_send_http_response(conn_id, 200, "application/json", response, strlen(response));
		alexa_logln("Authentification simulée envoyée");
		return;
	}

	// Vérifie si c'est une requête pour les périphériques
	if (strstr(request->path, "/api/lights"))
	{
		alexa_handle_lights(conn_id, request);
		return;
	}

	// Réponse par défaut (liste des périphériques)
	char response[1024];
	char *ptr = response;
	ptr += sprintf(ptr, "{\"lights\":{");

	for (int i = 0; i < g_device_count; i++)
	{
		ptr += sprintf(ptr, "\"%d\":{\"state\":{\"on\":%s,\"bri\":%d,\"hue\":0,\"sat\":0,\"effect\":\"none\",\"xy\":[0,0],\"ct\":0,\"alert\":\"none\",\"colormode\":\"hs\",\"reachable\":true},\"type\":\"%s\",\"name\":\"%s\",\"modelid\":\"LWB010\",\"manufacturername\":\"Philips\",\"uniqueid\":\"%s\",\"swversion\":\"1.0.0\"}",
					   i + 1,
					   g_devices[i].state ? "true" : "false",
					   g_devices[i].brightness,
					   g_devices[i].type == ALEXA_DEVICE_DIMMABLE ? "Dimmable light" : "On/Off light",
					   g_devices[i].name,
					   g_devices[i].uniqueid);

		if (i < g_device_count - 1)
			ptr += sprintf(ptr, ",");
	}

	ptr += sprintf(ptr, "}}");

	esp01_send_http_response(conn_id, 200, "application/json", response, strlen(response));
	alexa_logln("Liste des périphériques envoyée");
}

// Gestionnaire pour les périphériques
static void alexa_handle_lights(int conn_id, const http_parsed_request_t *request)
{
	// Afficher le chemin complet pour le débogage
	alexa_logln("Chemin de la requête lights: %s", request->path);

	// Si l'URL est exactement "/api/lights" ou "/api/lights/", retourne la liste complète
	if (strcmp(request->path, "/api/lights") == 0 || strcmp(request->path, "/api/lights/") == 0)
	{
		// Réponse avec la liste des périphériques
		char response[1024];
		char *ptr = response;
		ptr += sprintf(ptr, "{");

		for (int i = 0; i < g_device_count; i++)
		{
			ptr += sprintf(ptr, "\"%d\":{\"state\":{\"on\":%s,\"bri\":%d,\"hue\":0,\"sat\":0,\"effect\":\"none\",\"xy\":[0,0],\"ct\":0,\"alert\":\"none\",\"colormode\":\"hs\",\"reachable\":true},\"type\":\"%s\",\"name\":\"%s\",\"modelid\":\"LWB010\",\"manufacturername\":\"Philips\",\"uniqueid\":\"%s\",\"swversion\":\"1.0.0\"}",
						   i + 1,
						   g_devices[i].state ? "true" : "false",
						   g_devices[i].brightness,
						   g_devices[i].type == ALEXA_DEVICE_DIMMABLE ? "Dimmable light" : "On/Off light",
						   g_devices[i].name,
						   g_devices[i].uniqueid);

			if (i < g_device_count - 1)
				ptr += sprintf(ptr, ",");
		}

		ptr += sprintf(ptr, "}");

		esp01_send_http_response(conn_id, 200, "application/json", response, strlen(response));
		alexa_logln("Liste des périphériques envoyée");
		return;
	}

	// Extrait l'ID du périphérique de l'URL
	char device_id_str[8] = {0};
	const char *id_start = strstr(request->path, "/lights/");
	if (id_start)
	{
		id_start += 8; // Longueur de "/lights/"
		const char *id_end = strchr(id_start, '/');
		if (id_end)
		{
			strncpy(device_id_str, id_start, id_end - id_start);
		}
		else
		{
			strcpy(device_id_str, id_start);
		}
	}

	alexa_logln("ID du périphérique extrait: %s", device_id_str);

	// Si c'est une requête PUT pour contrôler un périphérique
	if (strcmp(request->method, "PUT") == 0 && strstr(request->path, "/state"))
	{
		alexa_handle_light_control(conn_id, request);
		return;
	}

	// Sinon, renvoie les informations sur le périphérique
	int device_id = atoi(device_id_str) - 1;
	if (device_id >= 0 && device_id < g_device_count)
	{
		alexa_device_t *device = &g_devices[device_id];

		char response[512];
		snprintf(response, sizeof(response),
				 "{\"state\":{\"on\":%s,\"bri\":%d,\"hue\":0,\"sat\":0,\"effect\":\"none\",\"xy\":[0,0],\"ct\":0,\"alert\":\"none\",\"colormode\":\"hs\",\"reachable\":true},\"type\":\"%s\",\"name\":\"%s\",\"modelid\":\"LWB010\",\"manufacturername\":\"Philips\",\"uniqueid\":\"%s\",\"swversion\":\"1.0.0\"}",
				 device->state ? "true" : "false",
				 device->brightness,
				 device->type == ALEXA_DEVICE_DIMMABLE ? "Dimmable light" : "On/Off light",
				 device->name,
				 device->uniqueid);

		esp01_send_http_response(conn_id, 200, "application/json", response, strlen(response));
		alexa_logln("Informations du périphérique %d envoyées", device_id);
	}
	else
	{
		esp01_send_http_response(conn_id, 404, "application/json", "{\"error\":\"Device not found\"}", 26);
		alexa_logln("Périphérique %d non trouvé", device_id);
	}
}

// Gestionnaire pour le contrôle des périphériques
static void alexa_handle_light_control(int conn_id, const http_parsed_request_t *request)
{
	alexa_logln("=== CONTRÔLE PÉRIPHÉRIQUE ===");
	alexa_logln("Chemin: %s", request->path);
	alexa_logln("Méthode: %s", request->method);
	alexa_logln("Body: %s", request->headers_buf);

	// Extrait l'ID du périphérique de l'URL
	char device_id_str[8] = {0};
	const char *id_start = strstr(request->path, "/lights/");
	if (id_start)
	{
		id_start += 8; // Longueur de "/lights/"
		const char *id_end = strchr(id_start, '/');
		if (id_end)
		{
			strncpy(device_id_str, id_start, id_end - id_start);
		}
		else
		{
			strcpy(device_id_str, id_start);
		}
	}

	alexa_logln("ID extrait: %s", device_id_str);

	int device_id = atoi(device_id_str) - 1;
	if (device_id < 0 || device_id >= g_device_count)
	{
		esp01_send_http_response(conn_id, 404, "application/json", "{\"error\":\"Device not found\"}", 26);
		alexa_logln("Périphérique %d non trouvé", device_id);
		return;
	}

	// Variables pour le nouvel état
	uint8_t new_state = g_devices[device_id].state;
	uint8_t new_brightness = g_devices[device_id].brightness;
	uint8_t state_changed = 0;
	uint8_t brightness_changed = 0;

	// Recherche de la commande ON/OFF dans le body avec différents formats possibles
	if (strstr(request->headers_buf, "\"on\":true") ||
		strstr(request->headers_buf, "{\"on\":true}"))
	{
		new_state = 1;
		state_changed = 1;
		alexa_logln("Commande ON détectée");
	}
	else if (strstr(request->headers_buf, "\"on\":false") ||
			 strstr(request->headers_buf, "{\"on\":false}"))
	{
		new_state = 0;
		state_changed = 1;
		alexa_logln("Commande OFF détectée");
	}

	// Recherche d'une commande de luminosité
	const char *bri_start = strstr(request->headers_buf, "\"bri\":");
	if (bri_start && g_devices[device_id].type == ALEXA_DEVICE_DIMMABLE)
	{
		bri_start += 6; // Longueur de "\"bri\":"
		new_brightness = (uint8_t)atoi(bri_start);
		brightness_changed = 1;

		// Si la luminosité est > 0, on s'assure que l'état est ON
		if (new_brightness > 0)
		{
			new_state = 1;
			state_changed = 1;
		}

		alexa_logln("Commande de luminosité détectée: %d", new_brightness);
	}

	// Mettre à jour l'état du périphérique
	alexa_logln("Mise à jour: device_id=%d, state=%d->%d, brightness=%d->%d",
				device_id, g_devices[device_id].state, new_state,
				g_devices[device_id].brightness, new_brightness);

	alexa_update_device_state(device_id, new_state, new_brightness);

	// CORRIGÉ: Générer une réponse JSON correcte
	char response[512];
	char *ptr = response;
	ptr += sprintf(ptr, "[");

	if (state_changed)
	{
		ptr += sprintf(ptr, "{\"success\":{\"/lights/%d/state/on\":%s}}",
					   device_id + 1, new_state ? "true" : "false");
	}

	if (brightness_changed)
	{
		if (state_changed)
			ptr += sprintf(ptr, ",");
		ptr += sprintf(ptr, "{\"success\":{\"/lights/%d/state/bri\":%d}}",
					   device_id + 1, new_brightness);
	}

	ptr += sprintf(ptr, "]");

	esp01_send_http_response(conn_id, 200, "application/json", response, strlen(response));
	alexa_logln("Réponse de contrôle envoyée: %s", response);
}

// Met à jour l'état d'un périphérique
static void alexa_update_device_state(int device_id, uint8_t state, uint8_t brightness)
{
	if (device_id < 0 || device_id >= g_device_count)
		return;

	alexa_device_t *device = &g_devices[device_id];
	device->state = state;
	device->brightness = brightness;

	// Applique l'état au périphérique physique
	if (device->type == ALEXA_DEVICE_ONOFF)
	{
		// Contrôle GPIO
		HAL_GPIO_WritePin(device->gpio_port, device->gpio_pin,
						  state ? GPIO_PIN_SET : GPIO_PIN_RESET);

		alexa_logln("Périphérique %s (%d) mis à %s",
					device->name, device_id, state ? "ON" : "OFF");
	}
	else if (device->type == ALEXA_DEVICE_DIMMABLE)
	{
		// Contrôle PWM
		if (state == 0)
		{
			// Éteint
			__HAL_TIM_SET_COMPARE(device->htim, device->channel, 0);
			alexa_logln("Périphérique %s (%d) éteint", device->name, device_id);
		}
		else
		{
			// Calcule la valeur PWM (0-100%) basée sur la luminosité (0-255)
			uint32_t period = __HAL_TIM_GET_AUTORELOAD(device->htim);
			uint32_t pulse = (period * brightness) / 255;
			__HAL_TIM_SET_COMPARE(device->htim, device->channel, pulse);

			alexa_logln("Périphérique %s (%d) réglé à %d%%",
						device->name, device_id, (brightness * 100) / 255);
		}
	}
}

// Trouve un périphérique par son ID
static int alexa_find_device_by_id(const char *id)
{
	for (int i = 0; i < g_device_count; i++)
	{
		if (strcmp(g_devices[i].uniqueid, id) == 0)
			return i;
	}
	return -1;
}

// Fonction publique pour la découverte
void alexa_handle_discovery(void)
{
	alexa_logln("Envoi d'une annonce de découverte SSDP");

	// Mise à jour du temps de la dernière annonce
	g_last_discovery_time = HAL_GetTick();
	
	// Envoyer les notifications SSDP
	alexa_send_ssdp_notify();
}

// Fonction pour envoyer les notifications SSDP
void alexa_send_ssdp_notify(void)
{
	char buffer[512];
	
	// Message SSDP pour Alexa (format Wemo - rootdevice)
	const char *ssdp_msg1 =
		"NOTIFY * HTTP/1.1\r\n"
		"HOST: 239.255.255.250:1900\r\n"
		"CACHE-CONTROL: max-age=86400\r\n"
		"LOCATION: http://%s:%d/description.xml\r\n"
		"SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
		"NT: upnp:rootdevice\r\n"
		"NTS: ssdp:alive\r\n"
		"USN: uuid:Socket-1_0-%s::upnp:rootdevice\r\n\r\n";

	snprintf(buffer, sizeof(buffer), ssdp_msg1, g_device_ip, ALEXA_HTTP_PORT, g_devices[0].uniqueid);
	esp01_send_udp_broadcast(1900, buffer, strlen(buffer));
	HAL_Delay(50);

	// Message SSDP pour Alexa (format Wemo - device)
	const char *ssdp_msg2 =
		"NOTIFY * HTTP/1.1\r\n"
		"HOST: 239.255.255.250:1900\r\n"
		"CACHE-CONTROL: max-age=86400\r\n"
		"LOCATION: http://%s:%d/description.xml\r\n"
		"SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
		"NT: urn:Belkin:device:**\r\n"
		"NTS: ssdp:alive\r\n"
		"USN: uuid:Socket-1_0-%s::urn:Belkin:device:**\r\n\r\n";

	snprintf(buffer, sizeof(buffer), ssdp_msg2, g_device_ip, ALEXA_HTTP_PORT, g_devices[0].uniqueid);
	esp01_send_udp_broadcast(1900, buffer, strlen(buffer));
	HAL_Delay(50);
	
	// Message SSDP pour Alexa (format Wemo - UUID)
	const char *ssdp_msg3 =
		"NOTIFY * HTTP/1.1\r\n"
		"HOST: 239.255.255.250:1900\r\n"
		"CACHE-CONTROL: max-age=86400\r\n"
		"LOCATION: http://%s:%d/description.xml\r\n"
		"SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
		"NT: uuid:Socket-1_0-%s\r\n"
		"NTS: ssdp:alive\r\n"
		"USN: uuid:Socket-1_0-%s\r\n\r\n";

	snprintf(buffer, sizeof(buffer), ssdp_msg3, g_device_ip, ALEXA_HTTP_PORT, g_devices[0].uniqueid, g_devices[0].uniqueid);
	esp01_send_udp_broadcast(1900, buffer, strlen(buffer));
	HAL_Delay(50);
	
	// Message SSDP pour Alexa (format Wemo - basicevent)
	const char *ssdp_msg4 =
		"NOTIFY * HTTP/1.1\r\n"
		"HOST: 239.255.255.250:1900\r\n"
		"CACHE-CONTROL: max-age=86400\r\n"
		"LOCATION: http://%s:%d/description.xml\r\n"
		"SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
		"NT: urn:Belkin:service:basicevent:1\r\n"
		"NTS: ssdp:alive\r\n"
		"USN: uuid:Socket-1_0-%s::urn:Belkin:service:basicevent:1\r\n\r\n";

	snprintf(buffer, sizeof(buffer), ssdp_msg4, g_device_ip, ALEXA_HTTP_PORT, g_devices[0].uniqueid);
	esp01_send_udp_broadcast(1900, buffer, strlen(buffer));
}
