#ifndef INC_UDP_SSDP_H_
#define INC_UDP_SSDP_H_

#include "main.h"
#include "STM32_WifiESP_Alexa.h"

// Définitions pour SSDP
#define SSDP_PORT 1900						// Port standard pour SSDP
#define SSDP_MULTICAST_IP "239.255.255.250" // Adresse multicast standard pour SSDP
#define SSDP_MAX_PACKET_SIZE 512			// Taille maximale d'un paquet SSDP

// Fonctions publiques
ESP01_Status_t udp_ssdp_init(void);
ESP01_Status_t udp_ssdp_send_response(const char *ip, uint16_t port, const char *response);
void udp_ssdp_handle_packet(int conn_id, const char *client_ip, uint16_t client_port, const char *payload, int payload_len);

#endif /* INC_UDP_SSDP_H_ */