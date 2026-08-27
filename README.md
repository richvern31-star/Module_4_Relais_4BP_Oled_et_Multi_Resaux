# Module Relais 4 Canaux — ESP32 (BP + OLED + Multi-Réseaux WiFi)

Firmware pour ESP32 pilotant **4 relais** via boutons poussoir physiques **et** une page web (avec mise à jour AJAX en temps réel), avec affichage d'état sur un écran **OLED I2C** et connexion automatique au **meilleur réseau WiFi connu** parmi une liste.

## ✨ Fonctionnalités

- Pilotage de 4 relais indépendamment via :
  - 4 boutons poussoir physiques (anti-rebond logiciel, non bloquant)
  - une interface web responsive (clic sur bouton ON/OFF, ou "Tout Allumer" / "Tout Éteindre")
- Mise à jour automatique de l'état affiché sur la page web toutes les 500 ms (AJAX, sans recharger la page)
- Écran OLED SSD1306 (128x64) affichant :
  - le SSID et l'adresse IP du réseau connecté
  - l'état ON/OFF de chaque relais (avec son nom personnalisé)
- Connexion **multi-réseaux WiFi** : scanne tous les réseaux connus déclarés dans `arduino_secrets.h` et se connecte automatiquement à celui dont le signal (RSSI) est le plus fort
- Reconnexion WiFi automatique et non bloquante en cas de coupure
- Serveur mDNS (accès via `http://<nom>.local`)
- Bouton "Infos Réseau" sur la page web (popup avec IP, MAC, SSID, puissance du signal)
- Table `CANAUX[]` unique regroupant relais + bouton poussoir associé (facile à étendre)

## 🛠️ Matériel

- Carte **ESP32 Dev Module** (ESP32-WROOM)
- Module relais 4 canaux
- 4 boutons poussoir
- Écran OLED I2C SSD1306 0,96" (128x64)

## 🔌 Câblage / Assignation des GPIO

| Élément      | GPIO | Détail                          |
|--------------|------|----------------------------------|
| Relais K1    | 32   | Piloté par BP GPIO 14           |
| Relais K2    | 33   | Piloté par BP GPIO 16           |
| Relais K3    | 25   | Piloté par BP GPIO 17           |
| Relais K4    | 26   | Piloté par BP GPIO 18           |
| OLED SDA     | 21   | Bus I2C                         |
| OLED SCL     | 22   | Bus I2C                         |

> Les boutons poussoir sont câblés en **INPUT_PULLUP** (entre la broche et la masse GND) : au repos la broche est à `HIGH`, et passe à `LOW` lors d'un appui.

⚠️ GPIO à éviter sur ESP32-WROOM-DA : `0, 6, 7, 8, 9, 10, 11, 15` (non utilisables) — non présents : `20, 24, 28, 29, 30, 31, 37, 38`.
GPIO utilisables : `1, 2, 3, 4, 5, 12, 13, 14, 16, 17, 18, 19, 21 (I2C), 22 (I2C), 23, 25, 26, 27, 32, 33`.
Broches en entrée uniquement : `34, 35, 36, 39`.

## 📚 Bibliothèques requises

À installer via le Gestionnaire de bibliothèques Arduino IDE :

- `WiFi` (incluse avec le core ESP32)
- `ESPmDNS` (incluse avec le core ESP32)
- `Adafruit SSD1306`
- `Adafruit GFX Library` (dépendance installée automatiquement)

Testé avec le **Gestionnaire de carte ESP32 par Espressif Systems v3.3.11**.

## 🔐 Configuration WiFi (`arduino_secrets.h`)

Créez un fichier `arduino_secrets.h` à côté du `.ino`, contenant vos identifiants réseau :

```cpp
#define SECRET_SSID   "NomDeVotreBox"
#define SECRET_PASS   "MotDePasseBox"
#define SECRET_SSID2  "ESP_Nat_Router"
#define SECRET_PASS2  "MotDePasseRepeteur"
// Ajoutez SECRET_SSID3 / SECRET_PASS3 si besoin d'un 3ème réseau
```

Puis complétez (ou réduisez) la liste `RESEAUX_WIFI[]` dans le `.ino` en fonction du nombre de réseaux déclarés.

> ⚠️ Ne partagez jamais ce fichier publiquement (mots de passe WiFi en clair). Ajoutez-le à votre `.gitignore`.

## 🚀 Installation

1. Installez les bibliothèques listées ci-dessus.
2. Créez `arduino_secrets.h` avec vos identifiants WiFi (voir ci-dessus).
3. Adaptez si besoin :
   - `HOSTNAME_MDNS` (nom d'accès local, par défaut `RichardV` → `http://RichardV.local`)
   - `NOM_PANNEAU_CENTRAL` / `MA_CARTE` (textes affichés sur la page web)
   - Les noms des relais dans `CANAUX[]` (ex: `"Relais K1"` → `"Salon"`)
4. Sélectionnez la carte **ESP32 Dev Module** dans l'IDE Arduino.
5. Téléversez le programme.
6. Ouvrez le moniteur série (115200 bauds) pour voir l'IP obtenue, ou consultez directement l'écran OLED.

## 🖱️ Utilisation

- **Boutons physiques** : un appui bascule (toggle) l'état du relais associé, indépendamment du WiFi.
- **Page web** : ouvrez `http://<IP_de_la_carte>` ou `http://<HOSTNAME_MDNS>.local` dans un navigateur sur le même réseau.
  - Cliquez sur une case pour allumer/éteindre le relais correspondant.
  - Boutons globaux "💡 Tout Allumer" / "🔌 Tout Éteindre".
  - Bouton "📡 Infos Réseau" : affiche IP, adresse MAC, SSID et qualité du signal.
- **Moniteur série** : tapez `i` puis Entrée pour ré-afficher les informations réseau et la liste des réseaux WiFi détectés à portée.

## 🌐 Points d'accès HTTP (API)

| Requête                  | Effet                                      |
|---------------------------|---------------------------------------------|
| `GET /`                   | Page web complète                          |
| `GET /P<pin>ON`           | Allume le relais sur la broche `<pin>`     |
| `GET /P<pin>OFF`          | Éteint le relais sur la broche `<pin>`     |
| `GET /ALL_ON`             | Allume tous les relais                     |
| `GET /ALL_OFF`            | Éteint tous les relais                     |
| `GET /ETAT?ajax=1`        | Renvoie l'état de tous les relais + RSSI/qualité (texte brut, utilisé en interne par la page web) |

Exemple : `http://RichardV.local/P32ON` allume le relais K1.

## 🧩 Structure du code

Le tableau `CANAUX[]` regroupe pour chaque canal : la broche du relais, son nom affiché, la broche du bouton poussoir associé et ses variables d'anti-rebond. Pour ajouter un 5ème canal, il suffit d'ajouter une ligne à ce tableau (plus besoin de synchroniser plusieurs tableaux séparés) :

```cpp
Canal CANAUX[] = {
  {32, "Relais K1", 14, HIGH, HIGH, 0},
  {33, "Relais K2", 16, HIGH, HIGH, 0},
  {25, "Relais K3", 17, HIGH, HIGH, 0},
  {26, "Relais K4", 18, HIGH, HIGH, 0},
  // {27, "Relais K5", 19, HIGH, HIGH, 0},  // exemple d'ajout
};
```

## 📄 Licence

Projet personnel — à adapter librement selon vos besoins.
