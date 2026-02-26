# Test du WebFlasher en Local

Ce guide explique comment tester le webflasher localement avant de le déployer sur GitHub Pages.

## 🚀 Méthode 1: Python HTTP Server (Simple)

### Prérequis
- Python 3.x installé

### Lancer le serveur

```bash
# Naviguer vers le dossier contenant le webflasher
cd DegenBeat-ESP32S3-Adapted

# Créer une structure temporaire de test
mkdir -p _test/firmwares
cp -r webflasher/* _test/
cp firmwares/*.json _test/firmwares/

# Placer les firmwares compilés (si disponibles)
# cp .pio/build/.../firmware.bin _test/firmwares/degenbeat-sunton-2432s028r.bin
# cp .pio/build/.../firmware.bin _test/firmwares/degenbeat-lcdwiki-esp32s3-28p.bin

# Lancer le serveur Python
cd _test
python -m http.server 8000
```

### Accéder au webflasher

Ouvrez votre navigateur à:
```
http://localhost:8000
```

⚠️ **Note**: WebSerial nécessite HTTPS ou localhost. Avec `localhost:8000`, cela devrait fonctionner.

## 🚀 Méthode 2: Node.js http-server (Avec HTTPS)

### Prérequis
- Node.js installé

### Installation

```bash
# Installer http-server globalement
npm install -g http-server

# Générer un certificat SSL auto-signé
openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 365 -nodes
```

### Lancer avec HTTPS

```bash
cd DegenBeat-ESP32S3-Adapted

# Créer la structure de test
mkdir -p _test/firmwares
cp -r webflasher/* _test/
cp firmwares/*.json _test/firmwares/

# Lancer avec HTTPS
cd _test
http-server -S -C ../cert.pem -K ../key.pem -p 8443
```

### Accéder

```
https://localhost:8443
```

Acceptez l'avertissement de sécurité du certificat auto-signé.

## 🚀 Méthode 3: VS Code Live Server

### Extension
Installez l'extension **"Live Server"** de Ritwick Dey.

### Configuration

1. Créer un fichier `.vscode/settings.json`:
```json
{
    "liveServer.settings.port": 5500,
    "liveServer.settings.root": "/webflasher",
    "liveServer.settings.https": {
        "enable": true,
        "cert": "cert.pem",
        "key": "key.pem",
        "passphrase": ""
    }
}
```

2. Clic droit sur `webflasher/index.html` → "Open with Live Server"

## 🧪 Test des fonctionnalités

### 1. Test de sélection hardware
- Cliquez sur chaque carte hardware
- Vérifiez que la sélection visuelle change
- Vérifiez que le texte "Hardware sélectionné" s'affiche

### 2. Test du bouton de flashage
- Le bouton "Connecter et Flasher" doit s'activer après sélection
- Vérifiez dans la console DevTools (F12) qu'il n'y a pas d'erreurs

### 3. Test WebSerial (Chrome/Edge)
- Connectez un ESP32 en USB
- Cliquez sur "Connecter et Flasher"
- Une popup de sélection de port série doit apparaître
- Sélectionnez le port correspondant à votre ESP32

### 4. Test des manifests
Vérifiez dans la console que les manifests se chargent:
```javascript
// Dans la console DevTools
fetch('../firmwares/manifest-sunton-2432s028r.json')
  .then(r => r.json())
  .then(console.log)
```

## 🔧 Débogage

### Problème: "La page n'est pas sécurisée"
**Solution**: WebSerial nécessite un contexte sécurisé. Utilisez:
- `http://localhost` (autorisé implicitement)
- `https://` avec un certificat valide

### Problème: "Cannot read property 'serial' of undefined"
**Cause**: Le navigateur ne supporte pas WebSerial
**Solution**: Utilisez Chrome 89+, Edge 89+, ou Opera 75+

### Problème: Le manifest ne se charge pas
**Vérifiez**:
- Les chemins dans les attributs `data-manifest`
- Que le serveur sert bien les fichiers JSON
- Dans Network DevTools que la requête retourne 200

### Problème: "Failed to fetch"
**Cause**: CORS ou fichier non trouvé
**Solution**:
```python
# Avec Python, ajoutez un handler CORS
python -m http.server 8000 --directory _test
```

Ou créez un serveur Python avec CORS:
```python
# server.py
from http.server import HTTPServer, SimpleHTTPRequestHandler
import os

class CORSRequestHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Access-Control-Allow-Origin', '*')
        super().end_headers()

os.chdir('_test')
httpd = HTTPServer(('localhost', 8000), CORSRequestHandler)
print("Serving on http://localhost:8000")
httpd.serve_forever()
```

## 📱 Test sur mobile

Le webflasher ne fonctionne **pas** sur mobile car WebSerial n'est pas supporté.
Cependant, vous pouvez tester l'interface:

```bash
# Rendre accessible sur le réseau local
python -m http.server 8000 --bind 0.0.0.0
```

Puis accédez via l'IP de votre PC:
```
http://192.168.1.XXX:8000
```

Le message "Navigateur non supporté" devrait s'afficher sur mobile.

## 📝 Checklist de test

- [ ] Page se charge sans erreurs console
- [ ] Design responsive (tester resize fenêtre)
- [ ] Sélection hardware fonctionne
- [ ] Bouton de flashage s'active après sélection
- [ ] Connexion USB détecte l'ESP32
- [ ] Processus de flashage complet fonctionne
- [ ] Message "non supporté" sur Safari/Firefox

## 🐛 Outils de développement

### Simuler un ESP32 (pour tests UI)
Créez un faux device serial:
```javascript
// Dans la console DevTools
navigator.serial = {
    requestPort: () => Promise.resolve({
        open: () => Promise.resolve(),
        close: () => Promise.resolve(),
        writable: { getWriter: () => ({ write: () => {}, releaseLock: () => {} }) },
        readable: { getReader: () => ({ read: () => Promise.resolve({ done: true }) }) }
    })
};
```

### Vérifier la structure du manifest
```bash
curl http://localhost:8000/firmwares/manifest-sunton-2432s028r.json | jq
```

### Tester les binaires
```bash
# Vérifier la taille et l'intégrité
ls -lh _test/firmwares/*.bin
file _test/firmwares/*.bin
```

## ✅ Avant le déploiement

1. Testez sur différents navigateurs (Chrome, Edge)
2. Vérifiez que les liens manifest sont corrects
3. Assurez-vous que les firmwares sont à jour
4. Testez le flashage réel sur chaque hardware
