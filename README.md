<div align="center">

<img src="https://img.shields.io/badge/Platform-ESP32-blue?style=for-the-badge&logo=espressif&logoColor=white"/>
<img src="https://img.shields.io/badge/Framework-Arduino-00979D?style=for-the-badge&logo=arduino&logoColor=white"/>
<img src="https://img.shields.io/badge/PlatformIO-Compatible-orange?style=for-the-badge&logo=platformio&logoColor=white"/>
<img src="https://img.shields.io/badge/Display-ST7789%20320x240-purple?style=for-the-badge"/>
<img src="https://img.shields.io/badge/Bitcoin-Lightning-F7931A?style=for-the-badge&logo=bitcoin&logoColor=white"/>
<img src="https://img.shields.io/badge/Release-v1.0.0-green?style=for-the-badge"/>
<img src="https://img.shields.io/badge/License-MIT-yellow?style=for-the-badge"/>

# DegenBeat — LN Markets Touch Terminal

### Terminal de trading Bitcoin sur ESP32 avec écran tactile 320×240, QR code de dépôt Lightning, indicateur P&L LED RGB, et connexion sécurisée à l'API LN Markets v3.

</div>

---

## Table des matières

- [Vue d'ensemble](#vue-densemble)
- [Matériel requis](#matériel-requis)
- [Fonctionnalités](#fonctionnalités)
- [Captures d'écran](#captures-décran)
- [Installation](#installation)
- [Configuration](#configuration)
- [Utilisation](#utilisation)
- [LED RGB — Indicateur P\&L](#led-rgb--indicateur-pl)
- [Architecture technique](#architecture-technique)
- [Performances RAM](#performances-ram)
- [Dépannage](#dépannage)
- [Licence](#licence)

---

## Vue d'ensemble

**DegenBeat** est un terminal de trading autonome qui transforme un ESP32 Sunton 2432S028R en station de trading Bitcoin directement connectée à [LN Markets](https://lnmarkets.com). Il affiche le prix BTC en temps réel, permet d'ouvrir/fermer des positions à effet de levier, de déposer via Lightning Network, de consulter l'historique des trades — le tout depuis un écran tactile 3,2 pouces, sans ordinateur.

```
┌─────────────────────────────────────────┐
│  HOME  │ WALLET │  POS   │    HIST      │  ← Barre navigation tactile
├─────────────────────────────────────────┤
│  BITCOIN PRICE                          │
│  $97,420          [SELL] [BUY]          │  ← Prix live + boutons trade
│                                         │
│  LEVERAGE:  [25x] [50x] [75x] [100x]   │  ← Sélection levier
│  QUICK: [100s] [500s] [1000s] [5000s]  │  ← Montants rapides
│  MARGIN: 1000 sats  [−] [+]            │  ← Ajustement montant
│  [======= EXECUTE TRADE =========]     │  ← Bouton exécution
└─────────────────────────────────────────┘
```

---

## Matériel requis

| Composant | Référence | Notes |
|-----------|-----------|-------|
| Microcontrôleur | **Sunton ESP32-2432S028R** | ESP32 WROOM-32, SPIFFS 4MB |
| Écran | **ST7789 320×240 IPS** | Intégré — câblage SPI dédié |
| Tactile | **XPT2046** | SPI séparé du TFT |
| LED | **RGB commun cathode** | Pins 4 (R), 16 (G), 17 (B) |
| Connectivité | **WiFi 2.4GHz** | WPA2 — portail captif intégré |

> **Compatible uniquement** avec le Sunton ESP32-2432S028R (références SD_CS, pin mapping TFT/touch spécifiques). D'autres boards ESP32 nécessitent un recâblage.

---

## Fonctionnalités

### Trading
- **Prix BTC temps réel** — mise à jour toutes les 2 secondes via tâche FreeRTOS dédiée (Core 0)
- **Ouverture de positions** — SELL / BUY avec levier 25×, 50×, 75×, 100×
- **Montants rapides** — 100, 500, 1000, 5000 sats + ajustement fin ±100 sats
- **Fermeture de positions** — bouton CLOSE par position dans l'onglet POS
- **Annulation d'ordres** — bouton CANCEL par ordre ouvert
- **Historique** — 10 derniers trades fermés avec P&L et date

### Wallet & Lightning
- **Solde en temps réel** — balance en sats + conversion USD
- **Retrait Lightning** — montants rapides + ajustement, vers adresse Lightning configurée
- **Dépôt QR code** — génération locale via librairie QRCode (pas de dépendance web)
- **Historique des dépôts** — 10 derniers dépôts Lightning avec montant, date, commentaire, hash

### Infrastructure
- **Portail captif WiFi** — configuration réseau + API sans code via navigateur web
- **SPIFFS chiffré** — credentials API stockés avec AES-256-CBC (clé dérivée depuis MAC ESP32)
- **Cache intelligent** — 6 niveaux de cache API (2s prix, 15s wallet, 10s positions…)
- **Reconnexion WiFi automatique** — non-bloquante via timer callback
- **Indicateur LED RGB** — 7 niveaux de P&L visualisés en clignotements colorés

---

## Captures d'écran

```
┌──────────────────────┐    ┌──────────────────────┐
│ HOME  WALLET POS HIST│    │ HOME  WALLET POS HIST│
│                      │    │                      │
│  BITCOIN PRICE       │    │  Balance: 24,500 sats│
│  $97,420    [SELL]   │    │  ≈ $23.80 USD        │
│             [BUY ]   │    │  Margin: 12,000 sats │
│  LEVERAGE:           │    │  Retrait: 1000 sats  │
│  [25x][50x][75x][100]│    │  [100][500][1000][2k]│
│  QTY: 1000 sats [-][+│    │  [WITHDRAW][DEPOSIT] │
│  [====EXECUTE====]   │    │  [HISTORY] [RESET]   │
└──────────────────────┘    └──────────────────────┘
        HOME                        WALLET

┌──────────────────────┐    ┌──────────────────────┐
│ HOME  WALLET POS HIST│    │ HOME  WALLET POS HIST│
│ POSITIONS & ORDERS   │    │   TRADE HISTORY      │
│ RUNNING POSITIONS:   │    │                      │
│ buy 50x 2000s        │    │ buy 25x 9a3f2b1c...  │
│ Entry:$95k Now:$97k  │    │ +2.45 sats  2025-01  │
│ +1042s +52.1%        │    │ sell 100x 3b7e9d2a.. │
│ [CLOSE]              │    │ -890 sats   2025-01  │
│ OPEN ORDERS:         │    │                      │
│ buy limit 25x 1000s  │    │                      │
│ [CANCEL]             │    │                      │
└──────────────────────┘    └──────────────────────┘
       POSITIONS                    HISTORY
```

---

## Installation

### Prérequis

- [PlatformIO IDE](https://platformio.org/install) (extension VSCode recommandée)
- [Git](https://git-scm.com/)
- Câble USB-C / Micro-USB selon votre board

### 1. Cloner le dépôt

```bash
git clone https://github.com/Silexperience210/DegenBeat.git
cd DegenBeat
```

### 2. Ouvrir dans PlatformIO

```bash
# Via VSCode
code .
# Puis cliquer sur l'icône PlatformIO dans la barre latérale

# Via CLI
pio run --target upload
```

### 3. Compiler et flasher

```bash
# Compiler seulement
pio run

# Compiler + flasher
pio run --target upload

# Monitor série
pio device monitor --baud 115200
```

> Le `platformio.ini` est préconfiguré pour le Sunton ESP32-2432S028R. **Ne pas modifier** les `build_flags` TFT sans connaître votre variante exacte.

---

## Configuration

### Premier démarrage — Portail captif

Au premier démarrage (ou après un RESET), le terminal crée un **point d'accès WiFi** :

```
SSID  :  LNMarkets-Setup
Mot de passe  :  (aucun)
URL config    :  http://192.168.4.1
```

Connectez-vous à ce réseau WiFi depuis votre téléphone ou PC, votre navigateur s'ouvrira automatiquement sur le formulaire de configuration.

### Champs à renseigner

| Champ | Description |
|-------|-------------|
| **WiFi SSID** | Nom de votre réseau WiFi 2.4GHz |
| **WiFi Password** | Mot de passe WiFi |
| **API Key** | Clé API LN Markets (Réglages → API) |
| **API Secret** | Secret API LN Markets |
| **API Passphrase** | Passphrase API LN Markets |
| **Lightning Address** | Votre adresse Lightning pour les retraits (ex: `you@wallet.com`) |
| **Deposit LN Address** | Adresse Lightning de dépôt (affichée en QR code) |

### Obtenir les clés API LN Markets

1. Connectez-vous sur [lnmarkets.com](https://lnmarkets.com)
2. Allez dans **Settings → API**
3. Créez une nouvelle paire de clés avec les permissions : `Read`, `Trade`, `Withdraw`
4. Notez la Key, Secret et Passphrase — ils ne seront plus affichés

> **Sécurité** : Les credentials sont stockés chiffrés en AES-256-CBC sur SPIFFS. La clé de chiffrement est dérivée depuis l'adresse MAC unique de l'ESP32.

### Réinitialisation

Depuis l'écran **WALLET**, appuyez sur le bouton **RESET** (coin inférieur droit). L'ESP32 efface la configuration SPIFFS et redémarre en mode portail captif.

---

## Utilisation

### Navigation

La barre du haut divise l'écran en 4 zones tactiles :

| Zone | Écran |
|------|-------|
| `HOME` (0–80px) | Prix BTC + ouverture de trade |
| `WALLET` (80–160px) | Solde + dépôt/retrait |
| `POS` (160–240px) | Positions actives + ordres ouverts |
| `HIST` (240–320px) | Historique trades + dépôts Lightning |

### Ouvrir un trade

1. Aller sur **HOME**
2. Appuyer **SELL** ou **BUY** (le bouton se met en couleur)
3. Sélectionner le levier : `25x`, `50x`, `75x` ou `100x`
4. Choisir un montant rapide ou ajuster avec `[−]` / `[+]`
5. Appuyer **EXECUTE TRADE**

### Déposer en Lightning

1. Aller sur **WALLET**
2. Appuyer **DEPOSIT**
3. Un QR code est généré localement depuis votre `deposit_lnaddress`
4. Scanner depuis votre wallet Lightning pour envoyer des sats

### Retirer en Lightning

1. Aller sur **WALLET**
2. Ajuster le montant de retrait (montants rapides ou `[−]` / `[+]`)
3. Appuyer **WITHDRAW**
4. Le retrait est envoyé vers votre `lightning_address` configurée

---

## LED RGB — Indicateur P&L

La LED RGB intégrée reflète l'état de vos positions en temps réel :

| LED | Signification |
|-----|---------------|
| **Éteinte** | Aucune position ouverte |
| **Bleu fixe** | Connexion API active, pas de position |
| **Clignotement lent** (1Hz) | P&L entre 0% et 5% |
| **Clignotement normal** (2Hz) | P&L entre 5% et 25% |
| **Clignotement rapide** (4Hz) | P&L entre 25% et 50% |
| **Clignotement très rapide** (8Hz) | P&L entre 50% et 100% |
| **Vert fixe** | P&L > 100% — Moon mode |
| **Rouge fixe** | P&L < −100% — Liquidation imminente |

> La LED est pilotée depuis `loop()` sans bloquer le rendu écran. La fréquence de clignotement est calculée dynamiquement sur le P&L moyen de toutes les positions actives.

---

## Architecture technique

### FreeRTOS — Dual-core

```
Core 0 (App)    │  Core 1 (Pro)
────────────────┼────────────────────────────────────
priceUpdateTask │  loop() → handleTouch() → switchScreen()
  fetchBTCPrice │           updateLedPnl()
  displayPrice()│           message_timer.check()
  (toutes 2s)   │           WiFi watchdog non-bloquant
```

Le prix BTC est mis à jour en tâche séparée sur le Core 0 via `xTaskCreatePinnedToCore`, avec accès TFT protégé par mutex (`tft_mutex`).

### Cache API

| Cache | TTL | Description |
|-------|-----|-------------|
| `priceCache` | 2s | Ticker BTC (tâche dédiée) |
| `walletCache` | 15s | Solde du compte |
| `positionsCache` | 10s | Trades en cours |
| `ordersCache` | 12s | Ordres ouverts |
| `historyCache` | 60s | Historique trades fermés |
| `depositsCache` | 60s | Historique dépôts Lightning |

La méthode `markValid()` marque le cache sans stocker la réponse JSON en heap — économise ~30–40 KB par cycle de refresh.

### Système non-bloquant

Toutes les temporisations utilisent `NonBlockingDelay` (callback-based timer), évitant tout `delay()` dans `loop()`. Le temps de réponse tactile reste sous 20ms même pendant un appel API.

### Affichage antiflicker

Le prix BTC est rendu via `TFT_eSprite` (sprite 172×35px, 16-bit color depth) : le rendu se fait en mémoire puis est poussé en une seule opération DMA vers l'écran, éliminant le flash visible du fillRect/print direct.

### Sécurité API

- Signature HMAC-SHA256 des requêtes (timestamp + method + path + body)
- Stockage credentials AES-256-CBC, clé dérivée depuis MAC ESP32 via PBKDF2
- Validation des champs API au formulaire de configuration
- Timeout HTTP 5 secondes pour éviter les blocages réseau

---

## Performances RAM

Budget mémoire sur ESP32 WROOM-32 (520 KB total, ~280 KB disponible après stack/libs) :

| Poste | Avant optim | Après optim | Économie |
|-------|------------|-------------|----------|
| Cache API (6×) | ~45 KB (JSON strings) | ~1 KB (timestamps) | **~44 KB** |
| Tableaux de données | ~3 KB (String heap) | ~2 KB (char BSS) | **~1 KB + no frag** |
| TFT_eSprite prix | 0 | 172×35×2 = ~12 KB | −12 KB (budget sprite) |
| **Free heap at rest** | ~90 KB | **~130–150 KB** | **+40–60 KB** |

> Sans PSRAM (WROOM-32). La fragmentation heap est réduite par la conversion `String[]` → `char[][]` et la non-rétention des réponses JSON en cache.

---

## Dépannage

### L'écran reste noir au boot

- Vérifier que `TFT_BL` (pin 21) est bien `OUTPUT HIGH` dans `setup()`
- Vérifier le câblage SPI TFT : MOSI=13, MISO=12, SCLK=14, CS=15, DC=2
- S'assurer que le `platformio.ini` contient `-DST7789_DRIVER=1`

### Les couleurs sont inversées / mauvaises

- Le ST7789 du Sunton nécessite `-DTFT_RGB_ORDER=TFT_BGR` et `-DTFT_INVERSION_OFF=1`
- Ne pas utiliser `ILI9341_DRIVER` — ce board utilise bien ST7789

### Le tactile ne répond pas ou est décalé

- La rotation touch doit être synchronisée avec la rotation TFT : `touch.setRotation(1)`
- Vérifier SPI dédié touch : CLK=25, MISO=39, MOSI=32, CS=33, IRQ=36

### L'API retourne des erreurs 401

- Régénérer les clés API sur lnmarkets.com (les clés ont une durée de vie)
- Vérifier que la passphrase fait au moins 8 caractères
- S'assurer que l'heure NTP est synchronisée (la signature HMAC inclut le timestamp)

### "Erreur API" sur tous les écrans

- Le WiFi est peut-être perdu : vérifier le message "WiFi perdu" sur l'écran
- Vérifier le mode : si `api_key == "public"`, seul le prix BTC s'affiche, pas le wallet

### SPIFFS FAIL au démarrage

- Reformater SPIFFS : `pio run --target uploadfs` puis re-flasher le firmware
- Certains ESP32 clones ont SPIFFS désactivé — vérifier avec `SPIFFS.begin(true)` (format automatique)

---

## Librairies utilisées

| Librairie | Version | Usage |
|-----------|---------|-------|
| [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) | ^2.5.43 | Driver TFT + sprites |
| [XPT2046_Touchscreen](https://github.com/PaulStoffregen/XPT2046_Touchscreen) | latest | Lecture touch |
| [ArduinoJson](https://arduinojson.org/) | ^7.2.0 | Parse JSON API |
| [QRCode](https://github.com/ricmoo/qrcode) | ^0.0.1 | Génération QR code local |
| [PNGdec](https://github.com/bitbank2/PNGdec) | ^1.1.6 | Décodage PNG |
| mbedTLS | (ESP-IDF) | AES-256-CBC + HMAC-SHA256 |

---

## Licence

```
MIT License

Copyright (c) 2025 Silexperience210

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

<div align="center">

**Built for degens, by degens.**

*Le trading de produits dérivés Bitcoin avec effet de levier comporte un risque élevé de perte en capital. Ce projet est fourni à titre éducatif. Tradez responsablement.*

[![GitHub Stars](https://img.shields.io/github/stars/Silexperience210/DegenBeat?style=social)](https://github.com/Silexperience210/DegenBeat)
[![GitHub Forks](https://img.shields.io/github/forks/Silexperience210/DegenBeat?style=social)](https://github.com/Silexperience210/DegenBeat/fork)

</div>
