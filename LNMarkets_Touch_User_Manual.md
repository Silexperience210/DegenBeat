# LN Markets Touch - User Manual
## Version 0.1 - Trading Terminal

---

## 🚀 QUICK START GUIDE

### ⚠️ IMPORTANT: LN MARKETS ACCOUNT SETUP (Do this FIRST!)

**Before configuring the device, you MUST create an LN Markets account and API keys:**

1. **Create Account**: Go to [lnmarkets.com](https://lnmarkets.com) and create a free account
2. **Fund Account**: Deposit funds using the Lightning address shown in your profile
3. **Generate API Keys**:
   - Go to **Profile** → **API V3** tab
   - Click **"Create New Keys"**
   - **Grant ALL permissions** (check all boxes)
   - Click **OK** to generate keys
4. **Save Credentials**: Immediately save ALL data on your smartphone:
   - **API Key** (long base64 string)
   - **API Secret** (long base64 string)
   - **API Passphrase** (alphanumeric string)
   - **Lightning Address** (shown in your profile, format: user@lnmarkets.com)
5. **Test Keys**: Make a small deposit to verify your Lightning address works

**⚠️ WARNING**: Without API keys, you can only view prices. Trading requires API access.

### 1. First Boot Configuration
- **Power on** the device
- **Connect to WiFi** "LNMarkets-Touch" network
- **Open browser** and go to `192.168.4.1`
- **Enter your settings:**
  - WiFi network name and password
  - LN Markets API credentials (from step 5 above)
  - Lightning addresses (from step 5 above)
- **Save configuration** - device will restart

### 2. Main Interface
- **HOME**: Bitcoin price, trading controls
- **WALLET**: Balance management, deposits/withdrawals
- **POSITIONS**: Open trades and orders
- **HISTORY**: Trade history and deposits

### 3. Trading
- **Select side**: BUY or SELL button
- **Choose leverage**: 25x, 50x, 75x, or 100x
- **Set margin**: Use quick buttons or +/- controls
- **Execute**: Press EXECUTE button

### 4. Fund Management
- **Deposit**: Generate Lightning invoice
- **Withdraw**: Send to Lightning address
- **History**: View all transactions

---

## 📱 DETAILED USER GUIDE

### Configuration Portal
When first powered on, the device creates a WiFi hotspot:
- **Network**: LNMarkets-Touch
- **IP**: 192.168.4.1

**Required Settings:**
- WiFi SSID and password for internet connection
- **LN Markets API key, secret, and passphrase** (MANDATORY for trading - from account setup above)

**Optional Settings:**
- Lightning withdrawal address (format: user@domain.com)
- Lightning deposit address (format: user@domain.com)

**⚠️ NOTE**: Without API keys, you can only view prices in demo mode. Trading requires API access.

### Main Screen (HOME)
**Top Navigation:**
- HOME (red when active)
- WALLET, POSITIONS, HISTORY tabs

**Bitcoin Price Display:**
- Real-time BTC/USD price
- Updates every 2 seconds

**Trading Controls:**
- **BUY/SELL buttons**: Select trade direction
- **Leverage buttons**: 25x, 50x, 75x, 100x
- **Quick margin**: 100, 500, 1000, 5000 sats
- **Margin controls**: +/- buttons (100 sats steps)
- **EXECUTE button**: Place the trade

### Wallet Screen
**Balance Display:**
- Current sats balance
- USD equivalent

**Withdrawal Controls:**
- **Quick amounts**: 100, 500, 1000, 2000 sats
- **+/- buttons**: Adjust withdrawal amount
- **WITHDRAW button**: Send to Lightning address

**Deposit Controls:**
- **DEPOSIT button**: Generate Lightning invoice
- **DEPOSIT HISTORY**: View past deposits

**Settings:**
- **RESET CONFIG**: Clear all settings (bottom right)

### Positions Screen
**Running Positions:**
- Shows active trades
- P&L percentage and sats
- **CLOSE button**: Exit position

**Open Orders:**
- Pending limit orders
- **CANCEL button**: Remove order

### History Screen
**Closed Trades:**
- Completed positions
- Final P&L results
- Close dates

**Lightning Deposits:**
- Payment history
- Amounts and timestamps

---

## ⚠️ IMPORTANT SAFETY NOTES

### Trading Risks
- **Bitcoin futures trading involves substantial risk**
- **You may lose all your invested capital**
- **Only trade with money you can afford to lose**
- **Past performance does not guarantee future results**

### Technical Notes
- **Version 0.1**: May contain bugs or unexpected behavior
- **Device may reboot** during operation
- **Always backup** your API credentials
- **Test with small amounts** first

### Security
- **Keep your device secure** - physical access = full access
- **Use strong WiFi passwords**
- **Regularly update** API credentials if compromised
- **Monitor your positions** - no guaranteed execution

---

## 🔧 TROUBLESHOOTING

### No WiFi Connection
- Check WiFi credentials in configuration
- Restart device and reconfigure

### API Connection Failed
- Verify LN Markets API credentials
- Check internet connection
- Try public mode (no API keys)

### Touch Not Responding
- Clean screen surface
- Press firmly on buttons
- Restart device if unresponsive

### Trading Errors
- Check available balance
- Verify leverage limits
- Confirm API permissions

### Device Reboots
- Normal during heavy network activity
- May indicate memory issues
- Restart and try again

---

## 📞 SUPPORT

For issues or questions:
- Check LN Markets documentation: lnmarkets.com
- Community forums: discord.gg/lnmarkets
- GitHub issues: github.com/lnmarkets/touch-terminal

---

---

---

# Terminal Tactile LN Markets - Notice d'Utilisation
## Version 0.1 - Terminal de Trading

---

## 🚀 GUIDE DE DÉMARRAGE RAPIDE

### ⚠️ IMPORTANT: CONFIGURATION COMPTE LN MARKETS (À FAIRE EN PREMIER!)

**Avant de configurer l'appareil, vous DEVEZ créer un compte LN Markets et des clés API:**

1. **Créer un Compte**: Allez sur [lnmarkets.com](https://lnmarkets.com) et créez un compte gratuit
2. **Alimenter le Compte**: Déposez des fonds en utilisant l'adresse Lightning affichée dans votre profil
3. **Générer les Clés API**:
   - Allez dans **Profil** → onglet **API V3**
   - Cliquez sur **"Create New Keys"**
   - **Accordez TOUTES les permissions** (cochez toutes les cases)
   - Cliquez sur **OK** pour générer les clés
4. **Sauvegardez les Identifiants**: Sauvegardez IMMÉDIATEMENT toutes les données sur votre smartphone:
   - **Clé API** (longue chaîne base64)
   - **Secret API** (longue chaîne base64)
   - **Passphrase API** (chaîne alphanumérique)
   - **Adresse Lightning** (affichée dans votre profil, format: user@lnmarkets.com)
5. **Tester les Clés**: Faites un petit dépôt pour vérifier que votre adresse Lightning fonctionne

**⚠️ ATTENTION**: Sans clés API, vous ne pouvez que voir les prix. Le trading nécessite l'accès API.

### 1. Configuration au Premier Démarrage
- **Allumez** l'appareil
- **Connectez-vous au WiFi** "LNMarkets-Touch"
- **Ouvrez votre navigateur** et allez sur `192.168.4.1`
- **Saisissez vos paramètres :**
  - Nom et mot de passe du réseau WiFi
  - Clés API LN Markets (de l'étape 5 ci-dessus)
  - Adresses Lightning (de l'étape 5 ci-dessus)
- **Sauvegardez la configuration** - l'appareil redémarrera

### 2. Interface Principale
- **HOME** : Prix Bitcoin, contrôles de trading
- **WALLET** : Gestion du solde, dépôts/retraits
- **POSITIONS** : Trades ouverts et ordres
- **HISTORIQUE** : Historique des trades et dépôts

### 3. Trading
- **Sélectionnez le côté** : Bouton BUY ou SELL
- **Choisissez le levier** : 25x, 50x, 75x, ou 100x
- **Définissez la marge** : Boutons rapides ou contrôles +/-
- **Exécutez** : Appuyez sur le bouton EXECUTE

### 4. Gestion des Fonds
- **Dépôt** : Générez une facture Lightning
- **Retrait** : Envoyez vers une adresse Lightning
- **Historique** : Consultez toutes les transactions

---

## 📱 GUIDE UTILISATEUR DÉTAILLÉ

### Portail de Configuration
Au premier démarrage, l'appareil crée un hotspot WiFi :
- **Réseau** : LNMarkets-Touch
- **IP** : 192.168.4.1

**Paramètres Obligatoires :**
- SSID et mot de passe WiFi pour la connexion internet
- **Clé API, secret et passphrase LN Markets** (OBLIGATOIRE pour le trading - de la configuration compte ci-dessus)

**Paramètres Optionnels :**
- Adresse Lightning de retrait (format : utilisateur@domaine.com)
- Adresse Lightning de dépôt (format : utilisateur@domaine.com)

**⚠️ NOTE**: Sans clés API, vous ne pouvez que voir les prix en mode démo. Le trading nécessite l'accès API.

### Écran Principal (HOME)
**Navigation Supérieure :**
- HOME (rouge quand actif)
- Onglets WALLET, POSITIONS, HISTORY

**Affichage Prix Bitcoin :**
- Prix BTC/USD temps réel
- Mise à jour toutes les 2 secondes

**Contrôles de Trading :**
- **Boutons BUY/SELL** : Sélectionnez la direction du trade
- **Boutons Levier** : 25x, 50x, 75x, 100x
- **Marges rapides** : 100, 500, 1000, 5000 sats
- **Contrôles marge** : Boutons +/- (pas de 100 sats)
- **Bouton EXECUTE** : Placez le trade

### Écran Wallet
**Affichage Solde :**
- Solde actuel en sats
- Équivalent en USD

**Contrôles de Retrait :**
- **Montants rapides** : 100, 500, 1000, 2000 sats
- **Boutons +/-** : Ajustez le montant de retrait
- **Bouton WITHDRAW** : Envoyez vers l'adresse Lightning

**Contrôles de Dépôt :**
- **Bouton DEPOSIT** : Générez une facture Lightning
- **DEPOSIT HISTORY** : Consultez les dépôts passés

**Paramètres :**
- **RESET CONFIG** : Effacez tous les paramètres (bas droite)

### Écran Positions
**Positions Ouvertes :**
- Affiche les trades actifs
- P&L en pourcentage et sats
- **Bouton CLOSE** : Sortez de la position

**Ordres Ouverts :**
- Ordres limite en attente
- **Bouton CANCEL** : Supprimez l'ordre

### Écran Historique
**Trades Fermés :**
- Positions terminées
- Résultats P&L finaux
- Dates de clôture

**Dépôts Lightning :**
- Historique des paiements
- Montants et horodatages

---

## ⚠️ NOTES DE SÉCURITÉ IMPORTANTES

### Risques de Trading
- **Le trading de futures Bitcoin comporte des risques substantiels**
- **Vous pouvez perdre tout votre capital investi**
- **Tradez uniquement avec de l'argent que vous pouvez vous permettre de perdre**
- **Les performances passées ne garantissent pas les résultats futurs**

### Notes Techniques
- **Version 0.1** : Peut contenir des bugs ou comportements inattendus
- **L'appareil peut redémarrer** pendant le fonctionnement
- **Sauvegardez toujours** vos clés API
- **Testez avec de petits montants** d'abord

### Sécurité
- **Gardez votre appareil sécurisé** - accès physique = accès complet
- **Utilisez des mots de passe WiFi forts**
- **Mettez à jour régulièrement** les clés API si compromises
- **Surveillez vos positions** - pas d'exécution garantie

---

## 🔧 DÉPANNAGE

### Pas de Connexion WiFi
- Vérifiez les identifiants WiFi dans la configuration
- Redémarrez l'appareil et reconfigurez

### Connexion API Échouée
- Vérifiez les clés API LN Markets
- Vérifiez la connexion internet
- Essayez le mode public (sans clés API)

### Tactile Ne Répond Pas
- Nettoyez la surface de l'écran
- Appuyez fermement sur les boutons
- Redémarrez l'appareil s'il ne répond pas

### Erreurs de Trading
- Vérifiez le solde disponible
- Vérifiez les limites de levier
- Confirmez les permissions API

### Redémarrages de l'Appareil
- Normal pendant une activité réseau intense
- Peut indiquer des problèmes de mémoire
- Redémarrez et réessayez

---

## 📞 SUPPORT

Pour les problèmes ou questions :
- Consultez la documentation LN Markets : lnmarkets.com
- Forums communautaires : discord.gg/lnmarkets
- Issues GitHub : github.com/lnmarkets/touch-terminal

---

**⚠️ AVERTISSEMENT SUR LES JEUX D'ARGENT**

Le trading de crypto-monnaies, y compris les futures Bitcoin, est considéré comme un jeu d'argent dans de nombreux pays. Cette activité comporte des risques élevés de perte financière. LN Markets Touch est un outil éducatif et de recherche - utilisez-le avec modération et responsabilité. Si vous souffrez d'addiction au jeu, contactez les services d'aide appropriés dans votre pays.

**Avertissement : Cette version 0.1 peut contenir des bugs mineurs ou redémarrages inattendus. Utilisez-la uniquement pour des tests et avec de petits montants.**