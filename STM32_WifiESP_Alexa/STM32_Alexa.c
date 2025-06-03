// STM32_Alexa.c
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

	// Réinitialise le tableau des périphériques
	memset(g_devices, 0, sizeof(g_devices));
	g_device_count = 0;

	printf("[ALEXA] Module initialisé, IP: %s\r\n", g_device_ip);
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

	printf("[ALEXA] Périphérique GPIO ajouté: %s (ID: %d)\r\n", name, id);
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
	snprintf(device->uniqueid, sizeof(device->uniqueid), "00:17:88:01:01:%02X:%02X:%02X",
			 id, (unsigned)htim & 0xFF, channel & 0xFF);

	device->type = ALEXA_DEVICE_DIMMABLE;
	device->htim = htim;
	device->channel = channel;
	device->state = 0;
	device->brightness = 0;

	printf("[ALEXA] Périphérique PWM ajouté: %s (ID: %d)\r\n", name, id);
	return id;
}

// Traitement des requêtes Alexa
void alexa_process(void)
{
	// Cette fonction est appelée dans la boucle principale
	// Le traitement des requêtes est géré par les handlers de route
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
			 "<friendlyName>STM32 Alexa Bridge</friendlyName>"
			 "<manufacturer>STM32</manufacturer>"
			 "<manufacturerURL>http://www.st.com/</manufacturerURL>"
			 "<modelDescription>STM32 Alexa Bridge</modelDescription>"
			 "<modelName>STM32 Alexa</modelName>"
			 "<modelNumber>1.0</modelNumber>"
			 "<serialNumber>001788010000</serialNumber>"
			 "<UDN>uuid:2f402f80-da50-11e1-9b23-001788010000</UDN>"
			 "</device>"
			 "</root>",
			 g_device_ip, ALEXA_HTTP_PORT);

	esp01_send_http_response(conn_id, 200, "text/xml", response, strlen(response));
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
}

// Gestionnaire pour les périphériques
static void alexa_handle_lights(int conn_id, const http_parsed_request_t *request)
{
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
	}
	else
	{
		esp01_send_http_response(conn_id, 404, "application/json", "{\"error\":\"Device not found\"}", 26);
	}
}

// Gestionnaire pour le contrôle des périphériques
static void alexa_handle_light_control(int conn_id, const http_parsed_request_t *request)
{
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
	}

	int device_id = atoi(device_id_str) - 1;
	if (device_id < 0 || device_id >= g_device_count)
	{
		esp01_send_http_response(conn_id, 404, "application/json", "{\"error\":\"Device not found\"}", 26);
		return;
	}

	// Parse le corps de la requête pour extraire les commandes
	const char *body_start = strstr(request->headers_buf, "\r\n\r\n");
	if (body_start)
	{
		body_start += 4; // Saute "\r\n\r\n"

		uint8_t new_state = g_devices[device_id].state;
		uint8_t new_brightness = g_devices[device_id].brightness;

		// Cherche la commande "on"
		const char *on_cmd = strstr(body_start, "\"on\":");
		if (on_cmd)
		{
			new_state = (strstr(on_cmd, "true") != NULL) ? 1 : 0;
		}

		// Cherche la commande "bri" (brightness)
		const char *bri_cmd = strstr(body_start, "\"bri\":");
		if (bri_cmd)
		{
			bri_cmd += 6; // Saute "\"bri\":"
			new_brightness = atoi(bri_cmd);
		}

		// Met à jour l'état du périphérique
		alexa_update_device_state(device_id, new_state, new_brightness);

		// Répond avec succès
		char response[256];
		snprintf(response, sizeof(response),
				 "[{\"success\":{\"/lights/%d/state/on\":%s}}]",
				 device_id + 1,
				 new_state ? "true" : "false");

		esp01_send_http_response(conn_id, 200, "application/json", response, strlen(response));
	}
	else
	{
		esp01_send_http_response(conn_id, 400, "application/json", "{\"error\":\"Invalid request\"}", 25);
	}
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

		printf("[ALEXA] Périphérique %s (%d) mis à %s\r\n",
			   device->name, device_id, state ? "ON" : "OFF");
	}
	else if (device->type == ALEXA_DEVICE_DIMMABLE)
	{
		// Contrôle PWM
		if (state == 0)
		{
			// Éteint
			__HAL_TIM_SET_COMPARE(device->htim, device->channel, 0);
			printf("[ALEXA] Périphérique %s (%d) éteint\r\n", device->name, device_id);
		}
		else
		{
			// Calcule la valeur PWM (0-100%) basée sur la luminosité (0-255)
			uint32_t period = __HAL_TIM_GET_AUTORELOAD(device->htim);
			uint32_t pulse = (period * brightness) / 255;
			__HAL_TIM_SET_COMPARE(device->htim, device->channel, pulse);

			printf("[ALEXA] Périphérique %s (%d) réglé à %d%%\r\n",
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
