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
	esp01_add_route("/api/lights", alexa_handle_api);
	esp01_add_route("/api/lights/", alexa_handle_api);
	esp01_add_route("/upnp/discovery", alexa_handle_ssdp);

	// Réinitialise le tableau des périphériques
	memset(g_devices, 0, sizeof(g_devices));
	g_device_count = 0;

	alexa_logln("Module initialisé, IP: %s", g_device_ip);
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

// Traitement des requêtes Alexa
void alexa_process(void)
{
	// Cette fonction est appelée dans la boucle principale
	// Le traitement des requêtes est géré par les handlers de route
}

// Gestionnaire pour la découverte SSDP
static void alexa_handle_ssdp(int conn_id, const http_parsed_request_t *request)
{
	char response[512];
	snprintf(response, sizeof(response),
			 "HTTP/1.1 200 OK\r\n"
			 "CACHE-CONTROL: max-age=86400\r\n"
			 "EXT:\r\n"
			 "LOCATION: http://%s:%d/description.xml\r\n"
			 "SERVER: FreeRTOS/9.0 UPnP/1.0 IpBridge/1.17.0\r\n"
			 "hue-bridgeid: 001788FFFE100000\r\n"
			 "ST: urn:schemas-upnp-org:device:basic:1\r\n"
			 "USN: uuid:2f402f80-da50-11e1-9b23-001788010000\r\n\r\n",
			 g_device_ip, ALEXA_HTTP_PORT);

	esp01_send_http_response(conn_id, 200, "text/plain", response, strlen(response));
	alexa_logln("Requête SSDP traitée");
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
			 "<friendlyName>Philips hue</friendlyName>"
			 "<manufacturer>Royal Philips Electronics</manufacturer>"
			 "<manufacturerURL>http://www.philips.com</manufacturerURL>"
			 "<modelDescription>Philips hue Personal Wireless Lighting</modelDescription>"
			 "<modelName>Philips hue bridge 2015</modelName>"
			 "<modelNumber>BSB002</modelNumber>"
			 "<serialNumber>001788102201</serialNumber>"
			 "<UDN>uuid:2f402f80-da50-11e1-9b23-001788010000</UDN>"
			 "</device>"
			 "</root>",
			 g_device_ip, ALEXA_HTTP_PORT);

	esp01_send_http_response(conn_id, 200, "text/xml", response, strlen(response));
	alexa_logln("Description du périphérique envoyée");
}

// Gestionnaire pour l'API Hue
static void alexa_handle_api(int conn_id, const http_parsed_request_t *request)
{
	// Vérifie si c'est une requête pour un périphérique spécifique
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
	// Afficher le chemin complet pour le débogage
	alexa_logln("Chemin de la requête de contrôle: %s", request->path);
	alexa_logln("Méthode: %s", request->method);
	alexa_logln("Headers: %s", request->headers_buf);

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

	int device_id = atoi(device_id_str) - 1;
	if (device_id < 0 || device_id >= g_device_count)
	{
		esp01_send_http_response(conn_id, 404, "application/json", "{\"error\":\"Device not found\"}", 26);
		alexa_logln("Périphérique %d non trouvé pour contrôle", device_id);
		return;
	}

	// Afficher le contenu complet pour le débogage
	alexa_logln("Contenu de la requête: %s", request->headers_buf);

	// Déterminer l'état à partir de la commande
	uint8_t new_state = g_devices[device_id].state; // État par défaut
	uint8_t new_brightness = g_devices[device_id].brightness; // Luminosité par défaut
	uint8_t brightness_changed = 0;
	uint8_t state_changed = 0;

	// Détection des commandes ON/OFF
	if (strstr(request->headers_buf, "on"))
	{
		if (strstr(request->headers_buf, "false"))
		{
			new_state = 0;
			state_changed = 1;
			alexa_logln("Commande OFF détectée");
		}
		else
		{
			new_state = 1;
			state_changed = 1;
			alexa_logln("Commande ON détectée");
		}
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
	alexa_logln("Mise à jour de l'état: device_id=%d, state=%d, brightness=%d", 
		device_id, new_state, new_brightness);
	alexa_update_device_state(device_id, new_state, new_brightness);

	// Générer la réponse avec l'ID du périphérique correct
	char response[256];
	
	// Utiliser sprintf pour inclure l'ID correct du périphérique
	sprintf(response, "[{\"success\":{\"/lights/%d/state/on\":%s}}]", 
		device_id + 1, 
		new_state ? "true" : "false");

	esp01_send_http_response(conn_id, 200, "application/json", response, strlen(response));
	alexa_logln("Contrôle du périphérique %d effectué (état: %d, luminosité: %d)", 
		device_id, new_state, new_brightness);
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

	// Exemple d'annonce SSDP (broadcast UDP sur le port 1900)
	const char *ssdp_msg =
		"NOTIFY * HTTP/1.1\r\n"
		"HOST: 239.255.255.250:1900\r\n"
		"CACHE-CONTROL: max-age=100\r\n"
		"LOCATION: http://%s:%d/description.xml\r\n"
		"SERVER: FreeRTOS/9.0 UPnP/1.0 IpBridge/1.17.0\r\n"
		"hue-bridgeid: 001788FFFE100000\r\n"
		"NT: upnp:rootdevice\r\n"
		"NTS: ssdp:alive\r\n"
		"USN: uuid:2f402f80-da50-11e1-9b23-001788010000::upnp:rootdevice\r\n\r\n";

	char buffer[512];
	snprintf(buffer, sizeof(buffer), ssdp_msg, g_device_ip, ALEXA_HTTP_PORT);

	esp01_send_udp_broadcast(1900, buffer, strlen(buffer));
}
