# Intégration au Projet DegenBeat Original

Ce document explique comment intégrer cette adaptation multi-hardware dans le repository DegenBeat original.

## 🎯 Objectif

Transformer le repository DegenBeat en projet multi-hardware avec:
1. **Code source unique** supportant plusieurs boards
2. **Builds automatiques** pour chaque hardware
3. **WebFlasher** avec sélecteur de hardware
4. **Releases GitHub** contenant tous les firmwares

## 📂 Structure recommandée du repository

```
DegenBeat/                          # Repository principal
├── .github/
│   └── workflows/
│       └── build-and-release.yml   # CI/CD (nouveau)
├── src/
│   └── main.cpp                    # Code source unifié
├── include/
│   └── hardware_config.h           # Configurations hardware (nouveau)
├── firmwares/
│   ├── manifests/                  # Manifests JSON (nouveau)
│   │   ├── sunton-2432s028r.json
│   │   └── lcdwiki-esp32s3-28p.json
│   └── builds/                     # Fichiers .bin (générés)
├── webflasher/
│   └── index.html                  # Page webflasher (nouveau)
├── platformio.ini                  # Config PlatformIO (modifié)
└── README.md                       # Documentation (modifiée)
```

## 📝 Modifications du code source

### 1. Créer `include/hardware_config.h`

Ce fichier centralise les configurations hardware:

```cpp
#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

// Détection automatique du hardware via build flags
#if defined(HARDWARE_SUNTON)
    // ========== SUNTON ESP32-2432S028R ==========
    #define TFT_BL 21
    #define TOUCH_CS 33
    #define TOUCH_IRQ 36
    #define TOUCH_MOSI 32
    #define TOUCH_MISO 39
    #define TOUCH_CLK 25
    #define LED_RED 4
    #define LED_GREEN 16
    #define LED_BLUE 17
    #define USE_XPT2046_TOUCH 1
    #define SCREEN_ROTATION 1  // Paysage
    
#elif defined(HARDWARE_LCDWIKI_S3)
    // ========== LCDWIKI ESP32-S3 2.8INCH ==========
    #define TFT_BL 45
    #define TOUCH_SDA 16
    #define TOUCH_SCL 15
    #define TOUCH_RST 18
    #define TOUCH_IRQ 17
    #define RGB_LED_PIN 42
    #define USE_FT6336U_TOUCH 1
    #define SCREEN_ROTATION 0  // Portrait
    
#else
    #error "Aucun hardware défini! Utilisez -DHARDWARE_SUNTON ou -DHARDWARE_LCDWIKI_S3"
#endif

#endif
```

### 2. Modifier `src/main.cpp`

Remplacer les définitions de pins par l'include:

```cpp
// Au début du fichier, remplacer:
// #define TFT_BL 21
// #define TOUCH_CS 33
// ...

// Par:
#include "hardware_config.h"

// Puis conditionner l'initialisation:
#ifdef USE_XPT2046_TOUCH
    #include <XPT2046_Touchscreen.h>
    XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);
#elif defined(USE_FT6336U_TOUCH)
    #include <FT6336U.h>
    #include <Wire.h>
    FT6336U ft6336u(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_IRQ);
#endif
```

### 3. Adapter les fonctions hardware-spécifiques

```cpp
// Fonction setupTouch() conditionnelle
void setupTouch() {
#ifdef USE_XPT2046_TOUCH
    SPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    touch.begin();
    touch.setRotation(SCREEN_ROTATION);
#elif defined(USE_FT6336U_TOUCH)
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    ft6336u.begin();
#endif
}

// Fonction readTouch() conditionnelle
bool readTouch(int &x, int &y) {
#ifdef USE_XPT2046_TOUCH
    if (!touch.touched()) return false;
    TS_Point p = touch.getPoint();
    if (p.z < 60) return false;
    x = map(p.x, 200, 3700, 0, 320);
    y = map(p.y, 200, 3700, 0, 240);
    return true;
#elif defined(USE_FT6336U_TOUCH)
    if (!ft6336u.read_touch()) return false;
    x = ft6336u.get_touch_x();
    y = ft6336u.get_touch_y();
    return true;
#else
    return false;
#endif
}
```

## 🔧 Modification du platformio.ini

Remplacer le `platformio.ini` existant par:

```ini
[platformio]
default_envs = sunton-2432s028r

[env]
platform = espressif32
framework = arduino
monitor_speed = 115200
lib_deps = 
    bodmer/TFT_eSPI@^2.5.43
    bblanchon/ArduinoJson@^7.2.0
    ricmoo/QRCode@^0.0.1
    bitbank2/PNGdec@^1.1.6

[env:sunton-2432s028r]
board = esp32dev
lib_deps = 
    ${env.lib_deps}
    paulstoffregen/XPT2046_Touchscreen
build_flags = 
    -DUSER_SETUP_LOADED=1
    -DST7789_DRIVER=1
    -DTFT_WIDTH=240
    -DTFT_HEIGHT=320
    -DTFT_MISO=12
    -DTFT_MOSI=13
    -DTFT_SCLK=14
    -DTFT_CS=15
    -DTFT_DC=2
    -DTFT_RST=-1
    -DHARDWARE_SUNTON=1
    -DSPI_FREQUENCY=40000000

[env:lcdwiki-esp32s3-28p]
board = esp32-s3-devkitc-1
board_build.partitions = default_16MB.csv
lib_deps = 
    ${env.lib_deps}
    https://github.com/strange-v/FT6336U.git
    fastled/FastLED@^3.6.0
build_flags = 
    -DUSER_SETUP_LOADED=1
    -DILI9341_DRIVER=1
    -DTFT_WIDTH=240
    -DTFT_HEIGHT=320
    -DTFT_MISO=13
    -DTFT_MOSI=11
    -DTFT_SCLK=12
    -DTFT_CS=10
    -DTFT_DC=46
    -DTFT_RST=-1
    -DTFT_BL=45
    -DHARDWARE_LCDWIKI_S3=1
    -DSPI_FREQUENCY=80000000
upload_speed = 921600
```

## 🔄 Workflow GitHub Actions

Créer `.github/workflows/build-and-release.yml`:

```yaml
name: Build and Release

on:
  push:
    tags:
      - 'v*'

jobs:
  build:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        target: [sunton-2432s028r, lcdwiki-esp32s3-28p]
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: '3.11'
      - name: Install PlatformIO
        run: pip install platformio
      - name: Build ${{ matrix.target }}
        run: pio run -e ${{ matrix.target }}
      - name: Rename firmware
        run: |
          mkdir -p release
          cp .pio/build/${{ matrix.target }}/firmware.bin \
             release/degenbeat-${{ matrix.target }}.bin
      - name: Upload artifact
        uses: actions/upload-artifact@v4
        with:
          name: firmware-${{ matrix.target }}
          path: release/*.bin

  release:
    needs: build
    runs-on: ubuntu-latest
    permissions:
      contents: write
      pages: write
      id-token: write
    environment:
      name: github-pages
      url: ${{ steps.deployment.outputs.page_url }}
    steps:
      - uses: actions/checkout@v4
      - uses: actions/download-artifact@v4
        with:
          path: firmwares/builds
          merge-multiple: true
      - name: Create Release
        uses: softprops/action-gh-release@v1
        with:
          files: firmwares/builds/*.bin
          generate_release_notes: true
      - name: Setup Pages
        uses: actions/configure-pages@v4
      - name: Copy to _site
        run: |
          mkdir -p _site/firmwares/builds
          cp firmwares/builds/*.bin _site/firmwares/builds/
          cp firmwares/manifests/*.json _site/firmwares/
          cp -r webflasher/* _site/
      - name: Upload to Pages
        uses: actions/upload-pages-artifact@v3
        with:
          path: _site
      - name: Deploy
        uses: actions/deploy-pages@v4
```

## 🚀 Création de la première release multi-hardware

### Étape 1: Préparer le code

```bash
# Créer une branche feature
git checkout -b feature/multi-hardware

# Copier les nouveaux fichiers
cp -r /path/to/DegenBeat-ESP32S3-Adapted/webflasher .
cp -r /path/to/DegenBeat-ESP32S3-Adapted/firmwares .
mkdir -p .github/workflows
cp /path/to/DegenBeat-ESP32S3-Adapted/.github/workflows/build-firmwares.yml .github/workflows/

# Modifier src/main.cpp et include/hardware_config.h
# ... (voir instructions ci-dessus)

# Commit
git add .
git commit -m "Ajout support multi-hardware + webflasher"

# Push
git push origin feature/multi-hardware
```

### Étape 2: Pull Request et Merge

1. Créer une Pull Request sur GitHub
2. Vérifier que les tests passent
3. Merger dans `main`

### Étape 3: Créer la release

```bash
# Tagger la version
git checkout main
git pull origin main
git tag -a v1.1.0 -m "Release v1.1.0 - Support multi-hardware"
git push origin v1.1.0
```

### Étape 4: Configurer GitHub Pages

1. Aller dans **Settings > Pages**
2. Source: **GitHub Actions**
3. Attendre le déploiement
4. Accéder à: `https://silexperience210.github.io/DegenBeat/`

## 📋 Checklist

- [ ] Code source modifié avec `hardware_config.h`
- [ ] `platformio.ini` avec les deux environnements
- [ ] Fichiers manifest JSON créés
- [ ] Page webflasher créée
- [ ] Workflow GitHub Actions créé
- [ ] GitHub Pages configuré
- [ ] Test de compilation local réussi
- [ ] Première release créée
- [ ] Webflasher testé

## 🔗 URLs importantes

| Ressource | URL |
|-----------|-----|
| Repository | `https://github.com/Silexperience210/DegenBeat` |
| WebFlasher | `https://silexperience210.github.io/DegenBeat/` |
| Releases | `https://github.com/Silexperience210/DegenBeat/releases` |

## 💡 Conseils

1. **Testez localement** avant de pousser: `pio run -e sunton-2432s028r` et `pio run -e lcdwiki-esp32s3-28p`
2. **Versioning**: Utilisez Semantic Versioning (ex: v1.1.0)
3. **Changelog**: Maintenez un CHANGELOG.md à jour
4. **Documentation**: Mettez à jour le README avec les nouveaux hardwares supportés
