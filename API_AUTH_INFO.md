# LN Markets - Authentification API

## ✅ CORRECTIONS EFFECTUÉES

### 1. Ajout du support d'authentification HMAC-SHA256
- Implémentation de la signature selon la doc LN Markets
- Headers requis : `LNM-ACCESS-KEY`, `LNM-ACCESS-PASSPHRASE`, `LNM-ACCESS-TIMESTAMP`, `LNM-ACCESS-SIGNATURE`

### 2. Formulaire de configuration mis à jour
- Champs ajoutés : API Key, API Secret, Passphrase
- Tous les champs API sont **optionnels**
- Si non renseignés, utilise l'API publique (ticker uniquement)

### 3. Fonctions ajoutées au code

#### `generateSignature(method, path, data, timestamp)`
Génère la signature HMAC-SHA256 selon le format LN Markets :
```
payload = timestamp + method + "/v2" + path + data
signature = base64(HMAC-SHA256(payload, api_secret))
```

#### `makeAuthenticatedRequest(method, path, data)`
Effectue des requêtes HTTP authentifiées vers l'API LN Markets.
Gère automatiquement :
- L'ajout des headers d'authentification si clés présentes
- Les méthodes GET, POST, PUT, DELETE
- Le mode public si pas de clés

## 📋 OBTENIR VOS CLÉS API

1. Allez sur **https://lnmarkets.com**
2. Connectez-vous à votre compte
3. Settings → API
4. Créez une nouvelle clé API
5. Notez :
   - **API Key** (commence par `lnm_...`)
   - **API Secret** 
   - **Passphrase**

⚠️ **IMPORTANT** : Ne partagez JAMAIS ces informations !

## 🔧 UTILISATION DANS LE CODE

### API Publique (sans auth)
```cpp
HTTPClient http;
http.begin("https://api.lnmarkets.com/v2/futures/ticker");
int code = http.GET();
String response = http.getString();
```

### API Privée (avec auth)
```cpp
// Exemple : Récupérer info utilisateur
String response = makeAuthenticatedRequest("GET", "/user", "");

// Exemple : Ouvrir un trade
String data = "{\"type\":\"m\",\"side\":\"b\",\"margin\":10000,\"leverage\":25}";
String response = makeAuthenticatedRequest("POST", "/futures", data);

// Exemple : Fermer un trade
String response = makeAuthenticatedRequest("DELETE", "/futures?id=trade_id", "");
```

## 📡 ENDPOINTS UTILES

### Publics (pas besoin d'auth)
- `GET /futures/ticker` - Prix BTC en temps réel
- `GET /futures/history/index` - Historique index
- `GET /futures/market` - Info marché

### Privés (besoin auth)
- `GET /user` - Info compte utilisateur
- `GET /user/deposit` - Historique dépôts
- `POST /user/withdraw` - Retrait sats
- `GET /futures` - Liste des trades
- `POST /futures` - Ouvrir un trade
- `DELETE /futures` - Fermer un trade
- `POST /futures/add-margin` - Ajouter marge
- `POST /futures/cash-in` - Cash-in PL

Documentation complète : **https://docs.lnmarkets.com**

## 🧪 TESTER L'AUTHENTIFICATION

Pour tester si vos clés fonctionnent, modifiez `testAPIConnection()` :

```cpp
void testAPIConnection() {
  // ... code existant ...
  
  // Test avec authentification
  if (api_key != "public") {
    String userInfo = makeAuthenticatedRequest("GET", "/user", "");
    Serial.println("User info: " + userInfo);
    
    JsonDocument doc;
    deserializeJson(doc, userInfo);
    
    if (doc["uid"]) {
      Serial.println("✓ Auth OK - UID: " + doc["uid"].as<String>());
    }
  }
}
```

## 🎯 PROCHAINES ÉTAPES

### Phase 2 : Wallet
- Afficher balance (sats + USD)
- Générer adresse BTC pour dépôt
- Fonction withdraw
- Historique transactions

### Phase 3 : Trading
- Boutons Long/Short
- Sélection leverage
- Take Profit / Stop Loss
- Liste positions actives
- Fermeture rapide

## 📝 NOTES IMPORTANTES

1. **Timestamp** : Le code utilise `millis()` qui compte depuis le boot. Pour une vraie app en production, utilisez un timestamp Unix réel via NTP.

2. **HTTPS** : L'ESP32 accepte les certificats auto-signés par défaut. Pour plus de sécurité, ajoutez la validation SSL.

3. **Rate Limiting** : LN Markets a des limites de requêtes. Évitez de spammer l'API.

4. **Mode Testnet** : Pour tester sans risque, utilisez `api.testnet.lnmarkets.com` au lieu de `api.lnmarkets.com`

## 🐛 DEBUG

Si vous obtenez une erreur 401 :
- Vérifiez que les 3 valeurs (key, secret, passphrase) sont correctes
- Contrôlez le format de la signature (base64 du HMAC-SHA256)
- Ajoutez des Serial.println() pour voir le payload signé
