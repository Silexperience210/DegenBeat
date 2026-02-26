# DegenBeat - Version LCDWiki ESP32-S3 2.8inch Display

Adaptation du projet DegenBeat pour le hardware **LCDWiki ESP32-S3 2.8inch Display** (SKU: ES3C28P).

## 🎯 Hardware Cible

| Composant | Spécification |
|-----------|---------------|
| **MCU** | ESP32-S3-N16R8 (Xtensa LX7 dual-core @ 240MHz) |
| **Flash** | 16MB |
| **PSRAM** | 8MB (OPI) |
| **Display** | ILI9341 2.8" IPS TFT, 240×320 (portrait) |
| **Touch** | FT6336U capacitif (I2C) |
| **RGB LED** | Single-line WS2812-style sur IO42 |
| **Alimentation** | USB-C 5V ou batterie lithium 3.7V |

## 📍 Pinout

### Display SPI
| Pin | GPIO | Fonction |
|-----|------|----------|
| CS | IO10 | Chip Select |
| MOSI | IO11 | Data Out |
| SCLK | IO12 | Clock |
| MISO | IO13 | Data In |
| DC | IO46 | Data/Command |
| BL | IO45 | Backlight |
| RST | - | Partagé avec ESP32-S3 |

### Touch I2C (FT6336U)
| Pin | GPIO | Fonction |
|-----|------|----------|
| SDA | IO16 | I2C Data |
| SCL | IO15 | I2C Clock |
| RST | IO18 | Reset |
| IRQ | IO17 | Interrupt |

### RGB LED
| Pin | GPIO | Fonction |
|-----|------|----------|
| DATA | IO42 | Single-line RGB LED |

## 🔧 Modifications par rapport à l'original

### 1. Changement de résolution
- **Original**: 320×240 (paysage)
- **Nouveau**: 240×320 (portrait)
- Tous les éléments UI ont été repositionnés

### 2. Changement de driver tactile
- **Original**: XPT2046 (SPI résistif)
- **Nouveau**: FT6336U (I2C capacitif)
- La librairie `FT6336U` remplace `XPT2046_Touchscreen`
- Plus besoin de calibration du touch (capacitif)

### 3. Changement de contrôleur LED
- **Original**: 3 pins séparés (R:4, G:16, B:17)
- **Nouveau**: 1 pin single-line (IO42)
- Utilisation de `FastLED` pour contrôler le LED WS2812-style

### 4. Changement de driver display
- **Original**: ST7789
- **Nouveau**: ILI9341
- Même résolution mais orientation différente

### 5. ESP32 → ESP32-S3
- Plus de RAM (8MB PSRAM)
- Plus de Flash (16MB)
- Meilleures performances WiFi

## 📦 Librairies requises

```ini
lib_deps = 
    bodmer/TFT_eSPI@^2.5.43
    https://github.com/strange-v/FT6336U.git
    bblanchon/ArduinoJson@^7.2.0
    ricmoo/QRCode@^0.0.1
    bitbank2/PNGdec@^1.1.6
    fastled/FastLED@^3.6.0
```

## 🚀 Installation

1. **Cloner le projet**:
```bash
cd DegenBeat-ESP32S3-Adapted
```

2. **Compiler et uploader**:
```bash
pio run --target upload
```

3. **Monitor série**:
```bash
pio device monitor --baud 115200
```

## 🖥️ Configuration initiale

Au premier démarrage:
1. Connectez-vous au WiFi `DEGENBEAT`
2. Ouvrez un navigateur à `192.168.4.1`
3. Configurez vos identifiants WiFi et LN Markets

## 📱 Interface utilisateur

L'interface a été adaptée pour l'écran portrait 240×320:

- **Barre de navigation**: En haut (4 boutons: HOME, WALLET, POS, HIST)
- **Prix BTC**: En haut à gauche avec grand affichage
- **Boutons SELL/BUY**: À droite
- **Léviers**: 25x, 50x, 75x, 100x centrés
- **Bouton EXECUTER**: En bas

## ⚠️ Notes importantes

1. **Touch capacitif**: Plus réactif que le résistif, pas besoin de calibration
2. **LED RGB**: Utilise FastLED avec une seule LED WS2812-style
3. **PSRAM**: Le projet peut utiliser la PSRAM si besoin de plus de mémoire
4. **Partition**: Utilise `default_16MB.csv` pour les 16MB de flash

## 🐛 Dépannage

### L'écran reste noir
- Vérifier que le backlight est activé (pin 45)
- Vérifier la connexion SPI

### Le touch ne répond pas
- Vérifier les connexions I2C (SDA:16, SCL:15)
- Vérifier que le câble est bien branché

### Problèmes de mémoire
- Le ESP32-S3 a 8MB de PSRAM, normalement pas de problème
- Réduire la taille du cache si nécessaire

## 📄 License

Même license que le projet original DegenBeat.

## 🙏 Crédits

- Projet original: [DegenBeat](https://github.com/Silexperience210/DegenBeat) par Silexperience210
- Adaptation pour LCDWiki ESP32-S3 2.8inch Display
