# STM32_WifiESP Library - Documentation complète

## Vue d'ensemble

La librairie **STM32_WifiESP** est un driver complet pour interfacer un microcontrôleur STM32 avec un module ESP01 (ESP8266) via UART. Elle offre une abstraction haut niveau pour créer facilement des serveurs web avec un système de routage HTTP structuré et robuste.

### Fonctionnalités principales

- **Communication UART/DMA optimisée** : Réception asynchrone avec buffer circulaire
- **Serveur web intégré** : Gestion complète des requêtes HTTP (GET, POST, PUT, DELETE)
- **Système de routage HTTP** : Routes configurables avec handlers personnalisés
- **Parsing robuste des protocoles** : Analyse des trames IPD et requêtes HTTP avec gestion d'erreurs
- **Debug intégré configurable** : Logs détaillés via UART de debug
- **Mode terminal interactif** : Interface pour tester manuellement les commandes AT
- **Gestion des erreurs avancée** : Récupération automatique en cas de corruption de données

## Architecture technique

### Gestion des données avec DMA

La librairie utilise un système de réception DMA circulaire pour optimiser les performances :

```
Buffer DMA circulaire (512 octets par défaut)
┌─────────────────────────────────────────┐
│ [données reçues] [espace libre] [...]   │
└─────────────────────────────────────────┘
     ↑                    ↑
g_rx_last_pos        write_pos (DMA)
```

L'accumulateur global permet de reconstituer les trames fragmentées :
- Taille : `ESP01_DMA_RX_BUF_SIZE * 2` (1024 octets par défaut)
- Gestion automatique de la resynchronisation sur les marqueurs `+IPD`
- Protection contre les débordements

### Flux de traitement des requêtes HTTP

```
Données UART → Buffer DMA → Accumulateur → Parser IPD → Router HTTP → Handler utilisateur
                    ↓              ↓           ↓            ↓
              Détection +IPD   Extraction   Parsing    Réponse HTTP
                              conn_id/len   méthode/path
```

## Configuration

### Macros de configuration principales

```c
// Active/désactive les logs de debug (0 = désactivé, 1 = activé)
#define ESP01_DEBUG 0

// Taille du buffer DMA pour la réception UART (512 octets recommandés)
#define ESP01_DMA_RX_BUF_SIZE 512

// Nombre maximum de routes HTTP simultanées (8 par défaut)
#define ESP01_MAX_ROUTES 8

// Longueur minimale d'un header +IPD pour la validation
#define IPD_HEADER_MIN_LEN 5
```

### Timeouts configurables

```c
#define ESP01_TIMEOUT_SHORT 1000     // Commandes AT rapides (AT, AT+GMR)
#define ESP01_TIMEOUT_MEDIUM 3000    // Commandes standards (AT+CIPSTATUS)
#define ESP01_TIMEOUT_LONG 5000      // Envoi de données (AT+CIPSEND)
#define ESP01_TIMEOUT_WIFI 15000     // Connexion WiFi (AT+CWJAP)
```

## Types et structures

### Énumérations

#### ESP01_Status_t
```c
typedef enum {
    ESP01_OK = 0,              // Opération réussie
    ESP01_FAIL = 1,            // Échec générique
    ESP01_TIMEOUT = 2,         // Timeout dépassé
    ESP01_NOT_INITIALIZED = 3, // Driver non initialisé
    ESP01_BUFFER_TOO_SMALL = 4 // Buffer insuffisant
} ESP01_Status_t;
```

#### ESP01_WifiMode_t
```c
typedef enum {
    ESP01_WIFI_MODE_STA = 1,   // Mode station (client WiFi)
    ESP01_WIFI_MODE_AP = 2,    // Mode point d'accès
    ESP01_WIFI_MODE_STA_AP = 3 // Mode mixte (station + AP)
} ESP01_WifiMode_t;
```

### Structures de données

#### http_request_t - Trame IPD brute
```c
typedef struct {
    int conn_id;          // ID de connexion TCP (0-4)
    int content_length;   // Taille du payload HTTP
    bool is_valid;        // Validation de la trame IPD
    bool is_http_request; // Détection de requête HTTP valide
} http_request_t;
```

#### http_parsed_request_t - Requête HTTP analysée
```c
typedef struct {
    char method[8];         // Méthode HTTP (GET, POST, PUT, DELETE)
    char path[64];          // Chemin de la ressource (/api/led)
    char query_string[128]; // Paramètres GET (?param1=value1&param2=value2)
    bool is_valid;          // Succès du parsing
} http_parsed_request_t;
```

#### esp01_route_t - Route HTTP
```c
typedef struct {
    char path[64];                 // Chemin de la route ("/", "/api/status")
    esp01_route_handler_t handler; // Fonction de traitement
} esp01_route_t;
```

## API détaillée

### Fonctions d'initialisation

#### esp01_init()
```c
ESP01_Status_t esp01_init(UART_HandleTypeDef *huart_esp, 
                         UART_HandleTypeDef *huart_debug,
                         uint8_t *dma_rx_buf, 
                         uint16_t dma_buf_size);
```
**Paramètres :**
- `huart_esp` : Handle UART connecté à l'ESP01
- `huart_debug` : Handle UART pour les logs (peut être NULL)
- `dma_rx_buf` : Buffer DMA pour la réception
- `dma_buf_size` : Taille du buffer DMA

**Retour :** `ESP01_OK` si succès, code d'erreur sinon

**Description :** Initialise le driver, configure le DMA et démarre la réception asynchrone.

### Fonctions de communication AT

#### esp01_send_raw_command_dma()
```c
ESP01_Status_t esp01_send_raw_command_dma(const char *cmd, 
                                         char **response_buffer,
                                         uint32_t max_response_size,
                                         const char *expected_terminator,
                                         uint32_t timeout_ms);
```
**Paramètres :**
- `cmd` : Commande AT à envoyer (sans \r\n)
- `response_buffer` : Pointeur vers buffer de réponse (alloué dynamiquement)
- `max_response_size` : Taille maximale de la réponse
- `expected_terminator` : Chaîne de fin attendue ("OK", "ERROR", etc.)
- `timeout_ms` : Timeout en millisecondes

**Important :** Le buffer de réponse doit être libéré avec `free()` après usage.

### Fonctions WiFi

#### esp01_connect_wifi()
```c
ESP01_Status_t esp01_connect_wifi(const char *ssid, const char *password);
```
**Description :** Connecte l'ESP01 au réseau WiFi spécifié. Attend la confirmation "WIFI GOT IP".

#### esp01_get_current_ip()
```c
ESP01_Status_t esp01_get_current_ip(char *ip_buffer, size_t buffer_size);
```
**Description :** Récupère l'adresse IP actuelle de l'ESP01 via la commande AT+CIFSR.

### Fonctions serveur web

#### esp01_start_web_server()
```c
ESP01_Status_t esp01_start_web_server(uint16_t port);
```
**Description :** Démarre le serveur TCP sur le port spécifié (généralement 80 pour HTTP).

#### esp01_process_requests()
```c
void esp01_process_requests(void);
```
**Description :** **Fonction principale à appeler dans la boucle principale.** Traite les requêtes HTTP entrantes :
1. Lit les nouvelles données DMA
2. Reconstitue les trames IPD complètes
3. Parse les requêtes HTTP
4. Appelle les handlers correspondants

### Fonctions de routage HTTP

#### esp01_add_route()
```c
ESP01_Status_t esp01_add_route(const char *path, esp01_route_handler_t handler);
```
**Exemple :**
```c
esp01_add_route("/api/led", handle_led_control);
esp01_add_route("/", handle_home_page);
```

#### Handler de route
```c
typedef void (*esp01_route_handler_t)(int conn_id, const http_parsed_request_t *request);
```
**Paramètres du handler :**
- `conn_id` : ID de connexion TCP pour la réponse
- `request` : Requête HTTP parsée (méthode, chemin, paramètres)

### Fonctions de réponse HTTP

#### esp01_send_http_response()
```c
ESP01_Status_t esp01_send_http_response(int conn_id, 
                                       int status_code, 
                                       const char *content_type,
                                       const char *body, 
                                       size_t body_len);
```
**Exemple :**
```c
const char *html = "<html><body><h1>Hello ESP01!</h1></body></html>";
esp01_send_http_response(conn_id, 200, "text/html", html, strlen(html));
```

#### esp01_send_json_response()
```c
ESP01_Status_t esp01_send_json_response(int conn_id, const char *json_data);
```
**Exemple :**
```c
esp01_send_json_response(conn_id, "{\"temperature\":23.5,\"humidity\":65}");
```

## Guide d'utilisation pratique

### 1. Initialisation complète

```c
#include "STM32_WifiESP.h"

// Déclaration des ressources
static uint8_t esp01_dma_buffer[ESP01_DMA_RX_BUF_SIZE];
extern UART_HandleTypeDef huart1; // ESP01
extern UART_HandleTypeDef huart2; // Debug

int main(void) {
    // Initialisation HAL...
    
    // Initialisation du driver ESP01
    ESP01_Status_t status = esp01_init(&huart1, &huart2, 
                                      esp01_dma_buffer, 
                                      sizeof(esp01_dma_buffer));
    
    if (status != ESP01_OK) {
        Error_Handler(); // Gestion d'erreur
    }
    
    // Configuration WiFi
    esp01_set_wifi_mode(ESP01_WIFI_MODE_STA);
    
    // Connexion au réseau
    status = esp01_connect_wifi("MonReseauWiFi", "MonMotDePasse");
    if (status != ESP01_OK) {
        Error_Handler();
    }
    
    // Démarrage du serveur web
    status = esp01_start_web_server(80);
    if (status != ESP01_OK) {
        Error_Handler();
    }
    
    // Configuration des routes
    setup_http_routes();
    
    // Boucle principale
    while (1) {
        esp01_process_requests(); // Traitement des requêtes HTTP
        HAL_Delay(10); // Petite pause pour éviter la surcharge CPU
    }
}
```

### 2. Définition des routes HTTP

```c
// Handler pour contrôler une LED
void handle_led_control(int conn_id, const http_parsed_request_t *request) {
    if (strcmp(request->method, "GET") == 0) {
        // Lecture de l'état de la LED
        bool led_state = HAL_GPIO_ReadPin(LED_GPIO_Port, LED_Pin);
        char json_response[64];
        snprintf(json_response, sizeof(json_response), 
                 "{\"led\":\"%s\"}", led_state ? "on" : "off");
        esp01_send_json_response(conn_id, json_response);
    }
    else if (strcmp(request->method, "POST") == 0) {
        // Modification de l'état de la LED
        // Analyse des paramètres dans request->query_string
        if (strstr(request->query_string, "state=on")) {
            HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
        }
        else if (strstr(request->query_string, "state=off")) {
            HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
        }
        esp01_send_json_response(conn_id, "{\"status\":\"ok\"}");
    }
    else {
        esp01_send_http_response(conn_id, 405, "text/plain", 
                                "Method Not Allowed", 18);
    }
}

// Handler pour une API de capteurs
void handle_sensors_api(int conn_id, const http_parsed_request_t *request) {
    if (strcmp(request->method, "GET") == 0) {
        // Lecture des capteurs (exemple)
        float temperature = read_temperature_sensor();
        float humidity = read_humidity_sensor();
        
        char json_response[128];
        snprintf(json_response, sizeof(json_response),
                 "{\"temperature\":%.1f,\"humidity\":%.1f,\"timestamp\":%lu}",
                 temperature, humidity, HAL_GetTick());
        
        esp01_send_json_response(conn_id, json_response);
    }
    else {
        esp01_send_404_response(conn_id);
    }
}

// Page d'accueil avec interface web
void handle_home_page(int conn_id, const http_parsed_request_t *request) {
    const char *html_page = 
        "<!DOCTYPE html>"
        "<html><head><title>ESP01 Server</title></head>"
        "<body>"
        "<h1>STM32 + ESP01 Server</h1>"
        "<p><a href='/api/sensors'>Sensors API</a></p>"
        "<p><a href='/api/led?state=on'>LED ON</a> | "
        "<a href='/api/led?state=off'>LED OFF</a></p>"
        "</body></html>";
    
    esp01_send_http_response(conn_id, 200, "text/html", 
                            html_page, strlen(html_page));
}

// Configuration des routes
void setup_http_routes(void) {
    esp01_clear_routes(); // Nettoie les routes existantes
    
    esp01_add_route("/", handle_home_page);
    esp01_add_route("/api/led", handle_led_control);
    esp01_add_route("/api/sensors", handle_sensors_api);
}
```

### 3. Gestion des erreurs et debug

```c
// Activation du debug (dans STM32_WifiESP.h)
#define ESP01_DEBUG 1

// Fonction de diagnostic
void esp01_diagnostic(void) {
    // Test de communication AT
    ESP01_Status_t status = esp01_test_at(&huart1, &huart2, 
                                         esp01_dma_buffer, 
                                         sizeof(esp01_dma_buffer));
    
    if (status == ESP01_OK) {
        printf("ESP01 : Communication OK\n");
    } else {
        printf("ESP01 : Erreur de communication\n");
        return;
    }
    
    // Vérification du statut WiFi
    status = esp01_get_connection_status();
    if (status == ESP01_OK) {
        char ip[32];
        if (esp01_get_current_ip(ip, sizeof(ip)) == ESP01_OK) {
            printf("WiFi connecté, IP: %s\n", ip);
        }
    } else {
        printf("WiFi non connecté\n");
    }
    
    // Affichage du statut complet
    esp01_print_status(&huart2);
}
```

## Bonnes pratiques

### Gestion mémoire
- **Toujours libérer** les buffers alloués par `esp01_send_raw_command_dma()`
- **Vérifier les retours** de toutes les fonctions d'allocation
- **Dimensionner correctement** les buffers selon l'usage prévu

### Performance
- **Appeler `esp01_process_requests()`** régulièrement dans la boucle principale
- **Éviter les traitements longs** dans les handlers de routes
- **Utiliser des timeouts appropriés** selon le type d'opération

### Fiabilité
- **Implémenter une logique de reconnexion** WiFi automatique
- **Gérer les cas de timeout** et implémenter des retry
- **Valider les données** reçues avant traitement

### Debug
- **Activer ESP01_DEBUG** pendant le développement
- **Utiliser `esp01_print_status()`** pour diagnostiquer les problèmes
- **Monitorer les logs** UART pour identifier les anomalies

## Limitations connues

1. **Connexions simultanées** : L'ESP01 supporte maximum 5 connexions TCP simultanées
2. **Taille des réponses** : Limitée par la mémoire disponible sur le STM32
3. **Requêtes POST** : Le body des requêtes POST n'est pas parsé automatiquement
4. **SSL/HTTPS** : Non supporté par cette version de la librairie

## Exemples d'intégration

### Serveur IoT simple
```c
// Endpoint pour contrôler des relais
void handle_relay_control(int conn_id, const http_parsed_request_t *request) {
    // Parse relay_id et state depuis query_string
    int relay_id = 0;
    bool state = false;
    
    // Parsing simple des paramètres
    char *relay_param = strstr(request->query_string, "relay=");
    char *state_param = strstr(request->query_string, "state=");
    
    if (relay_param) relay_id = atoi(relay_param + 6);
    if (state_param) state = (strncmp(state_param + 6, "on", 2) == 0);
    
    // Contrôle du relais
    control_relay(relay_id, state);
    
    // Réponse JSON
    char response[64];
    snprintf(response, sizeof(response), 
             "{\"relay\":%d,\"state\":\"%s\"}", 
             relay_id, state ? "on" : "off");
    esp01_send_json_response(conn_id, response);
}
```

Cette documentation couvre l'ensemble des fonctionnalités de la librairie STM32_WifiESP. Pour des questions spécifiques ou des cas d'usage avancés, consultez le code source qui contient des commentaires détaillés.