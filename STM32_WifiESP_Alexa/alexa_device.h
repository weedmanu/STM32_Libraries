#ifndef INC_ALEXA_DEVICE_H_
#define INC_ALEXA_DEVICE_H_

#include "main.h"
#include "STM32_WifiESP_Alexa.h"

// Définitions pour le protocole Alexa/SSDP
#define ALEXA_UDP_PORT 1900
#define ALEXA_MAX_DEVICES 1
#define ALEXA_DEVICE_NAME_MAX_LEN 32
#define ALEXA_UUID_LEN 37  // 36 caractères + null terminator

// Structure pour un appareil Alexa
typedef struct {
    char name[ALEXA_DEVICE_NAME_MAX_LEN];
    char uuid[ALEXA_UUID_LEN];
    bool state;
    GPIO_TypeDef* gpio_port;
    uint16_t gpio_pin;
} alexa_device_t;

// Fonctions publiques
void alexa_init(void);
void alexa_task(void);
ESP01_Status_t alexa_add_device(const char* name, GPIO_TypeDef* gpio_port, uint16_t gpio_pin);
void alexa_set_device_state(int device_idx, bool state);
bool alexa_get_device_state(int device_idx);
const char* alexa_get_first_device_uuid(void);
const char* alexa_get_first_device_name(void);

// Gestionnaires de routes HTTP pour Alexa
void alexa_handle_setup_xml(int conn_id, const http_parsed_request_t* request);
void alexa_handle_upnp_control(int conn_id, const http_parsed_request_t* request);
void alexa_handle_event_service(int conn_id, const http_parsed_request_t* request);
void alexa_handle_debug(int conn_id, const http_parsed_request_t* request);

#endif /* INC_ALEXA_DEVICE_H_ */