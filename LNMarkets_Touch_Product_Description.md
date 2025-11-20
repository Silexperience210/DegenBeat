# LN Markets Touch Terminal
## Professional Bitcoin Futures Trading Terminal

**Version:** 1.0 Final (November 16, 2025)  
**Hardware:** ESP32 with 320KB RAM, 2.8" TFT Touch Display  
**Platform:** Arduino Framework with PlatformIO  

---

## 📋 Product Description

The LN Markets Touch Terminal is a compact, standalone Bitcoin futures trading device designed for professional traders. Built on the ESP32 microcontroller, this device provides direct access to LN Markets' advanced trading platform through an intuitive touchscreen interface.

### Key Features
- **Real-time Bitcoin price monitoring** with intelligent caching
- **One-click trading execution** with customizable leverage (25x-100x)
- **Lightning Network integration** for instant withdrawals and deposits
- **Secure API key management** with AES-256 encryption
- **Multi-screen navigation** (Home, Wallet, Positions, History)
- **Visual P&L indicator** with RGB LED feedback
- **WiFi configuration portal** for easy setup
- **QR code generation** for Lightning payments

### Technical Specifications
- **Microcontroller:** ESP32 (240MHz dual-core)
- **Memory:** 320KB RAM (15.5% utilized in final version)
- **Display:** 2.8" TFT LCD with resistive touchscreen (320x240)
- **Storage:** SPIFFS filesystem for configuration
- **Connectivity:** WiFi 802.11 b/g/n
- **Security:** AES-256 encryption, HMAC-SHA256 authentication
- **Power:** 5V USB (500mA recommended)
- **Dimensions:** Sunton ESP32-2432S028R form factor

### Performance Optimizations
- **RAM Usage:** 50,944 bytes (15.5% of 320KB)
- **FreeRTOS Stack:** 8KB (optimized for stability)
- **Cache System:** Multi-tier intelligent caching (2s-60s durations)
- **API Timeouts:** 3 seconds for responsive operation
- **Touch Response:** <200ms debounce for smooth interaction

---

## 🚀 Quick Start Guide

### Initial Setup
1. Power on the device via USB
2. Connect to WiFi network "LNMarkets-Touch"
3. Open browser to `192.168.4.1`
4. Configure WiFi credentials and LN Markets API keys
5. Device automatically connects and displays main interface

### Trading Interface
- **HOME Screen:** Real-time BTC price, trading controls
- **WALLET Screen:** Balance, withdrawal/deposit functions
- **POSITIONS Screen:** Active trades and open orders
- **HISTORY Screen:** Trade history and deposit records

### Security Features
- API keys encrypted with AES-256 using WiFi password as key
- Lightning addresses validated and secured
- Automatic cache invalidation after trades
- Secure HTTP requests with HMAC authentication

---

## 🔧 Development Information

### Build Requirements
- PlatformIO IDE
- ESP32 development board
- TFT_eSPI library (v2.5.43)
- ArduinoJson library (v7.4.2)

### Memory Optimization
The final version achieves optimal RAM usage through:
- Static buffers instead of dynamic allocation
- Intelligent caching system
- Reduced AES encryption buffers (256 bytes)
- Optimized data structures

### Stability Features
- Stack overflow protection (8KB FreeRTOS stack)
- Touch input debouncing
- Error handling and recovery
- Mutex-protected TFT operations

---

## 📞 Support & Documentation

For technical support and documentation:
- LN Markets API: https://lnmarkets.com
- ESP32 Documentation: https://docs.espressif.com
- PlatformIO: https://platformio.org

---

*LN Markets Touch Terminal - Professional Bitcoin Trading at Your Fingertips*

---

---

---

# Terminal Tactile LN Markets
## Terminal de Trading Professionnel de Futures Bitcoin

**Version :** 1.0 Finale (16 novembre 2025)  
**Matériel :** ESP32 avec 320KB RAM, Écran Tactile TFT 2.8"  
**Plateforme :** Framework Arduino avec PlatformIO  

---

## 📋 Description du Produit

Le Terminal Tactile LN Markets est un dispositif compact et autonome de trading de futures Bitcoin conçu pour les traders professionnels. Construit sur le microcontrôleur ESP32, cet appareil offre un accès direct à la plateforme de trading avancée de LN Markets via une interface tactile intuitive.

### Caractéristiques Principales
- **Surveillance des prix Bitcoin en temps réel** avec cache intelligent
- **Exécution de trades en un clic** avec levier personnalisable (25x-100x)
- **Intégration Lightning Network** pour retraits et dépôts instantanés
- **Gestion sécurisée des clés API** avec chiffrement AES-256
- **Navigation multi-écrans** (Accueil, Portefeuille, Positions, Historique)
- **Indicateur visuel P&L** avec retour LED RGB
- **Portail de configuration WiFi** pour une installation facile
- **Génération de QR codes** pour les paiements Lightning

### Spécifications Techniques
- **Microcontrôleur :** ESP32 (240MHz double-cœur)
- **Mémoire :** 320KB RAM (15.5% utilisé dans la version finale)
- **Écran :** TFT LCD 2.8" avec écran tactile résistif (320x240)
- **Stockage :** Système de fichiers SPIFFS pour la configuration
- **Connectivité :** WiFi 802.11 b/g/n
- **Sécurité :** Chiffrement AES-256, authentification HMAC-SHA256
- **Alimentation :** USB 5V (500mA recommandé)
- **Dimensions :** Format Sunton ESP32-2432S028R

### Optimisations de Performance
- **Utilisation RAM :** 50 944 octets (15.5% de 320KB)
- **Pile FreeRTOS :** 8KB (optimisée pour la stabilité)
- **Système de Cache :** Cache intelligent multi-niveaux (durées 2s-60s)
- **Timeouts API :** 3 secondes pour un fonctionnement réactif
- **Réponse Tactile :** <200ms d'anti-rebond pour une interaction fluide

---

## 🚀 Guide de Démarrage Rapide

### Configuration Initiale
1. Alimenter l'appareil via USB
2. Se connecter au réseau WiFi "LNMarkets-Touch"
3. Ouvrir le navigateur vers `192.168.4.1`
4. Configurer les identifiants WiFi et les clés API LN Markets
5. L'appareil se connecte automatiquement et affiche l'interface principale

### Interface de Trading
- **Écran ACCUEIL :** Prix BTC temps réel, contrôles de trading
- **Écran PORTEFEUILLE :** Solde, fonctions de retrait/dépôt
- **Écran POSITIONS :** Trades actifs et ordres ouverts
- **Écran HISTORIQUE :** Historique des trades et dépôts

### Fonctionnalités de Sécurité
- Clés API chiffrées avec AES-256 utilisant le mot de passe WiFi comme clé
- Adresses Lightning validées et sécurisées
- Invalidation automatique du cache après les trades
- Requêtes HTTP sécurisées avec authentification HMAC

---

## 🔧 Informations de Développement

### Prérequis de Compilation
- IDE PlatformIO
- Carte de développement ESP32
- Bibliothèque TFT_eSPI (v2.5.43)
- Bibliothèque ArduinoJson (v7.4.2)

### Optimisation Mémoire
La version finale atteint une utilisation optimale de la RAM grâce à :
- Buffers statiques au lieu d'allocation dynamique
- Système de cache intelligent
- Buffers de chiffrement AES réduits (256 octets)
- Structures de données optimisées

### Fonctionnalités de Stabilité
- Protection contre les débordements de pile (pile FreeRTOS 8KB)
- Anti-rebond des entrées tactiles
- Gestion et récupération d'erreurs
- Opérations TFT protégées par mutex

---

## 📞 Support & Documentation

Pour le support technique et la documentation :
- API LN Markets : https://lnmarkets.com
- Documentation ESP32 : https://docs.espressif.com
- PlatformIO : https://platformio.org

---

*Terminal Tactile LN Markets - Trading Professionnel Bitcoin au Bout des Doigts*</content>
<parameter name="filePath">c:\Users\Silex\Documents\PlatformIO\Projects\LNMarkets_Touch\LNMarkets_Touch_Product_Description.md