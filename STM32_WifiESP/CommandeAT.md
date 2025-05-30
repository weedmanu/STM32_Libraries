# Commandes AT utiles pour ESP01 (ESP8266)

## Commandes réseau WiFi

| Commande AT                        | Description                                      | Retour attendu (exemple)                |
|-------------------------------------|--------------------------------------------------|-----------------------------------------|
| `AT`                               | Test de communication                            | `OK`                                    |
| `AT+GMR`                           | Version du firmware                              | `AT version:...` + `OK`                 |
| `AT+RST`                           | Redémarrage du module                            | `OK` puis `ready`                       |
| `AT+CWMODE?`                       | Lire le mode WiFi                                | `+CWMODE:<mode>` + `OK`                 |
| `AT+CWMODE=1`                      | Mode station (client WiFi)                       | `OK`                                    |
| `AT+CWLAP`                         | Liste des réseaux WiFi disponibles               | Liste des réseaux + `OK`                |
| `AT+CWJAP?`                        | Affiche le réseau WiFi connecté                  | `+CWJAP:"SSID"` + `OK` ou `No AP`       |
| `AT+CWJAP="SSID","PWD"`            | Connexion à un réseau WiFi                       | `WIFI CONNECTED` + `WIFI GOT IP` + `OK` |
| `AT+CWQAP`                         | Déconnexion du WiFi                              | `OK`                                    |
| `AT+CIFSR`                         | Affiche l’adresse IP actuelle                    | `+CIFSR:STAIP,"192.168.x.x"` + `OK`     |
| `AT+CIPSTA?`                       | Affiche l’IP en mode STA                         | `+CIPSTA:ip:"192.168.x.x"` + `OK`       |
| `AT+CWDHCP=1,1`                    | Active le DHCP client (STA)                      | `OK`                                    |
| `AT+CWDHCP=0,1`                    | Désactive le DHCP client (STA)                   | `OK`                                    |
| `AT+CWDHCP=2,1`                    | Active le DHCP serveur (AP)                      | `OK`                                    |
| `AT+CWDHCP=0,2`                    | Désactive le DHCP serveur (AP)                   | `OK`                                    |
| `AT+CIPSTATUS`                     | Statut des connexions réseau                     | `STATUS:<n>` + liste + `OK`             |

## Commandes serveur TCP/HTTP

| Commande AT                        | Description                                      | Retour attendu (exemple)                |
|-------------------------------------|--------------------------------------------------|-----------------------------------------|
| `AT+CIPMUX=1`                      | Active le mode multi-connexion                   | `OK`                                    |
| `AT+CIPSERVER=1,80`                | Démarre le serveur TCP sur le port 80            | `OK` ou `no change`                     |
| `AT+CIPSERVER=0`                   | Arrête le serveur TCP                            | `OK`                                    |
| `AT+CIPDINFO=1`                    | Affiche l’IP du client dans les trames +IPD      | `OK`                                    |
| `AT+CIPSTO=60`                     | Timeout de connexion TCP (en secondes)           | `OK`                                    |
| `AT+CIPSTART="TCP","host",port`    | Ouvre une connexion TCP (client)                 | `OK` puis `CONNECT`                     |
| `AT+CIPSEND=<id>,<len>`            | Envoie des données sur la connexion `<id>`       | `>` puis `SEND OK`                      |
| `AT+CIPCLOSE=<id>`                 | Ferme la connexion TCP `<id>`                    | `CLOSED` + `OK`                         |

---

## Exemples d’utilisation

```text
AT
OK

AT+GMR
AT version:1.7.4.0(...)
OK

AT+CWMODE=1
OK

AT+CWJAP="MonSSID","MonMotDePasse"
WIFI CONNECTED
WIFI GOT IP
OK

AT+CIFSR
+CIFSR:STAIP,"192.168.1.42"
OK

AT+CIPMUX=1
OK

AT+CIPSERVER=1,80
OK