# LN Markets Touch - Terminal de Trading ESP32

## Description
Terminal de trading Bitcoin avec interface tactile pour LN Markets, incluant un indicateur LED RGB P&L et support carte SD pour images de démarrage.

## Fonctionnalités
- ✅ Interface tactile 320x240 pixels
- ✅ Connexion API LN Markets v3
- ✅ Affichage temps réel des prix BTC
- ✅ Gestion des positions et ordres
- ✅ Indicateur LED RGB 5 niveaux basé sur P&L
- ✅ **Support carte SD pour images de démarrage**

## Support Carte SD

### Configuration Matérielle
Le Sunton ESP32-2432S028R dispose d'un slot carte SD. La carte SD utilise les pins suivants :
- SD_MMC (mode 1-bit pour compatibilité maximale)

### Images de Démarrage
Au démarrage, l'appareil recherche automatiquement les fichiers suivants sur la carte SD :
1. `/boot.bmp` - Image BMP de démarrage (priorité haute)
2. `/logo.bmp` - Image BMP alternative (priorité moyenne)

### Format des Images
- **Format** : BMP 24-bit (non compressé)
- **Résolution** : Recommandée 320x240 pixels (plein écran)
- **Taille** : Toute taille, sera automatiquement ajustée/coupée
- **Durée** : 5 secondes d'affichage

### Comment Ajouter des Images
1. Créez ou convertissez votre image en format BMP 24-bit
2. Redimensionnez si nécessaire (320x240 recommandé)
3. Nommez le fichier `boot.bmp` ou `logo.bmp`
4. Copiez sur la carte SD à la racine
5. Insérez la carte SD dans l'ESP32
6. Redémarrez l'appareil

### Outils pour Créer des BMP
- **GIMP** : Exportez en BMP 24-bit
- **Photoshop** : Save As → BMP → 24-bit
- **Online converters** : De nombreux outils gratuits existent

### Logs Série
L'appareil affiche des messages sur le port série :
```
Initialisation carte SD...OK!
Affichage BMP de démarrage: /boot.bmp
Loading image '/boot.bmp'
Image size: 320x240
Loaded in 1250 ms
```

## Installation
1. Clonez ce repository
2. Ouvrez dans PlatformIO
3. Configurez vos credentials WiFi et LN Markets
4. Compilez et uploadez sur ESP32

## Configuration API
Modifiez les variables dans le code :
```cpp
String api_key = "votre_cle_api";
String api_secret = "votre_secret";
String api_passphrase = "votre_passphrase";
```

## LED RGB Indicateur P&L
- **Éteint** : Aucune position
- **Clignotement lent** : P&L < 5%
- **Clignotement normal** : P&L 5-25%
- **Clignotement rapide** : P&L 25-50%
- **Clignotement très rapide** : P&L 50-100%
- **Continu vert** : P&L > 100% (gains)
- **Continu rouge** : P&L < -100% (pertes)

## Dépannage Carte SD
- Vérifiez que la carte SD est formatée en FAT32
- Assurez-vous que les fichiers sont à la racine
- Vérifiez les logs série pour les messages d'erreur
- Testez avec une carte SD différente si nécessaire

## Schéma des Pins (Sunton ESP32-2432S028R)
- TFT: Pins 13,12,14,15,2
- Touch: Pins 32,33,25,36
- LED RGB: Pins 4,16,17
- SD: Utilise SD_MMC (pins dédiées)

---
**Note**: Cette implémentation utilise SD_MMC en mode 1-bit pour une meilleure compatibilité avec l'ESP32.</content>
<parameter name="filePath">README.md