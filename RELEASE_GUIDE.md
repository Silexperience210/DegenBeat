# Guide de Release Multi-Hardware

Ce guide explique comment créer une release GitHub avec plusieurs firmwares et un webflasher intégré.

## 📁 Structure du projet

```
DegenBeat/
├── .github/
│   └── workflows/
│       └── build-firmwares.yml    # CI/CD pour builds auto
├── firmwares/
│   ├── manifest-sunton-2432s028r.json
│   ├── manifest-lcdwiki-esp32s3-28p.json
│   ├── degenbeat-sunton-2432s028r.bin      # (généré)
│   └── degenbeat-lcdwiki-esp32s3-28p.bin   # (généré)
├── webflasher/
│   └── index.html                  # Page webflasher avec sélecteur
├── src/
│   └── main.cpp                    # Code source unique
├── platformio-multi.ini            # Config pour tous hardwares
└── platformio.ini                  # Config active (copiée)
```

## 🚀 Processus de Release

### 1. Préparation du code

Assurez-vous que le code source supporte les deux hardwares via des directives `#ifdef`:

```cpp
#ifdef HARDWARE_SUNTON
    // Code pour Sunton
#elif defined(HARDWARE_LCDWIKI_S3)
    // Code pour LCDWiki ESP32-S3
#endif
```

### 2. Création d'un tag (déclenche la CI/CD)

```bash
# Créer un tag versionné
git tag -a v1.1.0 -m "Release v1.1.0 - Support LCDWiki ESP32-S3"

# Pousser le tag
git push origin v1.1.0
```

### 3. Ce qui se passe automatiquement

Le workflow GitHub Actions va:
1. **Compiler** les deux firmwares en parallèle
2. **Créer une release GitHub** avec les fichiers `.bin`
3. **Déployer** le webflasher sur GitHub Pages

### 4. Configuration GitHub Pages

1. Allez dans **Settings > Pages** du repository
2. Source: **GitHub Actions**
3. Le webflasher sera disponible à: `https://silexperience210.github.io/DegenBeat/`

## 🔧 Configuration manuelle (sans CI/CD)

Si vous préférez compiler localement:

### Compiler pour Sunton ESP32-2432S028R

```bash
# Copier la config
cp platformio-multi.ini platformio.ini

# Éditer pour ne garder que [env:sunton-esp32-2432s028r]
# Puis compiler
pio run -e sunton-esp32-2432s028r

# Récupérer le firmware
cp .pio/build/sunton-esp32-2432s028r/firmware.bin \
   firmwares/degenbeat-sunton-2432s028r.bin
```

### Compiler pour LCDWiki ESP32-S3

```bash
# Copier la config
cp platformio-multi.ini platformio.ini

# Éditer pour ne garder que [env:lcdwiki-esp32s3-28p]
# Puis compiler
pio run -e lcdwiki-esp32s3-28p

# Récupérer le firmware
cp .pio/build/lcdwiki-esp32s3-28p/firmware.bin \
   firmwares/degenbeat-lcdwiki-esp32s3-28p.bin
```

### Créer la release manuellement

1. Allez sur GitHub > Releases > Draft new release
2. Choisissez ou créez un tag (ex: `v1.1.0`)
3. Titre: "DegenBeat v1.1.0 - Multi-Hardware"
4. Décrivez les changements
5. Uploadez les fichiers `.bin` et `.json`
6. Publiez

## 🌐 WebFlasher

### Fonctionnement

Le webflasher utilise **ESP Web Tools** pour flasher directement depuis le navigateur:

1. **Sélection hardware**: L'utilisateur choisit son board
2. **Manifest JSON**: Charge le manifest correspondant
3. **Flash**: ESP Web Tools gère le flashage via WebSerial

### Ajouter un nouveau hardware

Pour ajouter un 3ème hardware (ex: CYD):

1. **Créer l'environnement** dans `platformio-multi.ini`:
```ini
[env:cyd-esp32-2432s028]
platform = espressif32
board = esp32dev
framework = arduino
build_flags = 
    -DHARDWARE_CYD=1
    ...
```

2. **Créer le manifest** `firmwares/manifest-cyd.json`:
```json
{
  "name": "DegenBeat - CYD ESP32",
  "version": "1.0.0",
  "builds": [
    {
      "chipFamily": "ESP32",
      "parts": [{"path": "degenbeat-cyd.bin", "offset": 0}]
    }
  ]
}
```

3. **Ajouter au webflasher** dans `webflasher/index.html`:
```html
<div class="hardware-card" 
     data-manifest="../firmwares/manifest-cyd.json" 
     data-name="CYD ESP32" 
     onclick="selectHardware(this)">
    <span class="chip-badge">ESP32</span>
    <div class="hardware-name">CYD ESP32</div>
    <ul class="hardware-specs">
        <li>Display ST7789</li>
        ...
    </ul>
</div>
```

4. **Mettre à jour le workflow** `.github/workflows/build-firmwares.yml`:
```yaml
  build-cyd:
    runs-on: ubuntu-latest
    steps:
      ...
      - name: Build CYD
        run: pio run -e cyd-esp32-2432s028
```

## 📦 Fichiers de release

Chaque release doit contenir:

| Fichier | Description |
|---------|-------------|
| `degenbeat-sunton-2432s028r.bin` | Firmware Sunton |
| `degenbeat-lcdwiki-esp32s3-28p.bin` | Firmware LCDWiki S3 |
| `manifest-*.json` | Manifests pour webflasher |
| `Source code` | Archive auto-générée par GitHub |

## 🔍 Dépannage

### Le firmware ne démarre pas

Vérifiez que le bon `chipFamily` est utilisé dans le manifest:
- ESP32 classique: `"chipFamily": "ESP32"`
- ESP32-S3: `"chipFamily": "ESP32-S3"`

### WebFlasher ne détecte pas l'appareil

- Utilisez Chrome, Edge ou Opera (desktop)
- Le WebSerial nécessite une connexion HTTPS ou localhost
- Sur GitHub Pages, assurez-vous que le site est en HTTPS

### Erreur de compilation

Vérifiez que toutes les librairies sont installées:
```bash
pio lib install
```

## 📚 Ressources

- [ESP Web Tools Documentation](https://esphome.github.io/esp-web-tools/)
- [PlatformIO Documentation](https://docs.platformio.org/)
- [GitHub Actions Documentation](https://docs.github.com/actions)
