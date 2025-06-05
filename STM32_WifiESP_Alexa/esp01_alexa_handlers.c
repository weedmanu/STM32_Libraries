#include "STM32_WifiESP_Alexa.h"
#include "alexa_device.h"
#include <stdio.h>
#include <string.h>

// Gestionnaire pour les requêtes de découverte Alexa
void handle_alexa_discovery(int conn_id, const http_parsed_request_t* request) {
    // Cette fonction est appelée lorsqu'Alexa envoie une requête de découverte
    // Elle est déjà implémentée dans alexa_device.c via les routes HTTP
    
    printf("[ALEXA] Requête de découverte reçue\r\n");
    
    // Rediriger vers le gestionnaire approprié
    if (strcmp(request->path, "/setup.xml") == 0) {
        alexa_handle_setup_xml(conn_id, request);
    } else if (strcmp(request->path, "/upnp/control/basicevent1") == 0) {
        alexa_handle_upnp_control(conn_id, request);
    } else if (strcmp(request->path, "/eventservice.xml") == 0) {
        alexa_handle_event_service(conn_id, request);
    } else {
        esp01_send_404_response(conn_id);
    }
}

// Fonction pour extraire la commande Alexa (ON/OFF) des données SOAP
bool extract_alexa_command(const char* soap_data, bool* state) {
    if (!soap_data || !state) {
        return false;
    }
    
    // Chercher la balise BinaryState
    const char* binary_state = strstr(soap_data, "<BinaryState>");
    if (!binary_state) {
        return false;
    }
    
    // Avancer au contenu de la balise
    binary_state += 13; // Longueur de "<BinaryState>"
    
    // Lire la valeur (0 ou 1)
    if (*binary_state == '0') {
        *state = false;
        return true;
    } else if (*binary_state == '1') {
        *state = true;
        return true;
    }
    
    return false;
}