// Gestionaire de carte ESP32 par Expressif Systems Ver 3.3.11
#include <WiFi.h>              // Bibliothèque WiFi standard pour ESP32 (gère la connexion et le serveur)
#include <ESPmDNS.h>           // Bibliothèque pour gerer un serveur Local
#include "arduino_secrets.h"   // Fichier séparé contenant le nom du réseau (SSID) et le mot de passe WiFi

// --- 🖥️ Écran OLED I2C 0,96" (SSD1306) -----------------------------------
// Bibliothèques à installer via le Gestionnaire de bibliothèques Arduino :
// "Adafruit SSD1306" + "Adafruit GFX Library" (dépendances installées automatiquement)
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH   128     // Largeur de l'écran OLED en pixels
#define SCREEN_HEIGHT  64      // Hauteur de l'écran OLED en pixels (mettre 32 si vous avez un modèle 128x32)
#define OLED_RESET     -1      // Pas de broche RESET dédiée (partagée avec le reset de l'ESP32)
#define OLED_ADRESSE   0x3C    // Adresse I2C la plus courante (0x3D sur certains modules, à tester si l'écran reste noir)

// Broches I2C par défaut de l'ESP32 Dev Module
#define OLED_SDA 21
#define OLED_SCL 22

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// 📶 Multi-SSID : structure associant un réseau WiFi (SSID + mot de passe)
struct ReseauWiFi {
  const char* ssid;
  const char* pass;
};

// 📶 Liste des réseaux WiFi connus (box, répéteur ESP_Nat_Router, etc.)
// Les identifiants SECRET_SSIDx / SECRET_PASSx doivent être définis dans arduino_secrets.h
// Vous pouvez ajouter ou retirer des lignes selon le nombre de réseaux disponibles.
const ReseauWiFi RESEAUX_WIFI[] = {
  { SECRET_SSID,  SECRET_PASS  },   // Réseau 1 : votre box internet
  { SECRET_SSID2, SECRET_PASS2 },   // Réseau 2 : ex. "ESP_Nat_Router"
  // { SECRET_SSID3, SECRET_PASS3 }, // Réseau 3 (optionnel) : décommentez pour en ajouter un autre et voir arduino_secrets.h
};

// Calcule automatiquement le nombre de réseaux connus déclarés ci-dessus
const byte NB_RESEAUX_WIFI = sizeof(RESEAUX_WIFI) / sizeof(RESEAUX_WIFI[0]);

int status = WL_IDLE_STATUS;     // Variable qui mémorise l'état de la connexion WiFi (au repos au départ)
//bool serialReady = false;        // Variable Attend que le moniteur série soit ouvert

// 🔴 Repere 1
//⚠️---------------(Adapté a ESP32-Wroom-DA Module  -> ✅ ESP-32D Dev Module---------------------------
// (❌ NE PAS UTILISER  0, 6, 7, 8, 9, 10, 11, 15)  Non Présent (20, 24, 28, 29, 30, 31, 37, 38 )
// GPIO Utilisable( 1, 2, 3, 4, 5, 12, 13, 14, 16, 17, 18, 19, I2c 21, I2c 22, 23, 25, 26, 27, 32, 33 -- 2 led bleue) ⚠️ en entrée seul 34, 35, 36, 39 👍
//GPIO Utilisé
// 21 et 22 pour SDA et SCL (SCK)
// Relais Rel-K1-> GPIO32 - Rel-K2-> GPIO33 - Rel-K3-> GPIO25 - Rel-K4-> GPIO26
//Les Boutons poussoir 14->BP1 Rel-K1 (32) - 16->BP2 Rel-K2 (33) - 17->BP Rel-K3 (25)- 18->BP Rel-K4 (26)
const char* NOM_PANNEAU_CENTRAL = "Serveur Local"; // Nom du panneau affiché sur la page HTML LIGNE ⚫⚪⚫
const char* MA_CARTE = "Module Relais 4 Canaux";// 🟡🟢

// 🔴 Repere 2
// Structure associant chaque broche GPIO à son nom affiché : plus besoin de synchroniser
// deux tableaux séparés (PINS[] et NOMS[]) — pin et nom sont liés sur la même ligne.
// valeurs fixe sur la carte ESP32 4 Relais  32 33 25 26
struct Sortie {
  byte pin;
  const char* nom;
};
// Pilotage des 4 relais avec GPIO et Nom choisi
const Sortie SORTIES[] = {
  {32, "Relais K1"},
  {33, "Relais K2"},
  {25, "Relais K3"},
  {26, "Relais K4"}
};

// Calcule automatiquement le nombre de sorties à partir de SORTIES[]
const byte NB_SORTIES = sizeof(SORTIES) / sizeof(SORTIES[0]);

// 🔴 Repere 2b
// Structure associant chaque bouton poussoir (BP) à son relais associé.
// Les BP sont câblés en INPUT_PULLUP (donc entre la broche et la masse GND) :
// au repos la broche lit HIGH, et passe à LOW lorsque le bouton est appuyé.
// Chaque appui inverse (bascule) l'état du relais correspondant.
struct Bouton {
  byte pinBP;                     // GPIO du bouton poussoir
  byte pinRelais;                 // GPIO du relais piloté par ce bouton
  bool etatStable;                // Dernier état stable retenu (anti-rebond)
  bool derniereLecture;           // Dernière lecture brute de la broche
  unsigned long dernierChangement; // Horodatage du dernier changement détecté (anti-rebond)
};
// 🔴 Repere 2c
// Association Bouton -> Relais : BP14->GPIO 32, BP16->GPIO 33, BP17->GPIO 25, BP18->GPIO 26

Bouton BOUTONS[] = {
  {14, 32, HIGH, HIGH, 0},   // BP GPIO14 -> Relais K1 GPIO32
  {16, 33, HIGH, HIGH, 0},   // BP GPIO16 -> Relais K2 GPIO33
  {17, 25, HIGH, HIGH, 0},   // BP GPIO17 -> Relais K3 GPIO25
  {18, 26, HIGH, HIGH, 0}    // BP GPIO18 -> Relais K4 GPIO26
};
// Calcule automatiquement le nombre de boutons à partir de BOUTONS[]
const byte NB_BOUTONS = sizeof(BOUTONS) / sizeof(BOUTONS[0]);

// Délai anti-rebond (debounce) en millisecondes pour les boutons poussoir
const unsigned long DEBOUNCE_BP_MS = 50;

//⚠️-------------Nom du serveur Local----------
const char* HOSTNAME_MDNS = "RichardV"; // Changez le nom ici si besoin (ex: "esp32.local")

WiFiServer server(80);           // Crée un serveur web qui écoute sur le port 80 (port HTTP standard)

// --- Protection anti-blocage sur la lecture client ---
const unsigned long TIMEOUT_CLIENT_MS = 2000;   // Délai max (ms) accordé à un client pour envoyer sa requête complète
const int TAILLE_MAX_LIGNE = 200;               // Taille max (caractères) tolérée pour une ligne de requête HTTP

// --- Chronomètre pour reconnexion WiFi non-bloquante ---
unsigned long dernierEssaiWiFi = 0;             // Mémorise le moment du dernier essai de connexion WiFi
const unsigned long INTERVALLE_WIFI_MS = 10000; // Délai d'attente minimal (10s) entre chaque tentative de reconnexion

// Déclarations des fonctions
void traiterCommande(const String &req);      // analyse une ligne de requête HTTP reçue (par référence constante pour éviter la copie)
void envoyerPage(WiFiClient &client);         // envoie la page HTML complète au navigateur
void envoyerEtat(WiFiClient &client);         // envoie juste l'état des sorties (pour l'AJAX)
void afficherInfosWiFi();                     // affiche les informations WiFi
void afficherReseauxDisponibles();            // 📶 scanne et liste tous les réseaux WiFi détectés avec leur qualité
void genererTableauJSPins(WiFiClient &client);// génère la ligne JS "let pins=[...]" à partir de SORTIES[]
void verifierWiFi();                          // Vérifie et rétablit la connexion WiFi si besoin
bool connecterMeilleurWiFi();                 // 📶 Prototype : scanne les réseaux connus et se connecte au meilleur signal
String genererCasesHTML();                    // construit en une seule fois le HTML de toutes les "cases"
void gererBoutons();                          // lecture des boutons poussoir et bascule les relais associés (avec anti-rebond)
void afficherEcranOLED();                     // 🖥️ Affiche le SSID, l'IP et l'état des relais sur l'écran OLED

// --- 🖥️ Tempo pour rafraîchissement non-bloquant de l'écran OLED ---
unsigned long dernierAffichageOLED = 0;
const unsigned long INTERVALLE_OLED_MS = 1000; // Rafraîchit l'écran toutes les "1 " seconde

//============================================================
// SETUP
//============================================================
void setup() {

  Serial.begin(115200);          // Démarre la liaison série à 115200 bauds pour les messages de débogage

  // --- 🖥️ Initialisation de l'écran OLED (bus I2C sur SDA=21 / SCL=22) ---
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADRESSE)) {
    Serial.println(F("Erreur : écran OLED non détecté (vérifier câblage/adresse I2C)"));
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("Demarrage..."));
    display.display();
  }
     // Remonte Infos carte
  Serial.println("\n--- SPÉCIFICATIONS ESP32 ---");
  Serial.printf("Modèle de puce  : %s\n", ESP.getChipModel());
  Serial.printf("Nb de Cœurs CPU : %d\n", ESP.getChipCores());
  Serial.printf("Fréquence CPU   : %d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Fréquence Flash : %d MHz\n", ESP.getFlashChipSpeed() / 1000000); // au lieu de / (1024 * 1024)) moins précis;
  Serial.printf("Taille Flash    : %d Mo\n", ESP.getFlashChipSize() / 1000000);
  Serial.printf("Taille PSRAM    : %d Mo\n", ESP.getPsramSize()  / 1000000);
  Serial.println("----------------------------"); 

Serial.println("\n=== Configuration Réseau ===");

// 🔴 Repere 3
  // Plus besoin de vérifier la correspondance PINS[]/NOMS[] : la structure SORTIES[]
  // garantit qu'une pin et son nom sont toujours définis ensemble, sur la même ligne.

  for (byte i = 0; i < NB_SORTIES; i++)      // Parcourt chaque broche déclarée dans le tableau SORTIES[]
  {
    pinMode(SORTIES[i].pin, OUTPUT);         // Déclare la broche courante comme une sortie numérique
    digitalWrite(SORTIES[i].pin, LOW);       // Force la sortie courante à l'état bas (éteinte) au démarrage
  }

  // 🔴 Repere 3b
  for (byte i = 0; i < NB_BOUTONS; i++)      // Parcourt chaque BP déclaré dans BOUTONS[]
  {
 // pinMode(BOUTONS[i].pinBP, INPUT_PULLDOWN); // repos = LOW, appui = HIGH (via +3.3V) voir modif a faire dans void gererBoutons()
    pinMode(BOUTONS[i].pinBP, INPUT_PULLUP);   // repos = HIGH, appui = LOW (Via GND) la broche du BP en entrée avec résistance de tirage interne
  }

  // Connexion au réseau WiFi (Corrigé pour ESP32 standard : boucle stable avec retour visuel)
  WiFi.mode(WIFI_STA);                       // Configure l'ESP32 en mode station (client de votre box / répéteur)

  // 📶 Scanne les réseaux connus (box, répéteur ESP_Nat_Router, ...) et lance la connexion
  // vers celui qui a le meilleur signal (RSSI le plus fort)
  connecterMeilleurWiFi();

  int temporisation = 0;
  while (WiFi.status() != WL_CONNECTED && temporisation < 30) {
    delay(500);
    Serial.print(".");
    temporisation++;
  }

  if (WiFi.status() == WL_CONNECTED)
{
    server.begin(); // Démarre le serveur
    if (MDNS.begin(HOSTNAME_MDNS)) {
      Serial.println("mDNS démarré");
      Serial.print("Accès par : http://");
      Serial.print(HOSTNAME_MDNS);
      Serial.println(".local");
    } else {
      Serial.println("Erreur de démarrage du mDNS");
    }
    delay(1000);// Delais entre mise en route du serveur et affichage Infos IMPORTANT
   Serial.println(F("\nServeur initialisé, attente de l'adresse IP..."));
    // On attend que l'IP soit valide 
    // (Utile si le routeur met un peu de temps à donner l'IP via DHCP)
    int tentatives = 0;
    while (WiFi.localIP().toString() == "0.0.0.0" && tentatives < 10) {
        delay(100); 
        tentatives++;
    }

    afficherInfosWiFi(); 
    afficherEcranOLED();  // 📶 🖥️ Affiche SSID / IP / état des relais dès que la connexion est établie
}
}

void loop() {

  // 🔴 🛟 Vérifie si l'utilisateur a tapé la lettre " i " dans le moniteur série pour renvoyer les infos réseau
  if (Serial.available() > 0) {
    char commande = Serial.read();
        if (commande == 'i') {
            afficherInfosWiFi();
    }
  }
  // 🔴 Lit les boutons poussoir et bascule les relais associés (fonctionne même sans WiFi)
  gererBoutons();

  // 🖥️ Rafraîchit l'écran OLED toutes les secondes, de façon non-bloquante
  if (millis() - dernierAffichageOLED >= INTERVALLE_OLED_MS)
  {
    dernierAffichageOLED = millis();
    afficherEcranOLED();
  }

  // Vérifie la connexion WiFi et la rétablit si elle a été coupée (de manière non-bloquante)
  verifierWiFi();

  WiFiClient client = server.accept();      // Vérifie si un client (navigateur) tente de se connecter

  if (!client)          // Si aucun client n'est connecté...
    return;              // ...on sort immédiatement de loop() et on recommence au tour suivant

  Serial.println(F("Client connecté"));   // Signale dans le moniteur série qu'un client vient de se connecter

  String requete = "";           // Chaîne qui accumule les caractères de la ligne HTTP en cours de lecture
  requete.reserve(TAILLE_MAX_LIGNE); // OPTIMISATION : Alloue de la mémoire à l'avance pour éviter la fragmentation de la RAM

  String premiereLigne = "";     // Mémorise la toute première ligne de la requête (contient l'URL demandée)
  premiereLigne.reserve(80);     // Évite également la fragmentation pour la première ligne
  
  bool premiereLigneLue = false; // Indique si la première ligne a déjà été capturée

  unsigned long debutAttente = millis();   // mémorise l'heure de début pour le calcul du timeout

  while (client.connected())     // Tant que le navigateur reste connecté...
  {
    // si le client met trop de temps à envoyer sa requête, on abandonne
    // proprement au lieu de rester bloqué ici indéfiniment.
    if (millis() - debutAttente > TIMEOUT_CLIENT_MS)
    {
      Serial.println(F("Timeout client : requête incomplète, abandon."));
      break;
    }

    if (client.available())      // ...et qu'il y a des données à lire...
    {
      char c = client.read();    // Lit un caractère envoyé par le navigateur
      debutAttente = millis();   // Réinitialise le timeout à chaque caractère reçu (le client est toujours actif)

      if (c == '\n')             // Si on reçoit un retour à la ligne, la ligne courante est terminée
      {
        if (requete.length() == 0)  // Une ligne vide signale la fin des en-têtes HTTP
        {
          if (premiereLigne.indexOf("ajax=1") >= 0)   // Vérifie si l'URL contient le paramètre ajax=1
              envoyerEtat(client);                     // Répond uniquement avec l'état des sorties (texte court)
          else
              envoyerPage(client);                     // Chargement complet de la page HTML
          break;      // Sort de la boucle de lecture car la réponse a été envoyée
        }

        if (!premiereLigneLue)      // Si on n'a pas encore mémorisé la première ligne de la requête
        {
          premiereLigne = requete;  // Sauvegarde la ligne actuelle (ex: "GET /P5ON?ajax=1 HTTP/1.1")
          premiereLigneLue = true;  // Marque que la première ligne est désormais connue
        }
        traiterCommande(requete);   // Analyse la ligne reçue pour voir si elle contient une commande ON/OFF ou globale
        requete = "";                // Réinitialise la chaîne pour lire la ligne suivante
      }
      else if (c != '\r')      // Ignore le caractère de retour chariot ('\r'), ne garde que le texte utile
      {
        // sécurité anti-débordement. Si une ligne dépasse une taille
        // raisonnable (requête malformée, cliente buguée ou malveillante...),
        // on ignore les caractères supplémentaires plutôt que de laisser la String grossir indéfiniment.
        if (requete.length() < TAILLE_MAX_LIGNE)
        {
          requete += c;          // Ajoute le caractère lu à la ligne en cours de construction
        }
      }
    }
  }

  client.stop();                          // Ferme la connexion avec le client une fois la réponse envoyée
  Serial.println(F("Client déconnecté"));    // Indique dans le moniteur série qu'un client s'est déconnecté
}

// 🔴 Repere 4
void verifierWiFi() {
  if (WiFi.status() != WL_CONNECTED)   // Si la carte n'est plus connectée au réseau
  {
    unsigned long tempsActuel = millis();
    if (tempsActuel - dernierEssaiWiFi >= INTERVALLE_WIFI_MS) // Tente une reconnexion seulement si le délai est écoulé
    {
      dernierEssaiWiFi = tempsActuel;
      Serial.println(F("WiFi déconnecté, tentative de reconnexion..."));  // Informe dans le moniteur série
      WiFi.disconnect(); 
      connecterMeilleurWiFi();    // 📶 Rescanne et se reconnecte au meilleur réseau connu disponible (rapide et non bloquant s'il échoue directement)
    }
  }
}

// 🔴 Repere Wifi-multi
//============================================================
// connecterMeilleurWiFi : scanne les réseaux WiFi visibles, ne garde que ceux
// déclarés dans RESEAUX_WIFI[] et arduino_secrets.h, et lance la connexion vers celui dont le
// signal (RSSI) est le plus fort (donc le moins négatif, ex: -50 dBm > -80 dBm).
//============================================================
bool connecterMeilleurWiFi() {
  Serial.println(F("Scan des réseaux WiFi disponibles..."));
  int nbTrouves = WiFi.scanNetworks();   // Lance un scan bloquant court (quelques centaines de ms)

  if (nbTrouves <= 0)
  {
    Serial.println(F("Aucun réseau WiFi détecté."));
    WiFi.scanDelete();
    return false;
  }

  int meilleurIndexConnu = -1;     // Index du meilleur réseau connu trouvé, dans RESEAUX_WIFI[]
  int32_t meilleurRSSI   = -1000;  // Très faible au départ pour être sûr d'être battu au premier tour

  // Parcourt tous les réseaux détectés par le scan
  for (int i = 0; i < nbTrouves; i++)
  {
    String  ssidTrouve = WiFi.SSID(i);
    int32_t rssiTrouve = WiFi.RSSI(i);

    // Compare ce réseau détecté à la liste des réseaux connus
    for (byte j = 0; j < NB_RESEAUX_WIFI; j++)
    {
      if (ssidTrouve == RESEAUX_WIFI[j].ssid)
      {
        Serial.printf("  -> Connu : %-24s (%d dBm)\n", ssidTrouve.c_str(), rssiTrouve);
        if (rssiTrouve > meilleurRSSI)   // Garde le signal le plus fort parmi les réseaux connus
        {
          meilleurRSSI = rssiTrouve;
          meilleurIndexConnu = j;
        }
      }
    }
  }

  WiFi.scanDelete();   // Libère la mémoire utilisée par les résultats du scan

  if (meilleurIndexConnu == -1)
  {
    Serial.println(F("Aucun réseau connu (voir arduino_secrets.h) n'a été détecté à portée."));
    return false;
  }

  Serial.print(F("-> Connexion au réseau retenu (meilleur signal) : "));
  Serial.println(RESEAUX_WIFI[meilleurIndexConnu].ssid);

  WiFi.begin(RESEAUX_WIFI[meilleurIndexConnu].ssid, RESEAUX_WIFI[meilleurIndexConnu].pass);
  return true;
}

// 🔴 Repere 4b
// Lit chaque bouton poussoir et bascule (toggle) l'état du relais associé
// à chaque appui détecté. Anti-rebond logiciel non-bloquant (sans delay()),
// basé sur le même principe que la bibliothèque Bounce2.
void gererBoutons() {
  for (byte i = 0; i < NB_BOUTONS; i++)
  {
    bool lecture = digitalRead(BOUTONS[i].pinBP);   // Lecture brute de l'état actuel de la broche du BP

    if (lecture != BOUTONS[i].derniereLecture)      // La lecture vient de changer par rapport au tour précédent
    {
      BOUTONS[i].dernierChangement = millis();      // On redémarre le chrono anti-rebond
    }

    if ((millis() - BOUTONS[i].dernierChangement) > DEBOUNCE_BP_MS)  // Le signal est stable depuis assez longtemps
    {
      if (lecture != BOUTONS[i].etatStable)         // L'état stable a réellement changé (et non un simple rebond)
      {
        BOUTONS[i].etatStable = lecture;            // Mémorise le nouvel état stable

        if (lecture == LOW)                         // LOW = bouton appuyé (câblage en INPUT_PULLUP vers la masse)
       // Pour cabler les boutons différemment (par exemple avec la résistance pull-down vers le +3.3V 
      // plutôt que vers la masse), il faudra inverser la logique LOW/HIGH.
        {
          bool etatActuelRelais = digitalRead(BOUTONS[i].pinRelais);
          digitalWrite(BOUTONS[i].pinRelais, !etatActuelRelais);  // Bascule (toggle) le relais associé
        }
      }
    }

    BOUTONS[i].derniereLecture = lecture;           // Mémorise la lecture brute pour la comparaison du prochain tour
  }
}

// 🔴 Repere 5
void traiterCommande(const String &req) {         // Reçoit la ligne HTTP par référence constante pour éviter de la dupliquer
  if (req.startsWith("GET /P"))                  // Vérification rapide pour s'assurer qu'il s'agit d'une commande de broche individuelle
  {
    for (byte i = 0; i < NB_SORTIES; i++)      // Teste chaque sortie du tableau, une par une
    {
      // Création d'un mini-buffer de caractères pour générer les requêtes à tester (ex: "/P5ON")
      char cmdOn[12];
      char cmdOff[12];
      sprintf(cmdOn, "/P%dON", SORTIES[i].pin);
      sprintf(cmdOff, "/P%dOFF", SORTIES[i].pin);

      if (req.indexOf(cmdOn) >= 0)      // Si la ligne demande d'allumer cette sortie
      {
        digitalWrite(SORTIES[i].pin, HIGH);           // Met la broche à l'état haut (allumée)
        break;                                        // Sortie trouvée, inutile de continuer la boucle pour ce tour
      }
      if (req.indexOf(cmdOff) >= 0)     // Si la ligne demande d'éteindre cette sortie
      {
        digitalWrite(SORTIES[i].pin, LOW);            // Met la broche à l'état bas (éteinte)
        break;                                        // Sortie trouvée, inutile de continuer la boucle
      }
    }
  }
  // Prise en charge des commandes globales
  else if (req.startsWith("GET /ALL_ON"))        // Si la ligne demande de TOUT allumer
  {
    for (byte i = 0; i < NB_SORTIES; i++)
    {
      digitalWrite(SORTIES[i].pin, HIGH);         // Force toutes les broches à l'état haut
    }
  }
  else if (req.startsWith("GET /ALL_OFF"))       // Si la ligne demande de TOUT éteindre
  {
    for (byte i = 0; i < NB_SORTIES; i++)
    {
      digitalWrite(SORTIES[i].pin, LOW);          // Force toutes les broches à l'état bas
    }
  }
}

// 🔴 Repere 6
void genererTableauJSPins(WiFiClient &client) {
  client.print("let pins=[");                  // Début du tableau JS, ex: "let pins=["
  for (byte i = 0; i < NB_SORTIES; i++)        // Parcourt chaque broche déclarée dans SORTIES[]
  {
    client.print(SORTIES[i].pin);               // Écrit le numéro de broche courant (ex: 5)
    if (i < NB_SORTIES - 1) client.print(","); // Ajoute une virgule séparatrice sauf après le dernier élément
  }
  client.println("];");                        // Ferme le tableau JS, ex: "5,6,7,8];"
}

// 🔴 Repere 7
String genererCasesHTML() {
  String html;                          // Buffer qui va accumuler tout le HTML des cases
  html.reserve(NB_SORTIES * 220UL);     // Réserve à l'avance ~220 caractères par case pour éviter les réallocations

  for (byte i = 0; i < NB_SORTIES; i++)     // Parcourt chaque sortie pour générer sa "case" HTML
  {
    byte pin = SORTIES[i].pin;              // Récupère le numéro de broche de la sortie courante
    bool etat = digitalRead(pin);           // Lit l'état actuel de cette broche (0 ou 1)
    String p = String(pin);                 // Convertit une seule fois le numéro de broche en texte

    html += "<!-- Case pour la sortie D"; html += p; html += " -->";        // Repère HTML pour cette case
    html += "<div class='case'>";                                           // Ouvre la case de la sortie courante
    html += "<div class='titre'>"; html += SORTIES[i].nom; html += "</div>"; // affiche le nom personnalisé au lieu de "Sortie Dx"
    html += "<div class='etat' id='txtD"; html += p; html += "'>";         // Ouvre le libellé d'état
    html += etat ? "🟢 ALLUMEE" : "🔴 ETEINTE";                             // Affiche le rond et le texte selon l'état actuel
    html += "</div>";                                                       // Ferme le libellé d'état
    html += "<button id='btnD"; html += p;                                 // Ouvre le bouton avec son identifiant unique
    html += "' class='toggle "; html += (etat ? "on" : "off");            // Ajoute la classe "on" ou "off"
    html += "' data-etat='"; html += etat;                                 // Mémorise l'état actuel dans l'attribut data-etat
    html += "' onclick=\"basculer("; html += p; html += ")\">";            // Définit le clic pour basculer(pin)
    html += etat ? "ON" : "OFF";                                            // Écrit "ON" ou "OFF" à l'intérieur du bouton
    html += "</button></div>";                                             // Ferme le bouton et la case
  }

  return html;      // Renvoie le bloc HTML complet, prêt à être envoyé en un seul print()
}

//============================================================
// Génération de la page Web
//============================================================
void envoyerPage(WiFiClient &client) {     // Construit et envoie la page HTML complète au navigateur

  client.println("HTTP/1.1 200 OK");                       // Ligne de statut HTTP : la requête a réussi
  client.println("Content-type:text/html; charset=utf-8"); // Indique que la réponse est du HTML en UTF-8
  client.println("Connection: close");                     // Ferme proprement et rapidement la connexion après la réponse
  client.println();                                         // Ligne vide obligatoire séparant les en-têtes du contenu

// 🔴 Repere 8
  client.print(F(R"=====(
<!DOCTYPE html>
<html>
<head>
<!-- Force l'affichage correct sur mobile (zoom initial à 100%) -->
<meta name='viewport' content='width=device-width, initial-scale=1'>

<script>
// Variables globales pour stocker la qualité réseau dynamique mise à jour via AJAX
let dernierRSSI = 0;
let derniereQualite = 0;

// Met à jour visuellement UN bouton/case selon l'état reçu (0 ou 1)
function majBouton(num, etat){
  let btn = document.getElementById('btnD'+num);   // Récupère le bouton correspondant à la broche "num"
  let txt = document.getElementById('txtD'+num);   // Récupère le texte d'état correspondant à la broche "num"
  if(!btn || !txt) return;                         // Vérification de sécurité pour éviter une erreur JS
  btn.setAttribute('data-etat', etat);              // Mémorise l'état actuel dans l'attribut data-etat du bouton
  if(etat=='1'){                                    // Si la sortie est allumée
    btn.className='toggle on';                      // Applique le style vert (classe "on")
    btn.innerHTML='ON';                              // Affiche "ON" sur le bouton
    txt.innerHTML = '🟢 ALLUMEE';                    // Affiche le rond vert + "ALLUMEE"
  } else {                                          // Sinon (sortie éteinte)
    btn.className='toggle off';                     // Applique le style rouge (classe "off")
    btn.innerHTML='OFF';                              // Affiche "OFF" sur le bouton
    txt.innerHTML = '🔴 ETEINTE';                   // Affiche le rond rouge + "ETEINTE"
  }
}

// Reçoit une chaîne et extrait à la fois les états des broches et les données réseau dynamiques
function majAffichage(rawText){
  if(!rawText) return;
  // Découpe la partie états de la partie réseau via le séparateur '|'
  let parties = rawText.split('|');
  let dataEtats = parties[0];
  
  // Si les infos réseau sont présentes dans le paquet AJAX, on les actualise dynamiquement
  if(parties.length > 1){
    let infosReseau = parties[1].split(';');
    dernierRSSI = infosReseau[0];
    derniereQualite = infosReseau[1];
  }

  let etats = dataEtats.split(';');   // Découpe la chaîne reçue en tableau d'états, ex: ["1","0","1","0"]
  // Le tableau ci-dessous est généré AUTOMATIQUEMENT par l'Arduino
  // à partir de SORTIES[] (voir fonction genererTableauJSPins côté C++).
  // Ne pas modifier cette ligne à la main : elle est réécrite à chaque
  // chargement de page en fonction du contenu réel de SORTIES[].
)====="));

  // --- PARTIE DYNAMIQUE : génère ici la ligne "let pins=[5,6,7,8];"
  // en fonction du contenu réel de SORTIES[], au lieu d'un tableau figé.
  genererTableauJSPins(client);

  // suite du document HTML (JS + CSS + début du body),
  // toujours statique, donc de nouveau envoyée en un seul print().
  client.print(F(R"=====(
  for(let i=0;i<etats.length;i++){ if(pins[i]) majBouton(pins[i], etats[i]); }  // Applique l'état reçu à chaque broche
}

// Appelée quand on clique sur un bouton : inverse l'état de la sortie "num"
function basculer(num){
  let btn = document.getElementById('btnD'+num);                         // Récupère le bouton cliqué
  if(!btn) return;
  let commande = (btn.getAttribute('data-etat')=='1') ? 'OFF' : 'ON';    // Détermine la commande inverse (ON<->OFF)
  fetch('/P'+num+commande+'?ajax=1').then(r=>r.text()).then(majAffichage); // Envoie la commande au serveur, puis met à jour l'affichage
}

// Appelée lors d'un clic sur l'un des boutons globaux (Tout Allumer / Tout Éteindre)
function commandeGlobale(action){
  fetch('/'+action+'?ajax=1').then(r=>r.text()).then(majAffichage); // Envoie l'action groupée puis actualise l'affichage global
}

// Verrou empêchant l'envoi d'une nouvelle requête AJAX tant que la
// précédente n'a pas reçu sa réponse (évite l'empilement de requêtes).
let requeteEnCours = false;

// Interroge périodiquement le serveur pour rafraîchir l'état affiché
function actualiserEtat(){
  if (requeteEnCours) return;              // Une requête est déjà en cours : on n'en relance pas une nouvelle
  requeteEnCours = true;                  // Verrouille avant l'envoi
  fetch('/ETAT?ajax=1')
    .then(r=>r.text())
    .then(majAffichage)
    .catch(()=>{})                        // Ignore silencieusement une éventuelle erreur réseau ponctuelle
    .finally(()=>{ requeteEnCours = false; });  // Déverrouille dans tous les cas une fois la requête terminée
}

// Intervalle d'actualisation porté à 500 ms pour laisser à la carte
// le temps de répondre avant la requête suivante.
setInterval(actualiserEtat, 500);
</script>

<style>
/* Style général de la page */
body{font-family:Arial,sans-serif;background:#f0f0f0;text-align:center;margin:0;padding:10px;}

/* Le Panneau Blanc central qui contient tout le contenu */
.carte{background:white;padding:20px 15px;margin:20px auto;max-width:360px;border-radius:15px;box-shadow:0px 0px 10px gray;}

/* Grille à 2 colonnes pour organiser les cases de sorties */
.grille{display:grid;grid-template-columns:1fr 1fr;gap:15px;margin-top:15px;}

/* Chaque case individuelle de la grille (une par sortie) */
.case{background:#f8f8f8;border-radius:12px;padding:12px 8px;}

/* Titre affiché en haut de chaque case (ex: "Sortie D5")  Taille du texte*/
.titre{font-size:16px;font-weight:bold;margin-bottom:6px;}

/* Texte affichant l'état (ex: "ALLUMEE"/"ETEINTE") */
.etat{font-size:16px;font-weight:bold;margin-bottom:10px;}

/* Style de base du bouton ON/OFF */
.toggle{width:100%;height:60px;border:none;border-radius:12px;color:white;font-size:18px;font-weight:bold;cursor:pointer;transition: 0.2s;}

/* Couleur verte quand l'état est "ON" */
.on{background:#28a745;}

/* Couleur rouge quand l'état est "OFF" */
.off{background:#d9534f;}

/* Bouton bleu affichant l'adresse IP et la qualité du signal */
.ip{background:#007bff;color:white;padding:12px 20px;border-radius:8px;border:none;font-size:16px;cursor:pointer;margin-top:15px;width:100%;font-weight:bold;}

/* AJOUT : Bouton vert pour tout allumer (même taille que le bouton IP) */
.all-on{background:#28a745;color:white;padding:12px 20px;border-radius:8px;border:none;font-size:16px;cursor:pointer;margin-top:15px;width:100%;font-weight:bold;}

/* AJOUT : Bouton rouge pour tout éteindre (même taille que le bouton IP) */
.all-off{background:#dc3545;color:white;padding:12px 20px;border-radius:8px;border:none;font-size:16px;cursor:pointer;margin-top:15px;width:100%;font-weight:bold;}
</style>
</head>
<body>
<!-- Carte principale contenant le titre et la grille des sorties -->
<div class='carte'>
<!-- Titre du panneau central -->
<h2>
)====="));

  // ⚫⚪⚫Insertion dynamique de la variable texte
  client.print(NOM_PANNEAU_CENTRAL);

  client.print(F(R"=====(</h2>
<!-- Grille qui accueillera une "case" générée par sortie (boucle côté C++) -->
<div class='grille'>
)====="));          // Fin du bloc CSS/JS/en-tête, envoyé en un seul print()

  // bloc dynamique — toutes les "cases" sont construites en
  // mémoire par genererCasesHTML() puis envoyées en UN SEUL client.print().
  client.print(genererCasesHTML());

  client.print(F("</div><!-- fin .grille --><hr>"));   // Ferme la grille des sorties et trace une ligne de séparation

  // Boutons d'actions globales (mêmes dimensions que le bouton IP) ---
  // Bouton Tout Allumer
  client.println(F("<button class='all-on' onclick=\"commandeGlobale('ALL_ON')\">💡 Tout Allumer</button>"));
  
  // Bouton Tout Éteindre
  client.println(F("<button class='all-off' onclick=\"commandeGlobale('ALL_OFF')\">🔌 Tout Éteindre</button>"));


// --- ✅Bouton "Adresse IP" et infos (affiche une popup JS) (Adapté : avec Nom Serveur mDNS dynamique)
  String macStr = WiFi.macAddress();

  client.print("<!-- Bouton affichant l'IP, l'adresse MAC, le nom local et la qualité dynamique via une popup alert() -->");
  client.print("<button class='ip' onclick=\"alert('Carte : "); // Ouvre le bouton bleu et début du message de la popup
  client.print(MA_CARTE); // 🟡🟢 Affiche le nom de la carte ESP utilisée
  client.print("\\nServeur local : http://");
  client.print(HOSTNAME_MDNS); // Nom du serveur local
  client.print(".local\\nBox connectée : "); client.print(WiFi.SSID()); // Nom de la box utilisée
  client.print("\\nAdresse IP : "); client.print(WiFi.localIP());// Insère l'adresse IP locale de la carte dans le message
  client.print("\\nAdresse MAC : "); client.print(macStr); // AJOUT MAC : Insère l'adresse MAC formatée dans la popup
  client.print("\\nPuissance : ' + dernierRSSI + ' dBm\\nQualité : ' + derniereQualite + ' %')\">📡 Infos Réseau</button>"); 

  client.println("</div><!-- fin .carte --></body></html>");   // Ferme la carte, le corps et le document HTML
}

// 🔴 Repere 9
void envoyerEtat(WiFiClient &client)  {   // Envoie une réponse courte contenant seulement l'état des sorties
  client.println("HTTP/1.1 200 OK");                        // Ligne de statut HTTP : la requête a réussi
  client.println("Content-type: text/plain");
  client.println("Connection: close");
  client.println();

  // 1. Construit la liste des états des broches
  for (byte i = 0; i < NB_SORTIES; i++) 
  {
    client.print(digitalRead(SORTIES[i].pin));
    if (i < NB_SORTIES - 1) client.print(";");
  }

  // 2. Calcule et ajoute les données réseau dynamiques (RSSI et %) séparées par '|'
  long rssi = WiFi.RSSI();
  long qualite = 0;
  if (rssi <= -100) qualite = 0;
  else if (rssi >= -50) qualite = 100;
  else qualite = 2 * (rssi + 100);

  client.print("|");
  client.print(rssi);
  client.print(";");
  client.print(qualite);
  
}

//============================================================
// afficherInfosWiFi (Version ultra-stable pour ESP32 standard / S3)
//============================================================
void afficherInfosWiFi() {     // Affiche dans le moniteur série les informations de connexion WiFi
   Serial.println("-------------------------------------");  // Ligne vide pour aérer l'affichage
  Serial.print("Connecté avec succès à : ");   // Affiche le libellé "Connecté au réseau :" (macro F() ajoutée)
  Serial.println(WiFi.SSID());              // Affiche le nom (SSID) du réseau WiFi connecté

  Serial.print("Adresse IP obtenue     : ");   // Affiche le libellé "Adresse IP :"        
  Serial.println(WiFi.localIP());           // Affiche l'adresse IP attribuée à la carte

  String macStr = WiFi.macAddress();     // Récupère l'adresse MAC (nécessaire pour la réservation DHCP dans la box)
  Serial.print("Adresse MAC de la carte: ");    // Affiche le libellé "Adresse MAC :"      
  Serial.println(macStr);

  Serial.print("Puissance du signal    : ");  // Affiche le libellé "Puissance du signal :"
  Serial.print(WiFi.RSSI());                // Affiche la puissance du signal WiFi (en dBm)
  Serial.println(" dBm");                   // Ajoute l'unité "dBm" et termine la ligne
  Serial.println("-------------------------------------");
  
  Serial.print("Ouvrez votre navigateur sur : http://");  
  Serial.println(WiFi.localIP());         // Affiche le libellé d'invitation à ouvrir le navigateur  
  Serial.print("Ou ouvrez votre navigateur sur : http://");
  Serial.print(HOSTNAME_MDNS);            // Affiche dynamiquement le nom mDNS configuré
  Serial.println(".local");
  Serial.println("-------------------------------------\n");

  afficherReseauxDisponibles();   // 📶 Liste tous les réseaux WiFi détectés avec leur qualité
}

// 🖥️ 🔴 Repere 9b
//============================================================
// afficherReseauxDisponibles : scanne et affiche dans le moniteur série
// TOUS les réseaux WiFi détectés à portée, avec leur RSSI (dBm) et leur
// qualité (%). Marque les réseaux connus (présents dans RESEAUX_WIFI[])
// et celui actuellement connecté.
//============================================================
void afficherReseauxDisponibles() {
  Serial.println(F("--- Réseaux WiFi détectés ---"));

  int nbTrouves = WiFi.scanNetworks();   // Scan bloquant court (quelques centaines de ms)

  if (nbTrouves <= 0)
  {
    Serial.println(F("  Aucun réseau détecté."));
  }
  else
  {
    String ssidActuel = WiFi.SSID();   // Réseau actuellement connecté (pour le repérer dans la liste)

    for (int i = 0; i < nbTrouves; i++)
    {
      String  ssidTrouve = WiFi.SSID(i);
      int32_t rssiTrouve = WiFi.RSSI(i);

      // Calcule la qualité en % à partir du RSSI (même formule que dans envoyerEtat())
      long qualite = 0;
      if (rssiTrouve <= -100) qualite = 0;
      else if (rssiTrouve >= -50) qualite = 100;
      else qualite = 2 * (rssiTrouve + 100);

      // Vérifie si ce réseau fait partie des réseaux connus (RESEAUX_WIFI[])
      bool connu = false;
      for (byte j = 0; j < NB_RESEAUX_WIFI; j++)
      {
        if (ssidTrouve == RESEAUX_WIFI[j].ssid) { connu = true; break; }
      }

      // Marqueurs visuels : ✅ = réseau connu, ➡️ = réseau actuellement connecté
      char marqueur = (ssidTrouve == ssidActuel) ? '>' : (connu ? '*' : ' ');

      Serial.printf("  %c %-24s %5d dBm   %3ld %%\n", marqueur, ssidTrouve.c_str(), rssiTrouve, qualite);
    }
    Serial.println(F("  (> = connecté actuellement, * = connu dans arduino_secrets.h)"));
  }

  WiFi.scanDelete();   // Libère la mémoire utilisée par les résultats du scan
  Serial.println(F("-------------------------------------\n"));
}

//============================================================
// 🖥️ afficherEcranOLED : Le SSID, l'IP et l'état des 4 relais
//============================================================
void afficherEcranOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  // --- Bloc réseau (2 lignes) ---
  if (WiFi.status() == WL_CONNECTED)
  {
    display.print(F("SSID:"));
    display.println(WiFi.SSID());          // Nom du réseau WiFi connecté
    display.print(F("IP: "));
    display.println(WiFi.localIP());       // Adresse IP locale
  }
  else
  {
    display.println(F("WiFi: deconnecte"));
    display.println(F("Reconnexion..."));
  }

  display.drawLine(0, 18, SCREEN_WIDTH - 1, 18, SSD1306_WHITE); // Ligne de séparation

  // --- Bloc état des relais (une ligne par relais) ---
  int y = 22;
  for (byte i = 0; i < NB_SORTIES; i++)
  {
    bool etat = digitalRead(SORTIES[i].pin);
    display.setCursor(0, y);
    display.print(SORTIES[i].nom);         // Nom du relais (ex: "Salon")
    display.print(F(": "));
    display.println(etat ? F("ON") : F("OFF"));
    y += 10;                               // Ligne suivante (10 px d'écart, adapté à un écran 128x64)
  }

  display.display();   // Envoie le buffer à l'écran (obligatoire avec Adafruit_SSD1306)
}