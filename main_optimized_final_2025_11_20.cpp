/*
 * LN MARKETS TOUCH - Phase 1 : Configuration & Connexion
 * Hardware : Sunton ESP32-2432S028R
 * 
 * TESTE D'ABORD CE CODE SIMPLE :
 * - Portail AP pour config WiFi + API
 * - Stockage SPIFFS
 * - Test connexion API
 * - Affichage prix BTC temps réel
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <mbedtls/md.h>
#include <mbedtls/aes.h>
#include <base64.h>
#include <time.h>
#include <PNGdec.h>
#include <qrcode.h>

// ===== PINS SUNTON ESP32-2432S028R =====
#define TFT_BL 21
#define TOUCH_CS 33
#define TOUCH_IRQ 36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK 25

// Carte SD (utilise HSPI)
#define SD_CS 15
#define SD_MOSI 13  // TFT_MOSI
#define SD_MISO 12  // TFT_MISO
#define SD_CLK 14   // TFT_SCLK

// LED RGB intégrée
#define LED_RED 4
#define LED_GREEN 16
#define LED_BLUE 17

// ===== OBJETS =====
TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);
WebServer server(80);
DNSServer dnsServer;

// Objet PNG global pour la callback
PNG pngGlobal;

// ===== VARIABLES =====
String ssid_saved = "";
String password_saved = "";
String api_key = "";
String api_secret = "";
String api_passphrase = "";
bool config_mode = false;
bool api_connected = false;
float btc_price = 0;

// Mutex pour protéger l'accès à l'écran TFT
SemaphoreHandle_t tft_mutex;

// Tâche FreeRTOS pour les mises à jour de prix
TaskHandle_t price_update_task = NULL;

// Navigation
enum Screen { SCREEN_HOME, SCREEN_WALLET, SCREEN_POSITIONS, SCREEN_HISTORY };
Screen current_screen = SCREEN_HOME;

// Wallet data
long balance_sats = 0;
float balance_usd = 0.0;

// Withdraw amount selection
long withdraw_amount_sats = 1000; // Default 1000 sats

// Deposit amount selection
long deposit_amount_sats = 1000; // Default 1000 sats

// Lightning address for withdrawals (LNURL)
String lightning_address = ""; // Default empty - user must configure

// Lightning address for deposits (LNURL)
String deposit_lnaddress = ""; // Default empty - user must configure

// Trade settings
int selected_leverage = 25; // Default leverage
String selected_side = ""; // Default no side selected
int selected_margin = 1000; // Default margin in sats

// Running trades data
int running_trades_count = 0;
long margin_in_positions = 0;
String running_trades_details[5]; // Store up to 5 trade details
float running_trades_pnl[5]; // Store P&L % for each trade
long running_trades_pnl_sats[5]; // Store P&L in satoshis for each trade
String running_trades_ids[5]; // Store position IDs for closing
float running_trades_entry[5]; // Store entry prices
int running_trades_leverage[5]; // Store leverage for closing
long running_trades_margin[5]; // Store margin for closing

// Open orders data
int open_orders_count = 0;
String open_orders_details[5]; // Store up to 5 open order details
String open_orders_ids[5]; // Store order IDs for canceling

// Closed trades data
int closed_trades_count = 0;
String closed_trades_details[10]; // Store up to 10 closed trade details
float closed_trades_pnl[10]; // Store P&L for each closed trade
String closed_trades_dates[10]; // Store close dates

// Lightning deposits history data
int lightning_deposits_count = 0;
String lightning_deposits_details[10]; // Store up to 10 deposit details
long lightning_deposits_amounts[10]; // Store amounts in sats
String lightning_deposits_dates[10]; // Store deposit dates
String lightning_deposits_hashes[10]; // Store payment hashes
String lightning_deposits_comments[10]; // Store comments

// ===== VARIABLES LED RGB =====
bool led_blink_state = false;
unsigned long last_led_blink = 0;
int blink_speed = 500; // Vitesse de clignotement en ms

// Variables pour LED configuration
bool config_led_blink_state = false;
unsigned long last_config_led_blink = 0;
int config_blink_speed = 800; // Vitesse plus lente pour config

// ===== CACHE WALLET =====
unsigned long last_wallet_update = 0;
const unsigned long WALLET_CACHE_DURATION = 10000; // 10 secondes de cache

// ===== CACHE INTELLIGENT =====
struct CacheEntry {
  String data;
  unsigned long timestamp;
  unsigned long duration;
  bool valid;
};

CacheEntry priceCache = {"", 0, 2000, false};        // Prix BTC: 2 secondes
CacheEntry walletCache = {"", 0, 12000, false};      // Wallet: 12 secondes  
CacheEntry positionsCache = {"", 0, 12000, false};   // Positions: 12 secondes
CacheEntry historyCache = {"", 0, 30000, false};     // Historique: 30 secondes
CacheEntry depositsCache = {"", 0, 60000, false};    // Dépôts: 60 secondes

// Fonctions utilitaires cache
bool isCacheValid(CacheEntry& cache) {
  if (!cache.valid) return false;
  return (millis() - cache.timestamp) < cache.duration;
}

void invalidateCache() {
  priceCache.valid = false;
  walletCache.valid = false;
  positionsCache.valid = false;
  historyCache.valid = false;
  depositsCache.valid = false;
  Serial.println("🗑️ Cache invalidé");
}

void updateCache(CacheEntry& cache, String data) {
  cache.data = data;
  cache.timestamp = millis();
  cache.valid = true;
}

// ===== VARIABLES QR CODE =====
int globalQrX = 80; // Position X par défaut du QR
int globalQrY = 50; // Position Y par défaut du QR

// ===== COULEURS =====
#define COLOR_BG 0x0010  // Bleu très foncé (thème sombre)
#define COLOR_RED 0xF800
#define COLOR_CYAN 0x07FF
#define COLOR_GREEN 0x07E0

// =====================================================
// FONCTION FADE OUT POUR TRANSITIONS
// =====================================================
void fadeOut(int duration = 150) {
  int steps = 10;
  for(int i = 0; i < steps; i++) {
    // Dessiner des bandes noires verticales progressives
    tft.fillRect(0, i * 240 / steps, 320, 240 / steps, TFT_BLACK);
    delay(duration / steps);
  }
}

// ===== PROTOTYPES FONCTIONS =====
void loadConfig();
bool saveConfig();
void startConfigPortal();
void handleRoot();
void handleSave();
void connectWiFi();
void testAPIConnection();
void showMainScreen();
void updatePrice();
void checkResetButton();
void showError(String msg);
String generateSignature(String method, String path, String data, String timestamp);
String makeAuthenticatedRequest(String method, String path, String data = "");
void showWalletScreen();
void showPositionsScreen();
void showHistoryScreen();
void updateWalletData();
void executeTrade(String side, int margin);
void handleTouch();
void withdrawBalance();
void syncTime();
void closeTrade(String positionId);
void cancelOrder(String orderId);
void getDepositAddress();
void showDepositQRScreen(String address);
void showDepositTextScreen(String address);
void showDepositManagementScreen();
void getDepositHistory();
void showDepositHistoryScreen();

// ===== PROTOTYPES FONCTIONS DE SÉCURITÉ =====
void deriveEncryptionKey(uint8_t* key, const char* password, size_t pass_len);
bool validateLightningAddress(String address);
bool validateApiKey(String key);
bool validateAmount(long amount, long min_val = 100, long max_val = 100000);
void updateLedPnl();
void setLedRGB(int red, int green, int blue);
size_t base64Decode(const char* input, size_t inputLen, uint8_t* output, size_t outputMaxLen);

// =====================================================
// TÂCHE FREERTOS POUR MISES À JOUR PRIX (CORE 1)
// =====================================================
void priceUpdateTask(void *pvParameters) {
  Serial.println("🚀 Tâche prix démarrée sur core 1");
  
  static unsigned long last_wallet_update = 0;
  
  while (true) {
    if (api_connected && current_screen == SCREEN_HOME) {
      // Prendre le mutex TFT avant d'accéder à l'écran
      if (xSemaphoreTake(tft_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        updatePrice();
        xSemaphoreGive(tft_mutex);
      }
    }
    
    // Mise à jour des données wallet toutes les 15 secondes (au lieu de 30)
    if (millis() - last_wallet_update > 15000) {
      if (api_connected && (current_screen == SCREEN_WALLET || current_screen == SCREEN_POSITIONS || current_screen == SCREEN_HISTORY)) {
        updateWalletData();
      }
      last_wallet_update = millis();
    }
    
    // Attendre 2 secondes pour une mise à jour plus fluide
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

// =====================================================
// SECURITY FUNCTIONS - PHASE 1
// =====================================================

// Derive encryption key from WiFi password using SHA-256
void deriveEncryptionKey(uint8_t* key, size_t keySize) {
  if (password_saved.length() == 0) {
    Serial.println("ERREUR: Pas de mot de passe WiFi pour dérivation de clé");
    memset(key, 0, keySize);
    return;
  }

  Serial.println("🔐 Dérivation de clé de chiffrement...");

  // Utiliser SHA-256 pour dériver une clé
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  uint8_t hash[32]; // Buffer temporaire pour le hash complet

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0); // Pas de HMAC
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char*)password_saved.c_str(), password_saved.length());
  mbedtls_md_finish(&ctx, hash);
  mbedtls_md_free(&ctx);

  // Copier seulement keySize octets (16 pour XOR, 32 pour AES)
  memcpy(key, hash, keySize);

  Serial.println("✅ Clé de chiffrement dérivée");
}

// =====================================================
// CHIFFREMENT XOR ULTRA-LÉGER (Solution 2)
// =====================================================

// Clé XOR dérivée du mot de passe WiFi (16 bytes)
const uint8_t XOR_KEY[16] = {
  0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
  0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
};

String xorEncrypt(String data) {
  // Utiliser une clé dérivée du mot de passe WiFi pour plus de sécurité
  uint8_t key[16];
  deriveEncryptionKey(key, sizeof(key));
  
  String result = "";
  for (size_t i = 0; i < data.length(); i++) {
    uint8_t encrypted = data[i] ^ key[i % 16];
    char hex[3];
    sprintf(hex, "%02X", encrypted);
    result += hex;
  }
  return result;
}

String xorDecrypt(String data) {
  // Utiliser la même clé dérivée pour le déchiffrement
  uint8_t key[16];
  deriveEncryptionKey(key, sizeof(key));
  
  String result = "";
  for (size_t i = 0; i < data.length(); i += 2) {
    String hexByte = data.substring(i, i + 2);
    uint8_t byte = strtol(hexByte.c_str(), NULL, 16);
    uint8_t decrypted = byte ^ key[(i / 2) % 16];
    result += (char)decrypted;
  }
  return result;
}
String encryptData(const char* plaintext, size_t plaintextLen) {
  // Utiliser des buffers statiques pour éviter les allocations dynamiques
  static uint8_t paddedPlaintext[256]; // Buffer statique réduit à 256 octets
  static uint8_t encrypted[256];       // Buffer statique réduit à 256 octets

  // Vérifier que les données ne dépassent pas la taille maximale
  if (plaintextLen > 200) { // Laisser de la marge pour le padding
    Serial.println("ERREUR: Données trop longues pour chiffrement");
    return "";
  }

  uint8_t key[32];
  deriveEncryptionKey(key, sizeof(key));

  // IV fixe pour simplicité
  uint8_t iv[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);

  int ret = mbedtls_aes_setkey_enc(&aes, key, 256);
  if (ret != 0) {
    Serial.println("ERREUR: Échec configuration clé AES");
    mbedtls_aes_free(&aes);
    return "";
  }

  // Chiffrement AES en mode CBC avec buffers statiques
  size_t paddedLen = ((plaintextLen + 15) / 16) * 16; // Padding PKCS7

  // Préparer les données avec padding
  memset(paddedPlaintext, 0, sizeof(paddedPlaintext)); // Nettoyer le buffer
  memcpy(paddedPlaintext, plaintext, plaintextLen);
  // Ajouter padding PKCS7
  uint8_t padding = 16 - (plaintextLen % 16);
  memset(paddedPlaintext + plaintextLen, padding, padding);

  memset(encrypted, 0, sizeof(encrypted)); // Nettoyer le buffer
  ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, paddedLen, iv, paddedPlaintext, encrypted);
  if (ret != 0) {
    Serial.println("ERREUR: Échec chiffrement AES");
    mbedtls_aes_free(&aes);
    return "";
  }

  // Encoder en base64 (utiliser un buffer statique aussi)
  String base64Result = base64::encode(encrypted, paddedLen);

  mbedtls_aes_free(&aes);

  Serial.println("✅ Données chiffrées (optimisé)");
  return base64Result;
}

// Decrypt data using AES-256 (version optimisée pour ESP32)
String decryptData(const char* ciphertext, size_t ciphertextLen) {
  // Utiliser des buffers statiques pour éviter les allocations dynamiques
  static uint8_t encrypted[256];  // Buffer pour les données décodées base64
  static uint8_t decrypted[256];  // Buffer pour les données déchiffrées

  uint8_t key[32];
  deriveEncryptionKey(key, sizeof(key));

  // IV fixe pour simplicité
  uint8_t iv[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};

  // Décoder depuis base64 (utiliser buffer statique)
  memset(encrypted, 0, sizeof(encrypted));
  size_t encryptedLen = base64Decode(ciphertext, ciphertextLen, encrypted, sizeof(encrypted));
  if (encryptedLen == 0 || encryptedLen % 16 != 0) {
    Serial.println("ERREUR: Échec décodage base64 ou longueur invalide");
    return "";
  }

  if (encryptedLen > 200) { // Vérifier la taille maximale
    Serial.println("ERREUR: Données déchiffrées trop longues");
    return "";
  }

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);

  int aes_ret = mbedtls_aes_setkey_dec(&aes, key, 256);
  if (aes_ret != 0) {
    Serial.println("ERREUR: Échec configuration clé AES déchiffrement");
    mbedtls_aes_free(&aes);
    return "";
  }

  memset(decrypted, 0, sizeof(decrypted));
  aes_ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, encryptedLen, iv, encrypted, decrypted);
  if (aes_ret != 0) {
    Serial.println("ERREUR: Échec déchiffrement AES");
    mbedtls_aes_free(&aes);
    return "";
  }

  // Supprimer padding PKCS7
  uint8_t padding = decrypted[encryptedLen - 1];
  if (padding > 16 || padding == 0) {
    Serial.println("ERREUR: Padding invalide");
    mbedtls_aes_free(&aes);
    return "";
  }

  // Vérifier que tout le padding est correct
  for (size_t i = encryptedLen - padding; i < encryptedLen; i++) {
    if (decrypted[i] != padding) {
      Serial.println("ERREUR: Padding corrompu");
      mbedtls_aes_free(&aes);
      return "";
    }
  }

  size_t actualLen = encryptedLen - padding;
  String result = String((char*)decrypted, actualLen);

  mbedtls_aes_free(&aes);

  Serial.println("✅ Données déchiffrées (optimisé)");
  return result;
}

// Validate Lightning address format
bool validateLightningAddress(String address) {
  if (address.length() == 0) return true; // Optionnel

  // Format: user@domain.com
  int atIndex = address.indexOf('@');
  if (atIndex <= 0 || atIndex >= address.length() - 1) {
    Serial.println("❌ Format Lightning Address invalide: pas de @");
    return false;
  }

  String user = address.substring(0, atIndex);
  String domain = address.substring(atIndex + 1);

  // Vérifier user (lettres, chiffres, points, tirets, underscores)
  for (char c : user) {
    if (!isalnum(c) && c != '.' && c != '-' && c != '_') {
      Serial.println("❌ Caractère invalide dans user: " + String(c));
      return false;
    }
  }

  // Vérifier domain (format email basique)
  if (domain.indexOf('.') == -1) {
    Serial.println("❌ Domain invalide: pas de point");
    return false;
  }

  Serial.println("✅ Lightning Address valide: " + address);
  return true;
}

// Validate API key format
bool validateApiKey(String key) {
  if (key.length() == 0 || key == "public") return true; // Optionnel

  // Format LN Markets: chaîne base64 d'au moins 20 caractères
  if (key.length() < 20) {
    Serial.println("❌ API Key trop courte (min 20 caractères)");
    return false;
  }

  // Vérifier que c'est une chaîne base64 valide (caractères alphanumériques + / + =)
  for (char c : key) {
    if (!isalnum(c) && c != '/' && c != '+' && c != '=') {
      Serial.println("❌ API Key contient des caractères invalides");
      return false;
    }
  }

  Serial.println("✅ API Key valide");
  return true;
}

// Validation séparée pour les passphrases (peuvent être plus courtes)
bool validatePassphrase(String passphrase) {
  if (passphrase.length() == 0 || passphrase == "public") return true; // Optionnel

  // Les passphrases peuvent être plus courtes que les clés API
  if (passphrase.length() < 8) {
    Serial.println("❌ Passphrase trop courte (min 8 caractères)");
    return false;
  }

  // Vérifier que c'est une chaîne alphanumérique (pas forcément base64)
  for (char c : passphrase) {
    if (!isalnum(c)) {
      Serial.println("❌ Passphrase contient des caractères invalides");
      return false;
    }
  }

  Serial.println("✅ Passphrase valide");
  return true;
}

// Fonction de décodage base64 simple pour ESP32
size_t base64Decode(const char* input, size_t inputLen, uint8_t* output, size_t outputMaxLen) {
  // Table de décodage base64
  static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  
  size_t outputLen = 0;
  int bits = 0;
  int bits_count = 0;
  
  for (size_t i = 0; i < inputLen && outputLen < outputMaxLen; ++i) {
    char c = input[i];
    if (c == '=') break; // Fin du padding
    
    const char* pos = strchr(base64_chars, c);
    if (!pos) continue; // Caractère invalide, ignorer
    
    int val = pos - base64_chars;
    bits = (bits << 6) | val;
    bits_count += 6;
    
    if (bits_count >= 8) {
      output[outputLen++] = (bits >> (bits_count - 8)) & 0xFF;
      bits_count -= 8;
    }
  }
  
  return outputLen;
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LN MARKETS TOUCH - PHASE 1 ===");
  
  // Init écran
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  tft.init();
  tft.setRotation(1); // Paysage
  tft.fillScreen(COLOR_BG);
  Serial.println("TFT init OK");
  
  // Init carte SD
  // if (initSDCard()) {
  //   Serial.println("Carte SD initialisée ✓");
  // } else {
  //   Serial.println("Carte SD non détectée");
  // }
  
  // Afficher image/GIF de démarrage (5 secondes)
  // showBootImage();
  
  // Message boot
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(2);
  tft.setCursor(50, 100);
  tft.println("BOOTING...");
  Serial.println("Affichage BOOTING...");
  delay(1000);
  
  // Init tactile avec SPI séparé
  Serial.println("Init SPI pour touch...");
  SPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touch.begin();
  touch.setRotation(1);
  Serial.println("Touch init OK");
  
  // Init LED RGB
  Serial.println("Init LED RGB...");
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  
  // Test LED au démarrage (cyan = rouge OFF + vert ON + bleu ON)
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);
  delay(500);
  
  // Éteindre LED
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_BLUE, LOW);
  
  Serial.println("LED RGB OK");
  Serial.println("Init SPIFFS...");
  if (!SPIFFS.begin(true)) {
    Serial.println("ERREUR: SPIFFS FAIL!");
    showError("SPIFFS FAIL!");
    while(1);
  }
  Serial.println("SPIFFS OK");
  
  // Créer le mutex TFT
  Serial.println("Creation mutex TFT...");
  tft_mutex = xSemaphoreCreateMutex();
  if (tft_mutex == NULL) {
    Serial.println("ERREUR: Mutex TFT NULL!");
  } else {
    Serial.println("Mutex TFT créé ✓");
  }
  
  // Charger config
  Serial.println("Chargement config...");
  loadConfig();
  
  // Vérifier si on doit démarrer en mode AP
  bool shouldStartAP = false;
  
  // Condition 1: Pas de SSID configuré
  if (ssid_saved == "") {
    Serial.println("❌ Pas de SSID configuré → Mode AP");
    shouldStartAP = true;
  }
  // Condition 2: SSID configuré mais pas de mot de passe
  else if (password_saved == "") {
    Serial.println("❌ SSID configuré mais pas de mot de passe → Mode AP");
    shouldStartAP = true;
  }
  // Condition 3: Tentative de connexion WiFi échoue (timeout)
  else {
    Serial.println("🔄 Tentative de connexion WiFi rapide...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid_saved.c_str(), password_saved.c_str());
    
    // Attendre max 5 secondes pour la connexion
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("✅ WiFi connecté rapidement - mode normal");
      // Continuer en mode normal
    } else {
      Serial.println("❌ Échec connexion WiFi rapide → Mode AP");
      shouldStartAP = true;
      WiFi.disconnect(); // Déconnecter pour éviter conflits
    }
  }
  
  if (shouldStartAP) {
    Serial.println("🚀 Démarrage mode AP");
    startConfigPortal();
  } else {
    Serial.println("🔗 Config trouvée → Connexion WiFi complète");
    connectWiFi();
  }
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  if (config_mode) {
    dnsServer.processNextRequest();
    server.handleClient();
    return;
  }
  
  // Check WiFi
  if (WiFi.status() != WL_CONNECTED) {
    showError("WiFi perdu");
    delay(3000);
    connectWiFi();
    return;
  }
  
  // Gérer les appuis tactiles
  handleTouch();
  
  // ✨ LED RGB - Indicateur P&L
  updateLedPnl();
  
  delay(100);
}

// =====================================================
// CONFIG SPIFFS
// =====================================================
void loadConfig() {
  Serial.println("🔍 [LOADCONFIG] Début chargement configuration...");

  // 1. Vérifier existence du fichier
  Serial.println("📁 [LOADCONFIG] Vérification existence /config.json...");
  bool fileExists = SPIFFS.exists("/config.json");
  Serial.print("📁 [LOADCONFIG] Fichier existe: ");
  Serial.println(fileExists ? "OUI" : "NON");

  if (!fileExists) {
    Serial.println("❌ [LOADCONFIG] Fichier config.json introuvable - mode AP activé");
    return;
  }

  // 2. Ouvrir le fichier
  Serial.println("📖 [LOADCONFIG] Ouverture du fichier /config.json...");
  fs::File file = SPIFFS.open("/config.json", "r");
  bool fileOpened = file;
  Serial.print("📖 [LOADCONFIG] Fichier ouvert: ");
  Serial.println(fileOpened ? "OUI" : "NON");

  if (!fileOpened) {
    Serial.println("❌ [LOADCONFIG] Impossible d'ouvrir config.json - mode AP activé");
    return;
  }

  // 3. Lire le contenu brut
  Serial.println("📄 [LOADCONFIG] Lecture contenu brut...");
  String rawContent = file.readString();
  Serial.print("📄 [LOADCONFIG] Contenu brut (");
  Serial.print(rawContent.length());
  Serial.println(" caractères):");
  Serial.println("--- DÉBUT CONTENU ---");
  Serial.println(rawContent);
  Serial.println("--- FIN CONTENU ---");

  // 4. Parser JSON
  Serial.println("🔧 [LOADCONFIG] Parsing JSON...");
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, rawContent);
  file.close();

  Serial.print("🔧 [LOADCONFIG] Erreur parsing: ");
  if (error) {
    Serial.println("OUI - " + String(error.c_str()));
    Serial.println("❌ [LOADCONFIG] Échec parsing JSON - mode AP activé");
    return;
  } else {
    Serial.println("NON - Parsing réussi");
  }

  // 5. Extraire les valeurs de base
  Serial.println("📋 [LOADCONFIG] Extraction valeurs de base...");
  ssid_saved = doc["ssid"].as<String>();
  password_saved = doc["password"].as<String>();
  Serial.print("📋 [LOADCONFIG] SSID: '");
  Serial.print(ssid_saved);
  Serial.println("'");
  Serial.print("📋 [LOADCONFIG] Password: '");
  Serial.print(password_saved.length() > 0 ? "[PRÉSENT]" : "[VIDE]");
  Serial.println("'");

  // 6. Charger et déchiffrer les clés API
  Serial.println("🔐 [LOADCONFIG] Traitement clés API...");
  String encrypted_api_key = doc["api_key"].as<String>();
  String encrypted_api_secret = doc["api_secret"].as<String>();
  String encrypted_api_passphrase = doc["api_passphrase"].as<String>();

  Serial.print("🔐 [LOADCONFIG] Clé API chiffrée: '");
  Serial.print(encrypted_api_key);
  Serial.println("'");
  Serial.print("🔐 [LOADCONFIG] Secret API chiffré: '");
  Serial.print(encrypted_api_secret.length() > 0 ? "[PRÉSENT]" : "[VIDE]");
  Serial.println("'");
  Serial.print("🔐 [LOADCONFIG] Passphrase chiffrée: '");
  Serial.print(encrypted_api_passphrase.length() > 0 ? "[PRÉSENT]" : "[VIDE]");
  Serial.println("'");

  // Traitement API Key
  if (encrypted_api_key.startsWith("XOR:")) {
    Serial.println("🔓 [LOADCONFIG] Déchiffrement XOR API key...");
    String encrypted_data = encrypted_api_key.substring(4);
    Serial.print("🔓 [LOADCONFIG] Données chiffrées: '");
    Serial.print(encrypted_data);
    Serial.println("'");
    String decrypted = xorDecrypt(encrypted_data);
    Serial.print("🔓 [LOADCONFIG] Données déchiffrées: '");
    Serial.print(decrypted);
    Serial.println("'");

    bool isValid = (decrypted != "" && validateApiKey(decrypted));
    Serial.print("🔓 [LOADCONFIG] Validation: ");
    Serial.println(isValid ? "RÉUSSIE" : "ÉCHEC");

    if (isValid) {
      api_key = decrypted;
      Serial.println("✅ [LOADCONFIG] Clé API XOR validée et assignée");
    } else {
      Serial.println("❌ [LOADCONFIG] Échec déchiffrement/validation XOR API key - mode public");
      api_key = "public";
    }
  } else if (encrypted_api_key.startsWith("ENC:")) {
    Serial.println("🔓 [LOADCONFIG] Déchiffrement AES legacy API key...");
    String encrypted_data = encrypted_api_key.substring(4);
    String decrypted = decryptData(encrypted_data.c_str(), encrypted_data.length());
    bool isValid = (decrypted != "" && validateApiKey(decrypted));
    Serial.print("🔓 [LOADCONFIG] Validation AES: ");
    Serial.println(isValid ? "RÉUSSIE" : "ÉCHEC");

    if (isValid) {
      api_key = decrypted;
      Serial.println("✅ [LOADCONFIG] Clé API AES legacy validée");
    } else {
      Serial.println("❌ [LOADCONFIG] Échec déchiffrement/validation AES legacy API key - mode public");
      api_key = "public";
    }
  } else {
    Serial.println("🔓 [LOADCONFIG] Clé API non chiffrée détectée");
    bool isValid = (encrypted_api_key != "" && encrypted_api_key != "public" && validateApiKey(encrypted_api_key));
    Serial.print("🔓 [LOADCONFIG] Validation clé brute: ");
    Serial.println(isValid ? "RÉUSSIE" : "ÉCHEC");

    if (isValid) {
      api_key = encrypted_api_key;
      Serial.println("✅ [LOADCONFIG] Clé API brute validée");
    } else {
      Serial.println("❌ [LOADCONFIG] Clé API brute invalide - mode public");
      api_key = "public";
    }
  }

  // Traitement API Secret
  if (encrypted_api_secret.startsWith("XOR:")) {
    Serial.println("🔓 [LOADCONFIG] Déchiffrement XOR API secret...");
    String encrypted_data = encrypted_api_secret.substring(4);
    String decrypted = xorDecrypt(encrypted_data);
    bool isValid = (decrypted != "" && validateApiKey(decrypted));
    Serial.print("🔓 [LOADCONFIG] Validation secret XOR: ");
    Serial.println(isValid ? "RÉUSSIE" : "ÉCHEC");

    if (isValid) {
      api_secret = decrypted;
      Serial.println("✅ [LOADCONFIG] Secret API XOR validé");
    } else {
      Serial.println("❌ [LOADCONFIG] Échec déchiffrement/validation XOR API secret - mode public");
      api_secret = "public";
    }
  } else if (encrypted_api_secret.startsWith("ENC:")) {
    Serial.println("🔓 [LOADCONFIG] Déchiffrement AES legacy API secret...");
    String encrypted_data = encrypted_api_secret.substring(4);
    String decrypted = decryptData(encrypted_data.c_str(), encrypted_data.length());
    bool isValid = (decrypted != "" && validateApiKey(decrypted));
    Serial.print("🔓 [LOADCONFIG] Validation secret AES: ");
    Serial.println(isValid ? "RÉUSSIE" : "ÉCHEC");

    if (isValid) {
      api_secret = decrypted;
      Serial.println("✅ [LOADCONFIG] Secret API AES legacy validé");
    } else {
      Serial.println("❌ [LOADCONFIG] Échec déchiffrement/validation AES legacy API secret - mode public");
      api_secret = "public";
    }
  } else {
    Serial.println("🔓 [LOADCONFIG] Secret API non chiffré détecté");
    bool isValid = (encrypted_api_secret != "" && encrypted_api_secret != "public" && validateApiKey(encrypted_api_secret));
    Serial.print("🔓 [LOADCONFIG] Validation secret brut: ");
    Serial.println(isValid ? "RÉUSSIE" : "ÉCHEC");

    if (isValid) {
      api_secret = encrypted_api_secret;
      Serial.println("✅ [LOADCONFIG] Secret API brut validé");
    } else {
      Serial.println("❌ [LOADCONFIG] Secret API brut invalide - mode public");
      api_secret = "public";
    }
  }

  // Traitement API Passphrase
  if (encrypted_api_passphrase.startsWith("XOR:")) {
    Serial.println("🔓 [LOADCONFIG] Déchiffrement XOR API passphrase...");
    String encrypted_data = encrypted_api_passphrase.substring(4);
    String decrypted = xorDecrypt(encrypted_data);
    bool isValid = (decrypted != "" && validatePassphrase(decrypted));
    Serial.print("🔓 [LOADCONFIG] Validation passphrase XOR: ");
    Serial.println(isValid ? "RÉUSSIE" : "ÉCHEC");

    if (isValid) {
      api_passphrase = decrypted;
      Serial.println("✅ [LOADCONFIG] Passphrase API XOR validée");
    } else {
      Serial.println("❌ [LOADCONFIG] Échec déchiffrement/validation XOR API passphrase - mode public");
      api_passphrase = "public";
    }
  } else if (encrypted_api_passphrase.startsWith("ENC:")) {
    Serial.println("🔓 [LOADCONFIG] Déchiffrement AES legacy API passphrase...");
    String encrypted_data = encrypted_api_passphrase.substring(4);
    String decrypted = decryptData(encrypted_data.c_str(), encrypted_data.length());
    bool isValid = (decrypted != "" && validatePassphrase(decrypted));
    Serial.print("🔓 [LOADCONFIG] Validation passphrase AES: ");
    Serial.println(isValid ? "RÉUSSIE" : "ÉCHEC");

    if (isValid) {
      api_passphrase = decrypted;
      Serial.println("✅ [LOADCONFIG] Passphrase API AES legacy validée");
    } else {
      Serial.println("❌ [LOADCONFIG] Échec déchiffrement/validation AES legacy API passphrase - mode public");
      api_passphrase = "public";
    }
  } else {
    Serial.println("🔓 [LOADCONFIG] Passphrase API non chiffrée détectée");
    bool isValid = (encrypted_api_passphrase != "" && encrypted_api_passphrase != "public" && validatePassphrase(encrypted_api_passphrase));
    Serial.print("🔓 [LOADCONFIG] Validation passphrase brute: ");
    Serial.println(isValid ? "RÉUSSIE" : "ÉCHEC");

    if (isValid) {
      api_passphrase = encrypted_api_passphrase;
      Serial.println("✅ [LOADCONFIG] Passphrase API brute validée");
    } else {
      Serial.println("❌ [LOADCONFIG] Passphrase API brute invalide - mode public");
      api_passphrase = "public";
    }
  }

  // 7. Adresses Lightning
  Serial.println("⚡ [LOADCONFIG] Extraction adresses Lightning...");
  lightning_address = doc["lightning_address"].as<String>();
  deposit_lnaddress = doc["deposit_lnaddress"].as<String>();
  Serial.print("⚡ [LOADCONFIG] Lightning Address: '");
  Serial.print(lightning_address);
  Serial.println("'");
  Serial.print("⚡ [LOADCONFIG] Deposit LN Address: '");
  Serial.print(deposit_lnaddress);
  Serial.println("'");

  // 8. Résumé final
  Serial.println("✅ [LOADCONFIG] Configuration chargée avec succès:");
  Serial.println("   📶 SSID: " + ssid_saved);
  Serial.print("   🔑 API Key: ");
  if (api_key != "" && api_key != "public") {
    Serial.println(api_key.substring(0, 10) + "...");
  } else {
    Serial.println("[MODE PUBLIC]");
  }
  Serial.print("   🔐 API Secret: ");
  Serial.println(api_secret != "public" ? "[CONFIGURÉ]" : "[MODE PUBLIC]");
  Serial.print("   🔒 API Passphrase: ");
  Serial.println(api_passphrase != "public" ? "[CONFIGURÉ]" : "[MODE PUBLIC]");
  if (lightning_address != "") {
    Serial.println("   ⚡ Lightning Address: " + lightning_address);
  }
  if (deposit_lnaddress != "") {
    Serial.println("   📥 Deposit LN Address: " + deposit_lnaddress);
  }
  Serial.println("✅ [LOADCONFIG] Chargement terminé - prêt pour connexion WiFi");
}

bool saveConfig() {
  Serial.println("💾 [SAVECONFIG] saveConfig() appelée - début sauvegarde");
  
  StaticJsonDocument<256> doc;
  Serial.println("💾 [SAVECONFIG] Création document JSON...");
  
  doc["ssid"] = ssid_saved;
  doc["password"] = password_saved;
  Serial.println("💾 [SAVECONFIG] WiFi ajouté: SSID='" + ssid_saved + "'");
  
  // Chiffrer les clés API avant sauvegarde
  if (api_key != "" && api_key != "public") {
    Serial.println("🔐 [SAVECONFIG] Chiffrement XOR API key...");
    String encrypted = xorEncrypt(api_key);
    if (encrypted != "") {
      doc["api_key"] = String("XOR:") + encrypted;
      Serial.println("✅ [SAVECONFIG] API key chiffrée et ajoutée");
    } else {
      Serial.println("❌ [SAVECONFIG] Échec chiffrement XOR API key");
      doc["api_key"] = "public";
    }
  } else {
    doc["api_key"] = api_key;
    Serial.println("💾 [SAVECONFIG] API key ajoutée (non chiffrée)");
  }
  
  if (api_secret != "" && api_secret != "public") {
    Serial.println("🔐 [SAVECONFIG] Chiffrement XOR API secret...");
    String encrypted = xorEncrypt(api_secret);
    if (encrypted != "") {
      doc["api_secret"] = String("XOR:") + encrypted;
      Serial.println("✅ [SAVECONFIG] API secret chiffré et ajouté");
    } else {
      Serial.println("❌ [SAVECONFIG] Échec chiffrement XOR API secret");
      doc["api_secret"] = "public";
    }
  } else {
    doc["api_secret"] = api_secret;
    Serial.println("💾 [SAVECONFIG] API secret ajouté (non chiffré)");
  }
  
  if (api_passphrase != "" && api_passphrase != "public") {
    Serial.println("🔐 [SAVECONFIG] Chiffrement XOR API passphrase...");
    String encrypted = xorEncrypt(api_passphrase);
    if (encrypted != "") {
      doc["api_passphrase"] = String("XOR:") + encrypted;
      Serial.println("✅ [SAVECONFIG] API passphrase chiffrée et ajoutée");
    } else {
      Serial.println("❌ [SAVECONFIG] Échec chiffrement XOR API passphrase");
      doc["api_passphrase"] = "public";
    }
  } else {
    doc["api_passphrase"] = api_passphrase;
    Serial.println("💾 [SAVECONFIG] API passphrase ajoutée (non chiffrée)");
  }
  
  doc["lightning_address"] = lightning_address;
  doc["deposit_lnaddress"] = deposit_lnaddress;
  Serial.println("💾 [SAVECONFIG] Adresses Lightning ajoutées");
  
  Serial.println("💾 [SAVECONFIG] Ouverture fichier /config.json en écriture...");
  fs::File file = SPIFFS.open("/config.json", "w");
  bool fileOpened = file;
  Serial.print("💾 [SAVECONFIG] Fichier ouvert: ");
  Serial.println(fileOpened ? "OUI" : "NON");
  
  if (!fileOpened) {
    Serial.println("❌ [SAVECONFIG] Erreur ouverture config.json en écriture");
    return false;
  }
  
  Serial.println("💾 [SAVECONFIG] Sérialisation JSON...");
  size_t bytesWritten = serializeJson(doc, file);
  Serial.print("💾 [SAVECONFIG] Bytes écrits: ");
  Serial.println(bytesWritten);
  
  if (bytesWritten == 0) {
    Serial.println("❌ [SAVECONFIG] Échec sérialisation JSON - aucun byte écrit");
    file.close();
    return false;
  }
  
  file.close();
  Serial.println("✅ [SAVECONFIG] Fichier fermé - sauvegarde terminée");
  return true;
}

// =====================================================
// PORTAIL AP
// =====================================================
void startConfigPortal() {
  config_mode = true;
  
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(2);
  tft.setCursor(40, 30);
  tft.println("MODE CONFIG");
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 80);
  tft.println("1. Connecte ton WiFi a:");
  
  tft.setTextColor(COLOR_RED);
  tft.setTextSize(2);
  tft.setCursor(40, 100);
  tft.println("LNMarkets-Touch");
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 140);
  tft.println("2. Ouvre ton navigateur:");
  
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(2);
  tft.setCursor(60, 160);
  tft.println("192.168.4.1");
  
  // Démarrer AP
  WiFi.softAP("LNMarkets-Touch");
  IPAddress IP = WiFi.softAPIP();
  
  Serial.println("AP démarré:");
  Serial.print("  IP: ");
  Serial.println(IP);
  
  // DNS captif (redirige tout vers l'ESP32)
  dnsServer.start(53, "*", WiFi.softAPIP());
  
  // Routes web
  server.on("/", handleRoot);
  server.on("/save", []() {
    Serial.println("📨 [SERVER] /save requested, method: " + String(server.method()));
    if (server.method() == HTTP_POST) {
      Serial.println("📨 [SERVER] Handling POST /save");
      handleSave();
    } else {
      Serial.println("📨 [SERVER] Redirecting non-POST /save to /");
      server.sendHeader("Location", "/", true);
      server.send(302, "text/plain", "");
    }
  });
  server.on("/favicon.ico", []() { server.send(204); }); // No content
  
  // Captive portal detection URLs - return 204 (No Content) to indicate captive portal
  server.on("/generate_204", []() { server.send(204); }); // Android captive portal
  server.on("/hotspot-detect.html", []() { server.send(204); }); // iOS captive portal
  server.on("/connecttest.txt", []() { server.send(200, "text/plain", "Microsoft Connect Test"); }); // Windows captive portal
  server.on("/connectivitycheck.gstatic.com/generate_204", []() { server.send(204); }); // Android full URL
  server.on("/captive.apple.com/hotspot-detect.html", []() { server.send(204); }); // iOS full URL
  server.on("/www.msftconnecttest.com/connecttest.txt", []() { server.send(200, "text/plain", "Microsoft Connect Test"); }); // Windows full URL
  
  // For all other requests, serve the portal page directly (not redirect)
  server.onNotFound([]() { 
    Serial.println("❓ [SERVER] Request not found: " + server.uri() + " method: " + String(server.method()));
    handleRoot(); 
  }); // Serve portal page for all unmatched requests
  
  server.begin();
  
  Serial.println("Serveur web démarré");
  Serial.println("✅ Portail captif actif");
}

void handleRoot() {
  Serial.println("🌐 [HANDLEROOT] handleRoot() appelée - envoi page config");
  
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>LN Markets Config</title>
  <style>
    body { font-family: Arial; background: #0a0a0a; color: #00ffff; padding: 20px; }
    .container { max-width: 400px; margin: 0 auto; background: #1a1a1a; padding: 30px; border-radius: 10px; border: 2px solid #ff0040; }
    h1 { color: #ff0040; text-align: center; margin-bottom: 30px; font-size: 24px; }
    label { display: block; margin: 20px 0 8px 0; color: #00ffff; font-weight: bold; }
    input { width: 100%; padding: 12px; background: #0a0a0a; border: 2px solid #00ffff; color: #fff; border-radius: 5px; }
    input:focus { outline: none; border-color: #ff0040; }
    button { width: 100%; padding: 15px; margin-top: 30px; background: #ff0040; border: none; color: #fff; font-size: 18px; font-weight: bold; border-radius: 5px; cursor: pointer; }
    button:hover { background: #cc0033; }
    .info { margin-top: 20px; padding: 10px; background: #0a0a0a; border-left: 3px solid #00ffff; font-size: 12px; }
  </style>
</head>
<body>
  <div class='container'>
    <h1>⚡ LN MARKETS TOUCH</h1>
    <form action='/save' method='POST'>
      <label>WiFi SSID</label>
      <input type='text' name='ssid' placeholder='Ton WiFi' required>
      <label>WiFi Password</label>
      <input type='password' name='password' placeholder='Mot de passe' required>
      <label>LN Markets API Key</label>
      <input type='text' name='api_key' placeholder='Clé API base64 (optionnel)'>
      <label>LN Markets API Secret</label>
      <input type='password' name='api_secret' placeholder='Secret (optionnel)'>
      <label>LN Markets Passphrase</label>
      <input type='password' name='api_passphrase' placeholder='Passphrase (optionnel)'>
      <label>Lightning Address</label>
      <input type='text' name='lightning_address' placeholder='silex@lnbits.com (optionnel)'>
      <label>Deposit LN Address</label>
      <input type='text' name='deposit_lnaddress' placeholder='silex@lnbits.com (optionnel)'>
      <button type='submit'>💾 SAUVEGARDER</button>
    </form>
    <div class='info'>
      ℹ️ API Keys sur lnmarkets.com → Settings → API<br>
      ⚡ Lightning Address: Format user@domain.com
    </div>
  </div>
</body>
</html>
  )rawliteral";
  
  server.send(200, "text/html", html);
}

void handleSave() {
  Serial.println("\n🔥🔥🔥 === HANDLESAVE APPELÉ === 🔥🔥🔥");
  Serial.println("Méthode: " + String(server.method() == HTTP_POST ? "POST" : "GET"));
  Serial.println("Nombre d'arguments: " + String(server.args()));
  
  Serial.println("🔥🔥🔥 HANDLESAVE APPELÉ 🔥🔥🔥");
  Serial.println("Méthode HTTP: " + String(server.method()));
  Serial.println("Nombre args: " + String(server.args()));
  
  Serial.println("💾 [HANDLESAVE] handleSave() appelée - début traitement sauvegarde");
  Serial.println("🔍 [HANDLESAVE] Vérification arguments serveur...");
  
  // Vérifier si des arguments sont reçus
  if (server.args() == 0) {
    Serial.println("❌ [HANDLESAVE] AUCUN argument reçu du formulaire!");
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset='utf-8'>
  <meta http-equiv='refresh' content='3;url=/'>
  <style>
    body {
      font-family: Arial;
      background: #0a0a0a;
      color: #ff0040;
      text-align: center;
      padding: 50px;
    }
    h1 { font-size: 48px; margin-bottom: 20px; }
    p { font-size: 18px; }
  </style>
</head>
<body>
  <h1>❌ ERREUR</h1>
  <p>Aucun argument reçu du formulaire</p>
  <p>Retour automatique dans 3s...</p>
</body>
</html>
  )rawliteral";
    
    server.send(400, "text/html", html);
    Serial.println("📤 [HANDLESAVE] Réponse d'erreur envoyée (pas d'args)");
    return;
  }
  
  Serial.print("✅ [HANDLESAVE] ");
  Serial.print(server.args());
  Serial.println(" arguments reçus");
  
  String raw_ssid = server.arg("ssid");
  String raw_password = server.arg("password");
  String raw_api_key = server.arg("api_key");
  String raw_api_secret = server.arg("api_secret");
  String raw_api_passphrase = server.arg("api_passphrase");
  String raw_lightning_address = server.arg("lightning_address");
  String raw_deposit_lnaddress = server.arg("deposit_lnaddress");
  
  Serial.println("📝 [HANDLESAVE] Données reçues:");
  Serial.println("  SSID: " + raw_ssid);
  Serial.println("  Password: " + String(raw_password.length() > 0 ? "[MASQUÉ]" : "[VIDE]"));
  Serial.println("  API Key: " + String(raw_api_key.length() > 0 ? "[PRÉSENT]" : "[VIDE]"));
  Serial.println("  API Secret: " + String(raw_api_secret.length() > 0 ? "[PRÉSENT]" : "[VIDE]"));
  Serial.println("  API Passphrase: " + String(raw_api_passphrase.length() > 0 ? "[PRÉSENT]" : "[VIDE]"));
  Serial.println("  Lightning Address: " + raw_lightning_address);
  Serial.println("  Deposit LN Address: " + raw_deposit_lnaddress);
  
  // Validation des entrées
  String validationError = "";
  
  // Valider les clés API si elles sont fournies (validation AVANT chiffrement)
  if (raw_api_key != "" && raw_api_key != "public") {
    if (!validateApiKey(raw_api_key)) {
      validationError = "Clé API invalide (format base64 attendu)";
    }
  }
  
  if (raw_api_secret != "" && raw_api_secret != "public") {
    if (!validateApiKey(raw_api_secret)) {
      validationError = "Secret API invalide (format base64 attendu)";
    }
  }
  
  if (raw_api_passphrase != "" && raw_api_passphrase != "public") {
    if (!validatePassphrase(raw_api_passphrase)) {
      validationError = "Passphrase invalide (8-32 caractères alphanumériques)";
    }
  }
  
  // Valider les adresses Lightning si elles sont fournies
  if (raw_lightning_address != "") {
    if (!validateLightningAddress(raw_lightning_address)) {
      validationError = "Adresse Lightning invalide (format: user@domain.com)";
    }
  }
  
  if (raw_deposit_lnaddress != "") {
    if (!validateLightningAddress(raw_deposit_lnaddress)) {
      validationError = "Adresse de dépôt invalide (format: user@domain.com)";
    }
  }
  
  // Si erreur de validation, retourner une page d'erreur
  if (validationError != "") {
    Serial.println("❌ [HANDLESAVE] Erreur de validation: " + validationError);
    
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset='utf-8'>
  <meta http-equiv='refresh' content='5;url=/'>
  <style>
    body {
      font-family: Arial;
      background: #0a0a0a;
      color: #ff0040;
      text-align: center;
      padding: 50px;
    }
    h1 { font-size: 48px; margin-bottom: 20px; }
    p { font-size: 18px; }
  </style>
</head>
<body>
  <h1>❌ ERREUR DE VALIDATION</h1>
  <p>)rawliteral" + validationError + R"rawliteral(</p>
  <p>Retour automatique dans 5s...</p>
</body>
</html>
  )rawliteral";
    
    server.send(400, "text/html", html);
    Serial.println("📤 [HANDLESAVE] Réponse d'erreur envoyée");
    return;
  }
  
  // Assigner les valeurs validées aux variables globales
  Serial.println("✅ [HANDLESAVE] Assignation des valeurs aux variables globales...");
  ssid_saved = raw_ssid;
  password_saved = raw_password;
  api_key = raw_api_key;
  api_secret = raw_api_secret;
  api_passphrase = raw_api_passphrase;
  lightning_address = raw_lightning_address;
  deposit_lnaddress = raw_deposit_lnaddress;
  
  // Si pas de clés API, utiliser mode public
  if (api_key == "") api_key = "public";
  if (api_secret == "") api_secret = "public";
  if (api_passphrase == "") api_passphrase = "public";
  
  Serial.println("\n📝 === DONNÉES REÇUES ===");
  Serial.println("  SSID: " + ssid_saved);
  Serial.println("  Password: " + String(password_saved.length() > 0 ? "***" : "VIDE"));
  Serial.println("  API Key: " + (api_key != "public" ? api_key.substring(0, 10) + "..." : "public"));

  Serial.println("\n💾 Appel saveConfig()...");
  saveConfig();
  Serial.println("✅ saveConfig() terminé");
  
  // Vérifier immédiatement que le fichier a été créé
  Serial.println("🔍 [HANDLESAVE] Vérification fichier sauvegardé...");
  if (SPIFFS.exists("/config.json")) {
    Serial.println("✅ [HANDLESAVE] Fichier /config.json existe");
    
    // Lire et afficher le contenu sauvegardé
    fs::File checkFile = SPIFFS.open("/config.json", "r");
    if (checkFile) {
      String savedContent = checkFile.readString();
      checkFile.close();
      Serial.println("📄 [HANDLESAVE] Contenu sauvegardé:");
      Serial.println("--- DÉBUT CONTENU SAUVEGARDÉ ---");
      Serial.println(savedContent);
      Serial.println("--- FIN CONTENU SAUVEGARDÉ ---");
    } else {
      Serial.println("❌ [HANDLESAVE] Impossible de lire le fichier sauvegardé");
    }
  } else {
    Serial.println("❌ [HANDLESAVE] Fichier /config.json n'existe pas après sauvegarde!");
  }
  
  Serial.println("📤 [HANDLESAVE] Envoi réponse HTML de succès...");
  
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset='utf-8'>
  <meta http-equiv='refresh' content='3;url=/'>
  <style>
    body {
      font-family: Arial;
      background: #0a0a0a;
      color: #00ff00;
      text-align: center;
      padding: 50px;
    }
    h1 { font-size: 48px; margin-bottom: 20px; }
    p { font-size: 18px; }
  </style>
</head>
<body>
  <h1>✓ SAUVEGARDÉ!</h1>
  <p>Redémarrage dans 3s...</p>
</body>
</html>
  )rawliteral";
  
  server.send(200, "text/html", html);
  Serial.println("📤 [HANDLESAVE] Réponse HTML envoyée");
  
  Serial.println("⏳ [HANDLESAVE] Attente 5 secondes pour SPIFFS...");
  delay(5000); // Attendre que SPIFFS termine l'écriture
  Serial.println("🔄 [HANDLESAVE] ESP.restart() appelé!");
  ESP.restart();
}

// =====================================================
// FORCE AP MODE (si connexion WiFi échoue)
// =====================================================
void forceAPMode() {
  Serial.println("🔄 FORÇAGE MODE AP...");
  
  // Déconnecter WiFi
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  delay(1000);
  
  // Démarrer AP
  startConfigPortal();
}
void connectWiFi() {
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(2);
  tft.setCursor(30, 100);
  tft.println("Connexion WiFi...");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid_saved.c_str(), password_saved.c_str());
  
  Serial.print("Connexion WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connecté!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    syncTime();
    testAPIConnection();
  } else {
    Serial.println("❌ WiFi échec - forçage mode AP");
    showError("WiFi ECHEC! Mode AP...");
    delay(3000);
    forceAPMode(); // Forcer le mode AP au lieu de startConfigPortal()
  }
}

// =====================================================
// TEST API
// =====================================================
void testAPIConnection() {
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(2);
  tft.setCursor(50, 100);
  tft.println("Test API...");
  
  // Pour tester la connexion, on récupère juste le ticker public (pas besoin d'auth)
  HTTPClient http;
  http.begin("https://api.lnmarkets.com/v3/futures/ticker");
  
  Serial.println("Test API LN Markets (ticker public)...");
  int httpCode = http.GET();
  String response = http.getString();
  
  Serial.print("Code HTTP: ");
  Serial.println(httpCode);
  Serial.println("Réponse: " + response.substring(0, 100) + "...");
  
  http.end();
  
  if (httpCode == 200) {
    api_connected = true;
    Serial.println("API OK ✓");
    
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(COLOR_GREEN);
    tft.setTextSize(3);
    tft.setCursor(60, 100);
    tft.println("CONNECTE!");
    
    delay(2000);
    
    // Créer la tâche de mise à jour des prix sur le core 1
    xTaskCreatePinnedToCore(
      priceUpdateTask,      // Fonction de la tâche
      "PriceUpdate",        // Nom de la tâche
      8192,                 // Taille de la pile (8KB) - remis à la taille originale pour éviter stack overflow
      NULL,                 // Paramètre
      1,                    // Priorité
      &price_update_task,   // Handle de la tâche
      1                     // Core 1 (le core 0 est pour loop())
    );
    
    showMainScreen();
  } else {
    api_connected = false;
    Serial.println("❌ API échec - forçage mode AP");
    showError("API ERREUR! Mode AP...");
    delay(5000);
    forceAPMode(); // Forcer le mode AP si l'API ne fonctionne pas
  }
}

// =====================================================
// ÉCRAN PRINCIPAL
// =====================================================
void showMainScreen() {
  tft.fillScreen(COLOR_BG);
  
  // Header avec navigation
  tft.fillRect(0, 0, 80, 40, COLOR_RED); // HOME rouge (actif)
  tft.fillRect(80, 0, 80, 40, 0x39E7); // WALLET gris
  tft.fillRect(160, 0, 80, 40, 0x39E7); // POSITIONS gris
  tft.fillRect(240, 0, 80, 40, 0x39E7); // HISTORY gris
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 12);
  tft.println("HOME");
  tft.setCursor(95, 12);
  tft.println("WALLET");
  tft.setCursor(170, 12);
  tft.println("POS");
  tft.setCursor(250, 12);
  tft.println("HIST");
  
  // Zone prix
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(1);
  tft.setCursor(20, 50);
  tft.println("BITCOIN PRICE");
  
  // Prix (aligné à gauche)
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(0); // Réduit à taille 0 pour "Loading..." (moitié de 1)
  tft.fillRect(10, 60, 180, 30, COLOR_BG); // Prix aligné à gauche
  tft.setCursor(10, 60);
  tft.println("Loading...");
  
  // Boutons SELL en haut à droite, BUY en dessous
  uint16_t sellColor, buyColor;
  
  if (selected_side == "sell") {
    sellColor = COLOR_RED;        // SELL sélectionné : couleur normale
    buyColor = 0x39E7;           // BUY obscurci : gris foncé
  } else if (selected_side == "buy") {
    sellColor = 0x39E7;          // SELL obscurci : gris foncé
    buyColor = COLOR_GREEN;      // BUY sélectionné : couleur normale
  } else {
    sellColor = COLOR_RED;       // Aucun sélectionné : couleurs normales
    buyColor = COLOR_GREEN;
  }
  
  tft.fillRoundRect(250, 55, 70, 30, 8, sellColor);  // SELL - haut droite
  tft.fillRoundRect(250, 90, 70, 30, 8, buyColor); // BUY - bas droite
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(268, 63);
  tft.println("SELL");
  tft.setCursor(268, 98);
  tft.println("BUY");
  
  // Boutons Leverage
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 130);
  tft.println("LEVERAGE:");
  
  // Boutons 25x, 50x, 75x, 100x
  int btnWidth = 50;
  int btnHeight = 20; // Réduit à 20px
  int startX = 20;
  int startY = 140; // Remonté
  
  // 25x
  uint16_t color25 = (selected_leverage == 25) ? TFT_YELLOW : 0x39E7;
  tft.fillRoundRect(startX, startY, btnWidth, btnHeight, 3, color25);
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(startX + 10, startY + 5);
  tft.println("25x");
  
  // 50x
  uint16_t color50 = (selected_leverage == 50) ? TFT_YELLOW : 0x39E7;
  tft.fillRoundRect(startX + 60, startY, btnWidth, btnHeight, 3, color50);
  tft.setCursor(startX + 70, startY + 5);
  tft.println("50x");
  
  // 75x
  uint16_t color75 = (selected_leverage == 75) ? TFT_YELLOW : 0x39E7;
  tft.fillRoundRect(startX + 120, startY, btnWidth, btnHeight, 3, color75);
  tft.setCursor(startX + 130, startY + 5);
  tft.println("75x");
  
  // 100x
  uint16_t color100 = (selected_leverage == 100) ? TFT_YELLOW : 0x39E7;
  tft.fillRoundRect(startX + 180, startY, btnWidth, btnHeight, 3, color100);
  tft.setCursor(startX + 185, startY + 5);
  tft.println("100x");
  
  // Boutons montants rapides (100, 500, 1000, 5000 sats)
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 165);
  tft.println("QUICK AMOUNTS:");
  
  // 100 sats
  tft.fillRoundRect(20, 175, 45, 15, 3, 0x39E7); // Réduit à 15px hauteur
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(25, 180);
  tft.println("100");
  
  // 500 sats
  tft.fillRoundRect(75, 175, 45, 15, 3, 0x39E7);
  tft.setCursor(80, 180);
  tft.println("500");
  
  // 1000 sats
  tft.fillRoundRect(130, 175, 45, 15, 3, 0x39E7);
  tft.setCursor(135, 180);
  tft.println("1000");
  
  // 5000 sats
  tft.fillRoundRect(185, 175, 45, 15, 3, 0x39E7);
  tft.setCursor(190, 180);
  tft.println("5000");

  // Margin control (remonté)
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 200);
  tft.print("MARGIN: ");
  tft.print(selected_margin);
  tft.println(" sats");
  
  // Boutons + et -
  tft.fillRoundRect(180, 195, 30, 20, 3, 0x39E7); // Réduit à 20px
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(190, 200);
  tft.println("-");
  
  tft.fillRoundRect(220, 195, 30, 20, 3, 0x39E7);
  tft.setCursor(230, 200);
  tft.println("+");
  
  // Bouton exécuter (remonté)
  tft.fillRoundRect(20, 220, 280, 20, 5, 0x39E7); // Remonté à Y=220
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(100, 225); // Ajusté pour Y=220
  tft.println("EXECUTER");
  
  Serial.println("Écran principal affiché");
}

// =====================================================
// UPDATE PRIX (avec cache intelligent)
// =====================================================
void updatePrice() {
  // Vérifier le cache prix
  if (isCacheValid(priceCache)) {
    Serial.println("💰 Prix BTC en cache");
    return;
  }
  
  // Vérifier que nous sommes toujours sur l'écran HOME
  if (current_screen != SCREEN_HOME) return;
  
  if (!api_connected) return;
  
  HTTPClient http;
  http.begin("https://api.lnmarkets.com/v3/futures/ticker");
  http.setTimeout(3000); // Timeout 3s pour éviter blocages
  
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    
    // Mettre à jour le cache
    updateCache(priceCache, payload);
    
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      float new_price = doc["index"].as<float>();
      
      if (new_price != btc_price) {
        btc_price = new_price;
        
        // Effacer complètement la zone du prix (allongée de 1cm pour éviter les carrés)
        tft.fillRect(8, 58, 172, 35, COLOR_BG);
        
        // Afficher nouvelle valeur (prix en USD direct)
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(4);
        tft.setCursor(10, 60);
        tft.print("$");
        tft.println((int)btc_price);
        
        Serial.print("Prix BTC: $");
        Serial.println(btc_price);
      }
    }
  } else {
    Serial.print("Erreur ticker: ");
    Serial.println(httpCode);
  }
  
  http.end();
}

// =====================================================
// RESET CONFIG
// =====================================================
void checkResetButton() {
  static unsigned long touch_start = 0;
  static bool was_touched = false;
  
  // Lire la position tactile
  if (touch.touched()) {
    TS_Point p = touch.getPoint();
    
    // Seuil de pression pour éviter faux positifs
    if (p.z > 60) {  // Seuil réduit x10
      // Zone reset : coin bas gauche (0-60, 200-240)
      int x = map(p.x, 200, 3700, 0, 320);
      int y = map(p.y, 200, 3700, 0, 240);
      
      // Ignorer valeurs aberrantes
      if (x < 0 || x > 320 || y < 0 || y > 240) {
        was_touched = false;
        return;
      }
      
      if (x < 60 && y > 200) {
        if (!was_touched) {
          touch_start = millis();
          was_touched = true;
          Serial.println("Touch RESET détecté");
          
          // Indicateur visuel
          tft.fillCircle(30, 220, 10, COLOR_RED);
        } else if (millis() - touch_start > 3000) {
          // Reset après 3s!
          Serial.println("RESET CONFIG!");
          SPIFFS.remove("/config.json");
          
          showError("RESET! Reboot...");
          delay(2000);
          ESP.restart();
        }
      } else {
        was_touched = false;
      }
    }
  } else {
    was_touched = false;
  }
}

// =====================================================
// ERREUR
// =====================================================
void showError(String msg) {
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_RED);
  tft.setTextSize(2);
  tft.setCursor(20, 100);
  tft.println(msg);
  Serial.println("ERREUR: " + msg);
}

// =====================================================
// AUTHENTIFICATION API LN MARKETS
// =====================================================

String generateSignature(String method, String path, String data, String timestamp) {
  // Format du payload : timestamp + method + path + data
  // Le path doit être le chemin COMPLET (incluant /v3)
  String methodLower = method;
  methodLower.toLowerCase();
  String payload = timestamp + methodLower + path + data;
  
  Serial.println("DEBUG Signature:");
  Serial.println("  Method: " + method);
  Serial.println("  Path: " + path);
  Serial.println("  Data: '" + data + "'");
  Serial.println("  Timestamp: " + timestamp);
  Serial.println("  Payload: " + payload);
  
  // Le secret API est-il déjà en base64 ou en clair ?
  // Testons sans décodage base64 d'abord
  Serial.println("  Secret length (raw): " + String(api_secret.length()));
  Serial.println("  Secret API (raw, 10): " + api_secret.substring(0, 10) + "...");
  
  // HMAC-SHA256 avec le secret brut (sans décodage)
  byte hmacResult[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char*)api_secret.c_str(), api_secret.length());
  mbedtls_md_hmac_update(&ctx, (const unsigned char*)payload.c_str(), payload.length());
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);
  
  // Encoder en base64
  String signature = base64::encode(hmacResult, 32);
  Serial.println("  Signature B64: " + signature);
  return signature;
}

String makeAuthenticatedRequest(String method, String path, String data) {
  HTTPClient http;
  String url = "https://api.lnmarkets.com/v3" + path;
  
  // Pour GET/DELETE, construire query string
  String queryString = "";
  if ((method == "GET" || method == "DELETE") && data != "") {
    queryString = data;
    url += "?" + queryString;
  }
  
  http.begin(url);
  http.setTimeout(3000); // ⏱️ Timeout réduit à 3 secondes pour accélérer
  
  // Si on a des clés API valides, ajouter l'authentification
  if (api_key != "" && api_key != "public" && api_secret != "" && api_secret != "public") {
    // Obtenir timestamp Unix en millisecondes
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long long timestamp_ms = (unsigned long long)(tv.tv_sec) * 1000ULL + (unsigned long long)(tv.tv_usec) / 1000ULL;
    String timestamp = String(timestamp_ms);
    
    // Pour GET/DELETE, data pour la signature = query string
    // Pour POST/PUT, data pour la signature = body JSON
    String dataForSignature = "";
    if (method == "GET" || method == "DELETE") {
      if (data != "") {
        dataForSignature = "?" + data;  // Ajouter ? pour la signature
      }
    } else {
      dataForSignature = data;
    }
    
    // Path complet pour la signature (avec /v3)
    String fullPath = "/v3" + path;
    String signature = generateSignature(method, fullPath, dataForSignature, timestamp);
    
    // ⚠️ CRITIQUE : N'ajouter Content-Type que si on a un body (POST/PUT)
    if (method == "POST" || method == "PUT") {
      http.addHeader("Content-Type", "application/json");
    }
    // ⚠️ NE PAS ajouter Content-Type pour GET/DELETE
    
    http.addHeader("LNM-ACCESS-KEY", api_key);
    http.addHeader("LNM-ACCESS-PASSPHRASE", api_passphrase);
    http.addHeader("LNM-ACCESS-TIMESTAMP", timestamp);
    http.addHeader("LNM-ACCESS-SIGNATURE", signature);
    
    Serial.println("Request authentifiée:");
    Serial.println("  URL: " + url);
    Serial.println("  API Key: " + api_key.substring(0, 10) + "...");
    Serial.println("  Passphrase: " + api_passphrase.substring(0, 5) + "...");
    Serial.println("  Timestamp: " + timestamp);
    Serial.println("  Signature: " + signature.substring(0, 20) + "...");
  }
  
  int httpCode;
  if (method == "GET") {
    httpCode = http.GET();
  } else if (method == "POST") {
    httpCode = http.POST(data);
  } else if (method == "PUT") {
    httpCode = http.PUT(data);
  } else if (method == "DELETE") {
    httpCode = http.sendRequest("DELETE");
  }
  
  String response = "";
  if (httpCode > 0) {
    response = http.getString();
    Serial.print("HTTP Code: ");
    Serial.println(httpCode);
    if (httpCode == 401) {
      Serial.println("ERREUR 401 - Réponse serveur: " + response);
    }
    if (httpCode >= 400) {
      Serial.println("ERREUR " + String(httpCode) + " - Réponse: " + response);
    }
  }
  
  http.end();
  return response;
}

// =====================================================
// SYNCHRONISATION TEMPS NTP
// =====================================================
void syncTime() {
  Serial.println("Synchronisation NTP...");
  
  // Configurer NTP (serveur Europe)
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  
  // Attendre la synchronisation (max 10s)
  int timeout = 0;
  while (time(nullptr) < 1000000000 && timeout < 20) {
    delay(500);
    Serial.print(".");
    timeout++;
  }
  Serial.println();
  
  if (time(nullptr) > 1000000000) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long long timestamp_ms = (unsigned long long)(tv.tv_sec) * 1000ULL;
    Serial.print("NTP OK - Timestamp: ");
    Serial.println(String(timestamp_ms));
  } else {
    Serial.println("NTP timeout - l'authentification peut échouer!");
  }
}

// =====================================================
// GESTION TACTILE UNIFIÉE
// =====================================================
void handleTouch() {
  if (!touch.touched()) return;
  
  TS_Point p = touch.getPoint();
  if (p.z < 60) return;  // Seuil de pression
  
  // Mapper coordonnées
  int x = map(p.x, 200, 3700, 0, 320);
  int y = map(p.y, 200, 3700, 0, 240);
  
  // Ignorer valeurs aberrantes
  if (x < 0 || x > 320 || y < 0 || y > 240) return;
  
  Serial.print("Touch X:");
  Serial.print(x);
  Serial.print(" Y:");
  Serial.println(y);
  
  // Navigation header (haut de l'écran, 0-40)
  if (y < 40) {
    if (x < 80) {
      // Zone HOME
      current_screen = SCREEN_HOME;
      showMainScreen();
    } else if (x < 160) {
      // Zone WALLET - Rafraîchir immédiatement les données
      current_screen = SCREEN_WALLET;
      Serial.println("🔄 WALLET: Rafraîchissement immédiat des données...");
      updateWalletData(); // 🔄 Requête immédiate au clic
      showWalletScreen();
    } else if (x < 240) {
      // Zone POSITIONS
      current_screen = SCREEN_POSITIONS;
      showPositionsScreen();
    } else {
      // Zone HISTORY - maintenant pour les dépôts
      getDepositHistory();
    }
    delay(200);
    return;
  }
  
  // Actions selon l'écran actuel
  if (current_screen == SCREEN_HOME) {
    // Bouton SELL (250, 55, 70x30) - haut droite
    if (x >= 250 && x <= 320 && y >= 55 && y <= 85) {
      selected_side = "sell";
      Serial.println("🔻 SELL sélectionné!");
      showMainScreen(); // Refresh to highlight
    }
    // Bouton BUY (250, 90, 70x30) - bas droite
    else if (x >= 250 && x <= 320 && y >= 90 && y <= 120) {
      selected_side = "buy";
      Serial.println("⚡ BUY sélectionné!");
      showMainScreen(); // Refresh to highlight
    }
    // Boutons Leverage (20, 140, 50x20 chacun)
    else if (y >= 140 && y <= 160) {
      if (x >= 20 && x <= 70) {
        selected_leverage = 25;
        Serial.println("Leverage: 25x");
        showMainScreen(); // Refresh display
      } else if (x >= 80 && x <= 130) {
        selected_leverage = 50;
        Serial.println("Leverage: 50x");
        showMainScreen(); // Refresh display
      } else if (x >= 140 && x <= 190) {
        selected_leverage = 75;
        Serial.println("Leverage: 75x");
        showMainScreen(); // Refresh display
      } else if (x >= 200 && x <= 250) {
        selected_leverage = 100;
        Serial.println("Leverage: 100x");
        showMainScreen(); // Refresh display
      }
    }
    // Boutons montants rapides (100, 500, 1000, 5000 sats) - Y=175-190
    else if (y >= 175 && y <= 190) {
      if (x >= 20 && x <= 65) {
        selected_margin = 100;
        Serial.println("Margin: 100 sats");
        showMainScreen();
      } else if (x >= 75 && x <= 120) {
        selected_margin = 500;
        Serial.println("Margin: 500 sats");
        showMainScreen();
      } else if (x >= 130 && x <= 175) {
        selected_margin = 1000;
        Serial.println("Margin: 1000 sats");
        showMainScreen();
      } else if (x >= 185 && x <= 230) {
        selected_margin = 5000;
        Serial.println("Margin: 5000 sats");
        showMainScreen();
      }
    }
    // Boutons Margin + (220, 195, 30x20) - ajustés
    else if (x >= 220 && x <= 250 && y >= 195 && y <= 215) {
      selected_margin += 100;
      if (selected_margin > 10000) selected_margin = 10000; // Max 10000 sats
      Serial.println("Margin: " + String(selected_margin));
      showMainScreen(); // Refresh display
    }
    // Boutons Margin - (180, 195, 30x20) - ajustés
    else if (x >= 180 && x <= 210 && y >= 195 && y <= 215) {
      selected_margin -= 100;
      if (selected_margin < 100) selected_margin = 100; // Min 100 sats
      Serial.println("Margin: " + String(selected_margin));
      showMainScreen(); // Refresh display
    }
    // Bouton EXECUTER (20, 220, 280x20) - remonté
    else if (x >= 20 && x <= 300 && y >= 220 && y <= 240) {
      if (selected_side != "") {
        Serial.println("🚀 EXECUTER TRADE!");
        executeTrade(selected_side, selected_margin);
      }
    }
  }
  else if (current_screen == SCREEN_WALLET) {
    // Boutons montants rapides (100, 500, 1000, 2000 sats) - Y=205-220
    if (y >= 205 && y <= 220) {
      if (x >= 20 && x <= 65) {
        withdraw_amount_sats = 100;
        Serial.println("Withdraw amount: 100 sats");
        showWalletScreen(); // Refresh display
      } else if (x >= 75 && x <= 120) {
        withdraw_amount_sats = 500;
        Serial.println("Withdraw amount: 500 sats");
        showWalletScreen(); // Refresh display
      } else if (x >= 130 && x <= 175) {
        withdraw_amount_sats = 1000;
        Serial.println("Withdraw amount: 1000 sats");
        showWalletScreen(); // Refresh display
      } else if (x >= 185 && x <= 230) {
        withdraw_amount_sats = 2000;
        Serial.println("Withdraw amount: 2000 sats");
        showWalletScreen(); // Refresh display
      }
    }
    // Boutons + et - pour retrait - Y=220-240
    else if (y >= 220 && y <= 240) {
      if (x >= 140 && x <= 170) {
        // Bouton - retrait
        withdraw_amount_sats -= 100;
        if (withdraw_amount_sats < 100) withdraw_amount_sats = 100; // Min 100 sats
        Serial.println("Withdraw amount: " + String(withdraw_amount_sats) + " sats");
        showWalletScreen(); // Refresh display
      } else if (x >= 180 && x <= 210) {
        // Bouton + retrait
        withdraw_amount_sats += 100;
        if (withdraw_amount_sats > 100000) withdraw_amount_sats = 100000; // Max 100k sats
        Serial.println("Withdraw amount: " + String(withdraw_amount_sats) + " sats");
        showWalletScreen(); // Refresh display
      }
    }
    // Boutons + et - pour dépôt - Y=240-260
    else if (y >= 240 && y <= 260) {
      if (x >= 120 && x <= 150) {
        // Bouton - dépôt
        deposit_amount_sats -= 100;
        if (deposit_amount_sats < 100) deposit_amount_sats = 100; // Min 100 sats
        Serial.println("Deposit amount: " + String(deposit_amount_sats) + " sats");
        showWalletScreen(); // Refresh display
      } else if (x >= 160 && x <= 190) {
        // Bouton + dépôt
        deposit_amount_sats += 100;
        if (deposit_amount_sats > 100000) deposit_amount_sats = 100000; // Max 100k sats
        Serial.println("Deposit amount: " + String(deposit_amount_sats) + " sats");
        showWalletScreen(); // Refresh display
      }
    }
    
    // Boutons sur le bord droit - vérifiés indépendamment
    // Bouton Withdraw (bord droit, en haut)
    if (x >= 250 && x <= 315 && y >= 50 && y <= 75) {
      Serial.println("💰 WITHDRAW pressed");
      withdrawBalance();
    }
    // Bouton Deposit (bord droit, en dessous)
    else if (x >= 250 && x <= 315 && y >= 85 && y <= 110) {
      Serial.println("📥 DEPOSIT pressed");
      showDepositManagementScreen();
    }
    // Bouton Deposit History (bord droit, en dessous)
    else if (x >= 250 && x <= 315 && y >= 120 && y <= 145) {
      Serial.println("📋 DEPOSIT HISTORY pressed");
      getDepositHistory();
    }
    // Bouton Reset Config (bord droit, en bas)
    else if (x >= 250 && x <= 315 && y >= 210 && y <= 235) {
      Serial.println("🔄 RESET CONFIG!");
      tft.fillScreen(COLOR_BG);
      tft.setTextColor(COLOR_RED);
      tft.setTextSize(2);
      tft.setCursor(40, 100);
      tft.println("RESET CONFIG...");
      
      // Reset toutes les variables aux valeurs par défaut
      ssid_saved = "";
      password_saved = "";
      api_key = "public";
      api_secret = "public";
      api_passphrase = "public";
      lightning_address = "";
      deposit_lnaddress = "";
      
      SPIFFS.remove("/config.json");
      delay(2000);
      ESP.restart();
    }
  }
  else if (current_screen == SCREEN_POSITIONS) {
    // Boutons pour positions et ordres
    int y_pos = 80;
    int item_index = 0;
    
    // Positions en cours d'abord
    // Synchroniser avec showPositionsScreen() : y_pos += 5 après "RUNNING POSITIONS:"
    y_pos += 5;  // Décalage pour correspondre à l'affichage
    
    for (int i = 0; i < running_trades_count && item_index < 5; i++) {
      if (x >= 20 && x <= 70 && y >= y_pos + 25 && y <= y_pos + 45) {
        Serial.println("🔴 CLOSE TRADE: " + running_trades_ids[i]);
        closeTrade(running_trades_ids[i]);
        return;
      }
      y_pos += 50;
      item_index++;
      if (y_pos > 200) break;
    }
    
    // Ordres ouverts ensuite
    if (open_orders_count > 0 && y_pos < 180) {
      y_pos += 15; // Skip the "OPEN ORDERS:" header
      for (int i = 0; i < open_orders_count && item_index < 10; i++) {
        if (x >= 20 && x <= 75 && y >= y_pos + 12 && y <= y_pos + 32) {
          Serial.println("🟠 CANCEL ORDER: " + open_orders_ids[i]);
          cancelOrder(open_orders_ids[i]);
          return;
        }
        y_pos += 35;
        item_index++;
        if (y_pos > 220) break;
      }
    }
  }
  
  delay(200); // Débounce
}

// =====================================================
// EXECUTER UN TRADE (avec invalidation cache)
// =====================================================
void executeTrade(String side, int margin) {
  if (api_key == "" || api_key == "public") {
    Serial.println("Pas de clés API - impossible de trader");
    return;
  }
  
  Serial.println("Exécution trade: " + side + " " + String(margin) + " sats");
  
  // Convertir side pour l'API (buy->buy, sell->sell)
  String api_side = side;
  
  // Construire le JSON
  String jsonData = "{";
  jsonData += "\"type\":\"market\",";
  jsonData += "\"side\":\"" + api_side + "\",";
  jsonData += "\"margin\":" + String(margin) + ",";
  jsonData += "\"leverage\":" + String(selected_leverage);
  jsonData += "}";
  
  Serial.println("JSON: " + jsonData);
  
  // Appeler l'API
  String response = makeAuthenticatedRequest("POST", "/futures/isolated/trade", jsonData);
  
  if (response != "") {
    Serial.println("Réponse trade: " + response);
    
    // Parser la réponse
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error) {
      if (doc["id"].is<String>()) {
        // Trade réussi - invalider le cache
        Serial.println("✅ Trade ouvert avec succès!");
        invalidateCache(); // 🔄 Invalider tout le cache après un trade
        
        tft.fillScreen(COLOR_BG);
        tft.setTextColor(TFT_GREEN);
        tft.setTextSize(3);
        tft.setCursor(40, 100);
        tft.println("TRADE");
        tft.setCursor(60, 130);
        tft.println("REUSSI!");
        delay(2000);
        current_screen = SCREEN_HOME;
        showMainScreen(); // Retour à l'écran home
      } else if (doc["message"].is<String>()) {
        // Erreur
        String errorMsg = doc["message"];
        Serial.println("❌ Erreur trade: " + errorMsg);
        tft.fillScreen(COLOR_BG);
        tft.setTextColor(TFT_RED);
        tft.setTextSize(2);
        tft.setCursor(20, 100);
        tft.println("ERREUR TRADE:");
        tft.setCursor(20, 130);
        tft.println(errorMsg.substring(0, 20));
        delay(3000);
        current_screen = SCREEN_HOME;
        showMainScreen(); // Retour à l'écran home
      }
    } else {
      Serial.println("Erreur parsing JSON réponse trade");
    }
  } else {
    Serial.println("Aucune réponse de l'API trade");
  }
}

// =====================================================
// ÉCRAN WALLET
// =====================================================
void showWalletScreen() {
  fadeOut(); // Transition fluide
  
  tft.fillScreen(COLOR_BG);
  
  // Fill black borders to cover any blue lines from TFT hardware
  tft.fillRect(0, 0, 320, 1, TFT_BLACK);     // top
  tft.fillRect(0, 239, 320, 1, TFT_BLACK);   // bottom
  tft.fillRect(0, 0, 1, 240, TFT_BLACK);     // left
  tft.fillRect(319, 0, 1, 240, TFT_BLACK);   // right
  
  // Header avec navigation
  tft.fillRect(0, 0, 80, 40, 0x39E7); // HOME gris
  tft.fillRect(80, 0, 80, 40, COLOR_RED); // WALLET rouge (actif)
  tft.fillRect(160, 0, 80, 40, 0x39E7); // POSITIONS gris
  tft.fillRect(240, 0, 80, 40, 0x39E7); // HISTORY gris
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 12);
  tft.println("HOME");
  tft.setCursor(95, 12);
  tft.println("WALLET");
  tft.setCursor(170, 12);
  tft.println("POS");
  tft.setCursor(250, 12);
  tft.println("HIST");
  
  // Titre
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(2);
  tft.setCursor(80, 50);
  tft.println("MY WALLET");
  
  // Balance disponible
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 75);
  tft.println("Balance disponible:");
  
  tft.setTextColor(COLOR_GREEN);
  tft.setTextSize(2);
  tft.setCursor(20, 90);
  tft.print(balance_sats);
  tft.setTextSize(1);
  tft.println(" sats");
  
  tft.setTextColor(COLOR_GREEN);
  tft.setTextSize(1);
  tft.setCursor(20, 105);
  tft.print("$");
  tft.println(balance_usd, 2);
  
  // Margin en positions (nouveau)
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 115);
  tft.println("Margin en positions:");
  
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(2);
  tft.setCursor(20, 130);
  tft.print(margin_in_positions);
  tft.setTextSize(1);
  tft.println(" sats");
  
  // Montant de retrait sélectionné (descendu)
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 150);
  tft.println("Montant retrait:");
  
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(2);
  tft.setCursor(20, 165);
  tft.print(withdraw_amount_sats);
  tft.setTextSize(1);
  tft.println(" sats");
  
  // Boutons montants rapides (100, 500, 1000, 2000 sats) - remontés
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 185);
  tft.println("MONTANTS RAPIDES:");
  
  // 100 sats
  tft.fillRoundRect(20, 195, 45, 15, 3, 0x39E7);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(25, 200);
  tft.println("100");
  
  // 500 sats
  tft.fillRoundRect(75, 195, 45, 15, 3, 0x39E7);
  tft.setCursor(80, 200);
  tft.println("500");
  
  // 1000 sats
  tft.fillRoundRect(130, 195, 45, 15, 3, 0x39E7);
  tft.setCursor(135, 200);
  tft.println("1000");
  
  // 2000 sats
  tft.fillRoundRect(185, 195, 45, 15, 3, 0x39E7);
  tft.setCursor(190, 200);
  tft.println("2000");
  
  // Contrôles + et - pour retrait (remontés)
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 215);
  tft.print("AJUSTER RETRAIT: ");
  
  // Bouton - retrait
  tft.fillRoundRect(140, 210, 30, 20, 3, 0x39E7);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(150, 215);
  tft.println("-");
  
  // Bouton + retrait
  tft.fillRoundRect(180, 210, 30, 20, 3, 0x39E7);
  tft.setCursor(190, 215);
  tft.println("+");
  
  // Bouton Reset Config - remonté
  tft.fillRoundRect(260, 210, 55, 20, 5, 0x4208); // Rouge foncé - déplacé complètement à droite
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(265, 215);
  tft.println("RESET");
  
  // Boutons sur le bord droit - petits et empilés
  // Bouton Withdraw (en haut à droite)
  tft.fillRoundRect(250, 50, 65, 25, 5, 0xFD20); // Orange
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(255, 58);
  tft.println("WITHDRAW");
  
  // Bouton Deposit (en dessous)
  tft.fillRoundRect(250, 85, 65, 25, 5, 0x07E0); // Vert
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(260, 93);
  tft.println("DEPOSIT");
  
  // Bouton Deposit History (en dessous)
  tft.fillRoundRect(250, 120, 65, 25, 5, 0x39E7); // Gris
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(255, 128);
  tft.println("HISTORY");
  
  Serial.println("Écran Wallet affiché");
}

// =====================================================
// ÉCRAN GESTION DÉPÔTS
// =====================================================
void showDepositManagementScreen() {
  Serial.println("📥 Affichage écran gestion dépôts");

  // Vérifier si une adresse de dépôt est configurée
  if (deposit_lnaddress == "") {
    Serial.println("❌ Aucune adresse de dépôt configurée");
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(COLOR_RED);
    tft.setTextSize(2);
    tft.setCursor(20, 80);
    tft.println("ERREUR:");
    tft.setCursor(20, 110);
    tft.println("Adresse de dépôt");
    tft.setCursor(20, 140);
    tft.println("non configurée!");
    tft.setCursor(20, 170);
    tft.println("Configurez-la dans");
    tft.setCursor(20, 200);
    tft.println("le portail AP.");
    delay(5000);
    showWalletScreen();
    return;
  }

  fadeOut(); // Transition fluide

  tft.fillScreen(TFT_BLACK); // Fond noir cyberpunk

  // Fill black borders to cover any blue lines from TFT hardware
  tft.fillRect(0, 0, 320, 1, TFT_BLACK);     // top
  tft.fillRect(0, 239, 320, 1, TFT_BLACK);   // bottom
  tft.fillRect(0, 0, 1, 240, TFT_BLACK);     // left
  tft.fillRect(319, 0, 1, 240, TFT_BLACK);   // right

  // Générer le QR code localement avec la bibliothèque QRCode
  Serial.println("🔄 Génération QR code local...");

  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(6)];
  qrcode_initText(&qrcode, qrcodeData, 6, ECC_LOW, deposit_lnaddress.c_str());

  // Calculer la taille et position du QR code (centré)
  int qrSize = qrcode.size * 4; // Scale factor de 4 pour une meilleure visibilité
  int qrX = (320 - qrSize) / 2; // Centré horizontalement
  int qrY = (240 - qrSize) / 2; // Centré verticalement

  // Fond noir cyberpunk pour le QR code
  tft.fillRect(qrX, qrY, qrSize, qrSize, TFT_BLACK);

  // Dessiner le QR code module par module
  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        // Module actif = blanc (visible sur fond noir)
        tft.fillRect(qrX + x * 4, qrY + y * 4, 4, 4, TFT_WHITE);
      } else {
        // Module inactif = noir (fond déjà noir)
        // tft.fillRect(qrX + x * 4, qrY + y * 4, 4, 4, TFT_BLACK);
      }
    }
  }

  Serial.println("✅ QR code de dépôt généré localement!");
  Serial.println("✅ Écran gestion dépôts affiché");

  // Attendre 2 minutes ou touch pour annuler
  unsigned long startTime = millis();
  while (millis() - startTime < 120000) { // 2 minutes = 120 secondes
    delay(100); // Petit délai pour éviter de bloquer complètement

    // Vérifier si l'utilisateur touche l'écran pour annuler
    uint16_t touchX, touchY;
    if (tft.getTouch(&touchX, &touchY)) {
      Serial.println("👆 Touch détecté - retour anticipé");
      break; // Sortir de la boucle d'attente
    }
  }

  // Timeout ou touch détecté
  Serial.println("⏰ Fin écran dépôt, retour auto");
  showWalletScreen();
}

// =====================================================
// ÉCRAN POSITIONS
// =====================================================
void showPositionsScreen() {
  fadeOut(); // Transition fluide
  
  tft.fillScreen(COLOR_BG);
  
  // Header avec navigation
  tft.fillRect(0, 0, 80, 40, 0x39E7); // HOME gris
  tft.fillRect(80, 0, 80, 40, 0x39E7); // WALLET gris
  tft.fillRect(160, 0, 80, 40, COLOR_RED); // POSITIONS rouge (actif)
  tft.fillRect(240, 0, 80, 40, 0x39E7); // HISTORY gris
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 12);
  tft.println("HOME");
  tft.setCursor(95, 12);
  tft.println("WALLET");
  tft.setCursor(170, 12);
  tft.println("POS");
  tft.setCursor(250, 12);
  tft.println("HIST");
  
  // Titre
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(2);
  tft.setCursor(60, 50);
  tft.println("POSITIONS & ORDERS");
  
  // Afficher les positions en cours
  int y_pos = 80;
  int total_items = 0;
  
  if (running_trades_count > 0) {
    tft.setTextColor(COLOR_CYAN);
    tft.setTextSize(1);
    tft.setCursor(20, y_pos - 15);
    tft.println("RUNNING POSITIONS:");
    y_pos += 5;
    
    for (int i = 0; i < 5 && running_trades_details[i] != ""; i++) {
      // Détails de la position
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(1);
      tft.setCursor(20, y_pos);
      tft.print(running_trades_details[i]);
      
      // Entry price
      tft.setCursor(20, y_pos + 12);
      tft.print("Entry: $");
      tft.print((int)running_trades_entry[i]);
      
      // Current price
      tft.setCursor(120, y_pos + 12);
      tft.print("Now: $");
      tft.print((int)btc_price);
      
      // P&L en satoshis et %
      long pnl_sats = running_trades_pnl_sats[i];
      float pnl_percent = running_trades_pnl[i];
      
      tft.setCursor(200, y_pos + 12);
      if (pnl_sats >= 0) {
        tft.setTextColor(COLOR_GREEN);
        tft.print("+");
      } else {
        tft.setTextColor(COLOR_RED);
        tft.print("");
      }
      tft.print(pnl_sats);
      tft.print("s ");
      
      if (pnl_percent >= 0) {
        tft.setTextColor(COLOR_GREEN);
        tft.print("+");
      } else {
        tft.setTextColor(COLOR_RED);
        tft.print("");
      }
      tft.print(pnl_percent, 1);
      tft.print("%");
      
      // Bouton CLOSE
      tft.fillRoundRect(20, y_pos + 25, 50, 20, 3, COLOR_RED);
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(1);
      tft.setCursor(25, y_pos + 30);
      tft.println("CLOSE");
      
      y_pos += 50;
      total_items++;
      if (y_pos > 200) break;
    }
  }
  
  // Afficher les ordres ouverts
  if (open_orders_count > 0 && y_pos < 180) {
    tft.setTextColor(COLOR_CYAN);
    tft.setTextSize(1);
    tft.setCursor(20, y_pos);
    tft.println("OPEN ORDERS:");
    y_pos += 15;
    
    for (int i = 0; i < 5 && open_orders_details[i] != ""; i++) {
      // Détails de l'ordre
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(1);
      tft.setCursor(20, y_pos);
      tft.print(open_orders_details[i]);
      
      // Bouton CANCEL
      tft.fillRoundRect(20, y_pos + 12, 55, 20, 3, 0xFD20); // Orange
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(1);
      tft.setCursor(25, y_pos + 17);
      tft.println("CANCEL");
      
      y_pos += 35;
      total_items++;
      if (y_pos > 220) break;
    }
  }
  
  // Message si rien
  if (total_items == 0) {
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(40, 100);
    tft.println("No positions or orders");
  }
  
  Serial.println("Écran Positions affiché");
}

// =====================================================
// ÉCRAN HISTORY
// =====================================================
void showHistoryScreen() {
  fadeOut(); // Transition fluide
  
  tft.fillScreen(COLOR_BG);
  
  // Header avec navigation
  tft.fillRect(0, 0, 80, 40, 0x39E7); // HOME gris
  tft.fillRect(80, 0, 80, 40, 0x39E7); // WALLET gris
  tft.fillRect(160, 0, 80, 40, 0x39E7); // POSITIONS gris
  tft.fillRect(240, 0, 80, 40, COLOR_RED); // HISTORY rouge (actif)
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 12);
  tft.println("HOME");
  tft.setCursor(95, 12);
  tft.println("WALLET");
  tft.setCursor(170, 12);
  tft.println("POS");
  tft.setCursor(250, 12);
  tft.println("HIST");
  
  // Titre
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(2);
  tft.setCursor(70, 50);
  tft.println("TRADE HISTORY");
  
  // Afficher les trades fermés
  if (closed_trades_count == 0) {
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(50, 100);
    tft.println("No closed trades");
  } else {
    int y_pos = 80;
    for (int i = 0; i < 10 && closed_trades_details[i] != ""; i++) {
      // Détails du trade
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(1);
      tft.setCursor(10, y_pos);
      tft.print(closed_trades_details[i]);
      
      // P&L
      float pnl = closed_trades_pnl[i];
      tft.setCursor(200, y_pos);
      if (pnl >= 0) {
        tft.setTextColor(COLOR_GREEN);
        tft.print("+");
      } else {
        tft.setTextColor(COLOR_RED);
        tft.print("");
      }
      tft.print(pnl, 2);
      tft.print(" sats");
      
      // Date
      tft.setTextColor(0x39E7);
      tft.setCursor(10, y_pos + 12);
      tft.print(closed_trades_dates[i]);
      
      y_pos += 30;
      if (y_pos > 220) break;
    }
  }
  
  Serial.println("Écran History affiché");
}

// =====================================================
// UPDATE WALLET DATA
// =====================================================
void updateWalletData() {
  // Vérifier le cache - éviter les appels trop fréquents
  if (millis() - last_wallet_update < WALLET_CACHE_DURATION) {
    Serial.println("💾 Données wallet en cache (moins de 10s)");
    return;
  }
  
  Serial.println("\n=== UPDATE WALLET DATA ===");
  
  if (api_key == "" || api_key == "public") {
    Serial.println("❌ Pas de clés API valides - mode public");
    return;
  }
  
  Serial.println("🔄 Récupération données wallet...");
  Serial.print("API Key: ");
  Serial.println(api_key.substring(0, 10) + "...");
  
  // Marquer le début de la mise à jour
  last_wallet_update = millis();
  
  // Get account balance
  Serial.println("📡 Appel API: GET /account");
  String response = makeAuthenticatedRequest("GET", "/account", "");
  
  if (response == "") {
    Serial.println("❌ Aucune réponse de l'API /account");
  } else {
    Serial.print("📨 Réponse /account: ");
    Serial.println(response.substring(0, 100) + "...");
    
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error) {
      if (doc.containsKey("balance")) {
        balance_sats = doc["balance"].as<long>();
        
        // Calculer balance USD
        if (btc_price > 0) {
          balance_usd = (balance_sats / 100000000.0) * btc_price;
        }
        
        Serial.print("✅ Balance trouvée: ");
        Serial.print(balance_sats);
        Serial.println(" sats");
      } else {
        Serial.println("❌ Clé 'balance' non trouvée dans la réponse");
        Serial.println("Clés disponibles: " + String(doc.size()));
      }
    } else {
      Serial.print("❌ Erreur parsing JSON wallet: ");
      Serial.println(error.c_str());
    }
  }
  
  // Get running trades
  Serial.println("📡 Appel API: GET /futures/isolated/trades/running");
  String tradesResponse = makeAuthenticatedRequest("GET", "/futures/isolated/trades/running", "");
  
  running_trades_count = 0;
  margin_in_positions = 0;
  
  // Clear previous details
  for (int i = 0; i < 5; i++) {
    running_trades_details[i] = "";
    running_trades_pnl[i] = 0.0;
    running_trades_pnl_sats[i] = 0;
    running_trades_ids[i] = "";
    running_trades_entry[i] = 0.0;
    running_trades_leverage[i] = 0;
    running_trades_margin[i] = 0;
  }
  
  if (tradesResponse == "") {
    Serial.println("❌ Aucune réponse de l'API trades/running");
  } else {
    Serial.print("📨 Réponse trades/running: ");
    Serial.println(tradesResponse.substring(0, 100) + "...");
    
    StaticJsonDocument<1024> tradesDoc;
    DeserializationError tradesError = deserializeJson(tradesDoc, tradesResponse);
    
    if (!tradesError && tradesDoc.is<JsonArray>()) {
      JsonArray trades = tradesDoc.as<JsonArray>();
      running_trades_count = trades.size();
      
      Serial.print("✅ ");
      Serial.print(running_trades_count);
      Serial.println(" trades trouvés");
      
      int detailIndex = 0;
      for (JsonVariant trade : trades) {
        long trade_margin = trade["margin"].as<long>();
        margin_in_positions += trade_margin;
        
        if (detailIndex < 5) {
          String id = trade["id"].as<String>();
          String side = trade["side"].as<String>();
          int leverage = trade["leverage"].as<int>();
          long pnl_sats = trade["pl"].as<long>();
          float entry_price = trade["entryPrice"].as<float>();
          
          // Calculate P&L % using pnl field
          float pnl_percent = 0.0;
          if (trade_margin > 0) {
            pnl_percent = (pnl_sats / (float)trade_margin) * 100.0;
          }
          
          // Also calculate using entry price as backup
          float pnl_calc = 0.0;
          if (btc_price > 0 && entry_price > 0) {
            if (side == "buy") {
              pnl_calc = ((btc_price - entry_price) / entry_price) * leverage * 100.0;
            } else if (side == "sell") {
              pnl_calc = ((entry_price - btc_price) / entry_price) * leverage * 100.0;
            }
          }
          
          // Use pnl field if available, else calculated
          if (abs(pnl_percent) > 0.01) {
            running_trades_pnl[detailIndex] = pnl_percent;
          } else {
            running_trades_pnl[detailIndex] = pnl_calc;
          }
          
          running_trades_pnl_sats[detailIndex] = pnl_sats;
          
          String detail = side + " " + String(leverage) + "x " + String(trade_margin) + "s";
          running_trades_details[detailIndex] = detail;
          running_trades_ids[detailIndex] = id;
          running_trades_entry[detailIndex] = entry_price;
          running_trades_leverage[detailIndex] = leverage;
          running_trades_margin[detailIndex] = trade_margin;
          detailIndex++;
        }
      }
      
      Serial.print("💰 Margin total en positions: ");
      Serial.print(margin_in_positions);
      Serial.println(" sats");
    } else {
      Serial.print("❌ Erreur parsing JSON trades: ");
      Serial.println(tradesError.c_str());
    }
  }
  
  // Get open orders
  Serial.println("Récupération ordres ouverts...");
  String openResponse = makeAuthenticatedRequest("GET", "/futures/isolated/trades/open", "");
  
  open_orders_count = 0;
  
  // Clear previous open orders
  for (int i = 0; i < 5; i++) {
    open_orders_details[i] = "";
    open_orders_ids[i] = "";
  }
  
  if (openResponse != "") {
    StaticJsonDocument<256> openDoc;
    DeserializationError openError = deserializeJson(openDoc, openResponse);
    
    if (!openError && openDoc.is<JsonArray>()) {
      JsonArray openTrades = openDoc.as<JsonArray>();
      open_orders_count = openTrades.size();
      
      int openIndex = 0;
      for (JsonVariant trade : openTrades) {
        if (openIndex < 5) {
          String id = trade["id"].as<String>();
          String side = trade["side"].as<String>();
          String type = trade["type"].as<String>();
          int leverage = trade["leverage"].as<int>();
          long margin = trade["margin"].as<long>();
          float price = trade["price"].as<float>();
          
          String detail = side + " " + type + " " + String(leverage) + "x " + String(margin) + "s @" + String((int)price);
          open_orders_details[openIndex] = detail;
          open_orders_ids[openIndex] = id;
          openIndex++;
        }
      }
      
      Serial.print("Ordres ouverts: ");
      Serial.println(open_orders_count);
    } else {
      Serial.println("Erreur parse JSON open orders");
    }
  }
  
  // Get closed trades
  Serial.println("Récupération trades fermés...");
  String closedResponse = makeAuthenticatedRequest("GET", "/futures/isolated/trades/closed", "limit=10");
  
  closed_trades_count = 0;
  
  // Clear previous closed trades
  for (int i = 0; i < 10; i++) {
    closed_trades_details[i] = "";
    closed_trades_pnl[i] = 0.0;
    closed_trades_dates[i] = "";
  }
  
  if (closedResponse != "") {
    StaticJsonDocument<512> closedDoc;
    DeserializationError closedError = deserializeJson(closedDoc, closedResponse);
    
    if (!closedError && closedDoc.is<JsonArray>()) {
      JsonArray closedTrades = closedDoc.as<JsonArray>();
      closed_trades_count = closedTrades.size();
      
      int closedIndex = 0;
      for (JsonVariant trade : closedTrades) {
        if (closedIndex < 10) {
          String id = trade["id"].as<String>();
          String side = trade["side"].as<String>();
          int leverage = trade["leverage"].as<int>();
          long pl = trade["pl"].as<long>();
          String closedAt = trade["closedAt"].as<String>();
          
          // Format date (take just the date part)
          if (closedAt.length() > 10) {
            closedAt = closedAt.substring(0, 10);
          }
          
          String detail = side + " " + String(leverage) + "x " + id.substring(0, 8) + "...";
          closed_trades_details[closedIndex] = detail;
          closed_trades_pnl[closedIndex] = pl / 1000.0; // Convert to sats (API returns msats)
          closed_trades_dates[closedIndex] = closedAt;
          closedIndex++;
        }
      }
      
      Serial.print("Trades fermés: ");
      Serial.println(closed_trades_count);
    } else {
      Serial.println("Erreur parse JSON closed trades");
    }
  }
  
  // Rafraîchir affichage
  if (current_screen == SCREEN_WALLET) {
    showWalletScreen();
  } else if (current_screen == SCREEN_POSITIONS) {
    showPositionsScreen();
  } else if (current_screen == SCREEN_HISTORY) {
    showHistoryScreen();
  }
}

// =====================================================
// ANNULER UN ORDRE
// =====================================================
void cancelOrder(String orderId) {
  if (api_key == "" || api_key == "public") {
    Serial.println("Pas de clés API - impossible d'annuler");
    return;
  }
  
  Serial.println("Annulation ordre: " + orderId);
  
  // Construire le JSON
  String jsonData = "{\"id\":\"" + orderId + "\"}";
  
  Serial.println("JSON cancel: " + jsonData);
  
  // Appeler l'API
  String response = makeAuthenticatedRequest("POST", "/futures/isolated/trade/cancel", jsonData);
  
  if (response != "") {
    Serial.println("Réponse cancel: " + response);
    
    // Parser la réponse
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error) {
      if (doc["id"].is<String>()) {
        // Cancel réussi
        Serial.println("✅ Ordre annulé avec succès!");
        tft.fillScreen(COLOR_BG);
        tft.setTextColor(0xFD20); // Orange
        tft.setTextSize(3);
        tft.setCursor(50, 100);
        tft.println("ORDER");
        tft.setCursor(40, 130);
        tft.println("CANCELED!");
        delay(2000);
        
        // Invalider cache positions et ordres
        positionsCache.valid = false; // 🔄 Invalider seulement le cache positions
        current_screen = SCREEN_POSITIONS;
        showPositionsScreen();
      } else if (doc["message"].is<String>()) {
        // Erreur
        String errorMsg = doc["message"];
        Serial.println("❌ Erreur cancel: " + errorMsg);
        tft.fillScreen(COLOR_BG);
        tft.setTextColor(TFT_RED);
        tft.setTextSize(2);
        tft.setCursor(20, 100);
        tft.println("ERREUR CANCEL:");
        tft.setCursor(20, 130);
        tft.println(errorMsg.substring(0, 20));
        delay(3000);
        current_screen = SCREEN_POSITIONS;
        showPositionsScreen();
      }
    } else {
      Serial.println("Erreur parsing JSON réponse cancel");
    }
  } else {
    Serial.println("Aucune réponse de l'API cancel");
  }
}

// =====================================================
// FERMER UN TRADE
// =====================================================
void closeTrade(String positionId) {
  if (api_key == "" || api_key == "public") {
    Serial.println("Pas de clés API - impossible de fermer");
    return;
  }
  
  Serial.println("🔴 Fermeture position: " + positionId);
  
  // API v3 LN Markets isolated: POST /v3/futures/isolated/trade/close
  String path = "/futures/isolated/trade/close";
  String jsonData = "{\"id\":\"" + positionId + "\"}";
  
  Serial.println("POST " + path);
  Serial.println("Body: " + jsonData);
  
  String response = makeAuthenticatedRequest("POST", path, jsonData);
  
  if (response != "") {
    Serial.println("Réponse close: " + response);
    
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error) {
      // Succès si la réponse contient un objet position
      if (doc["id"].is<String>() || doc["pl"].is<long>()) {
        Serial.println("✅ Position fermée avec succès!");
        
        tft.fillScreen(COLOR_BG);
        tft.setTextColor(TFT_GREEN);
        tft.setTextSize(3);
        tft.setCursor(40, 100);
        tft.println("POSITION");
        tft.setCursor(60, 130);
        tft.println("CLOSED!");
        
        delay(2000);
        
        // Rafraîchir et invalider cache
        invalidateCache(); // 🔄 Invalider tout le cache après fermeture
        updateWalletData();
        current_screen = SCREEN_POSITIONS;
        showPositionsScreen();
        
      } else if (doc["message"].is<String>()) {
        // Erreur API
        String errorMsg = doc["message"];
        Serial.println("❌ Erreur: " + errorMsg);
        
        tft.fillScreen(COLOR_BG);
        tft.setTextColor(TFT_RED);
        tft.setTextSize(2);
        tft.setCursor(20, 80);
        tft.println("ERREUR:");
        tft.setCursor(20, 110);
        tft.println(errorMsg.substring(0, 20));
        
        delay(3000);
        current_screen = SCREEN_POSITIONS;
        showPositionsScreen();
      } else {
        // Réponse inattendue mais peut-être succès
        Serial.println("Réponse inattendue");
        tft.fillScreen(COLOR_BG);
        tft.setTextColor(TFT_YELLOW);
        tft.setTextSize(2);
        tft.setCursor(60, 110);
        tft.println("DONE!");
        
        delay(1500);
        updateWalletData();
        current_screen = SCREEN_POSITIONS;
        showPositionsScreen();
      }
    } else {
      Serial.println("❌ Erreur parsing JSON");
      Serial.println("Raw: " + response);
      
      tft.fillScreen(COLOR_BG);
      tft.setTextColor(TFT_YELLOW);
      tft.setTextSize(2);
      tft.setCursor(40, 100);
      tft.println("REQUEST SENT");
      
      delay(2000);
      updateWalletData();
      current_screen = SCREEN_POSITIONS;
      showPositionsScreen();
    }
  } else {
    Serial.println("❌ Pas de réponse API");
    
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(20, 100);
    tft.println("NO RESPONSE");
    
    delay(3000);
    current_screen = SCREEN_POSITIONS;
    showPositionsScreen();
  }
}

// =====================================================
// GESTION CARTE SD ET IMAGES DE DEMARRAGE
// =====================================================






// =====================================================
// LED RGB - INDICATEUR P&L AVEC 5 VITESSES ou CONFIG
// =====================================================
void updateLedPnl() {
  // Priorité au mode configuration
  if (config_mode) {
    // LED pulsant en mode "heartbeat" (cœur qui bat) - effet plus visible
    static unsigned long last_beat = 0;
    static int beat_phase = 0; // 0: repos, 1: battement rapide, 2: pause
    
    unsigned long now = millis();
    
    switch (beat_phase) {
      case 0: // Repos (LED éteinte)
        if (now - last_beat > 1500) { // 1.5s de repos
          last_beat = now;
          beat_phase = 1;
          setLedRGB(1, 0, 1); // Magenta allumé
        }
        break;
        
      case 1: // Battement rapide (double pulsation)
        if (now - last_beat > 150) { // 150ms allumé
          last_beat = now;
          beat_phase = 2;
          setLedRGB(0, 0, 0); // Éteint
        }
        break;
        
      case 2: // Pause courte
        if (now - last_beat > 100) { // 100ms éteint
          last_beat = now;
          beat_phase = 3;
          setLedRGB(1, 0, 1); // Magenta allumé (2ème pulsation)
        }
        break;
        
      case 3: // 2ème battement
        if (now - last_beat > 150) { // 150ms allumé
          last_beat = now;
          beat_phase = 0; // Retour au repos
          setLedRGB(0, 0, 0); // Éteint
        }
        break;
    }
    return; // Ne pas exécuter le code P&L en mode config
  }
  
  // Mode normal - Indicateur P&L
  // Pas de positions = LED éteinte
  if (running_trades_count == 0) {
    setLedRGB(0, 0, 0);
    return;
  }
  
  // Calculer P&L total en %
  float total_margin = 0;
  float total_pnl_sats = 0;
  
  for (int i = 0; i < running_trades_count; i++) {
    total_margin += running_trades_margin[i];
    total_pnl_sats += running_trades_pnl_sats[i];
  }
  
  float pnl_percent = (total_margin > 0) ? (total_pnl_sats / total_margin) * 100.0 : 0;
  float pnl_abs = abs(pnl_percent); // Valeur absolue pour les 2 sens
  
  // ====== 5 PALIERS DE VITESSE ======
  int current_blink_speed = 0;
  bool continuous_mode = false;
  
  if (pnl_abs < 5) {
    // 🐌 NIVEAU 0 : < 5% = Très lent (presque pas de mouvement)
    current_blink_speed = 2000; // 2 secondes
  } 
  else if (pnl_abs < 25) {
    // 🚶 NIVEAU 1 : 5-25% = Vitesse de base
    current_blink_speed = 1000; // 1 seconde
  } 
  else if (pnl_abs < 50) {
    // 🏃 NIVEAU 2 : 25-50% = x2 plus rapide
    current_blink_speed = 500; // 0.5 seconde
  } 
  else if (pnl_abs < 75) {
    // 🚀 NIVEAU 3 : 50-75% = x4 plus rapide
    current_blink_speed = 250; // 0.25 seconde
  } 
  else if (pnl_abs < 100) {
    // ⚡ NIVEAU 4 : 75-100% = x8 plus rapide (super rapide)
    current_blink_speed = 125; // 0.125 seconde
  } 
  else {
    // 🔥 NIVEAU 5 : >100% = MODE CONTINU (allumé en permanence)
    continuous_mode = true;
  }
  
  // ====== MODE CONTINU (>100%) ======
  if (continuous_mode) {
    // LED allumée en permanence
    if (pnl_percent >= 0) {
      setLedRGB(0, 1, 0); // VERT fixe 🟢
    } else {
      setLedRGB(1, 0, 0); // ROUGE fixe 🔴
    }
    
    // Log une seule fois
    static bool continuous_logged = false;
    if (!continuous_logged) {
      Serial.println("🔥 MODE CONTINU ! P&L: " + String(pnl_percent, 1) + "%");
      continuous_logged = true;
    }
    return;
  }
  
  // ====== MODE CLIGNOTANT ======
  if (millis() - last_led_blink > current_blink_speed) {
    led_blink_state = !led_blink_state;
    last_led_blink = millis();
    
    // Log changement de niveau
    static int last_level = -1;
    int current_level = (pnl_abs < 5) ? 0 : 
                       (pnl_abs < 25) ? 1 : 
                       (pnl_abs < 50) ? 2 : 
                       (pnl_abs < 75) ? 3 : 4;
    
    if (current_level != last_level) {
      String level_name[] = {"🐌 TRÈS LENT", "🚶 NORMAL", "🏃 RAPIDE", "🚀 TRÈS RAPIDE", "⚡ ULTRA RAPIDE"};
      Serial.println("LED Niveau " + String(current_level) + ": " + level_name[current_level] + " | P&L: " + String(pnl_percent, 1) + "%");
      last_level = current_level;
    }
    
    if (led_blink_state) {
      // LED allumée
      if (pnl_percent >= 0) {
        setLedRGB(0, 1, 0); // VERT 🟢
      } else {
        setLedRGB(1, 0, 0); // ROUGE 🔴
      }
    } else {
      // LED éteinte
      setLedRGB(0, 0, 0); // OFF
    }
  }
}

void setLedRGB(int red, int green, int blue) {
  // La LED RGB du Sunton est active à l'état LOW (logique inversée)
  digitalWrite(LED_RED, red ? LOW : HIGH);
  digitalWrite(LED_GREEN, green ? LOW : HIGH);
  digitalWrite(LED_BLUE, blue ? LOW : HIGH);
}

// =====================================================
// FONCTIONS LNURL POUR PAIEMENTS LIGHTNING
// =====================================================

// Résoudre une adresse Lightning (user@domain.com) en URL LNURL-pay
String resolveLightningAddress(String address) {
  Serial.println("🔍 Résolution adresse Lightning: " + address);
  
  // Séparer user et domain
  int atIndex = address.indexOf('@');
  if (atIndex == -1) {
    Serial.println("❌ Adresse Lightning invalide (pas de @)");
    return "";
  }
  
  String user = address.substring(0, atIndex);
  String domain = address.substring(atIndex + 1);
  
  Serial.println("  User: " + user);
  Serial.println("  Domain: " + domain);
  
  // Construire l'URL LNURL
  String lnurl = "https://" + domain + "/.well-known/lnurlp/" + user;
  Serial.println("  LNURL: " + lnurl);
  
  return lnurl;
}

// Obtenir l'URL de paiement depuis l'URL LNURL
String getPayUrl(String lnurl) {
  Serial.println("📡 Requête LNURL: " + lnurl);
  
  HTTPClient http;
  http.begin(lnurl);
  http.setTimeout(5000); // 5 secondes timeout
  
  int httpCode = http.GET();
  String response = http.getString();
  
  Serial.print("📨 Code HTTP LNURL: ");
  Serial.println(httpCode);
  Serial.println("📨 Réponse LNURL: " + response.substring(0, 200) + "...");
  
  http.end();
  
  if (httpCode != 200) {
    Serial.println("❌ Erreur HTTP LNURL: " + String(httpCode));
    return "";
  }
  
  // Parser la réponse JSON
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, response);
  
  if (error) {
    Serial.println("❌ Erreur parsing JSON LNURL");
    return "";
  }
  
  // Vérifier que c'est un endpoint payRequest
  if (!doc.containsKey("callback")) {
    Serial.println("❌ Pas d'URL callback dans la réponse LNURL");
    return "";
  }
  
  String callback = doc["callback"].as<String>();
  Serial.println("✅ URL callback trouvée: " + callback);
  
  return callback;
}

// Générer une invoice Lightning depuis l'URL de paiement
String generateInvoice(String payUrl, long amount_sats) {
  Serial.println("💰 Génération invoice: " + String(amount_sats) + " sats");
  Serial.println("📡 URL paiement: " + payUrl);
  
  // Convertir sats en millisats (LNURL utilise des millisats)
  long amount_msats = amount_sats * 1000;
  
  // Ajouter le paramètre amount à l'URL
  String url = payUrl + "?amount=" + String(amount_msats);
  Serial.println("📡 URL complète: " + url);
  
  HTTPClient http;
  http.begin(url);
  http.setTimeout(5000); // 5 secondes timeout
  
  int httpCode = http.GET();
  String response = http.getString();
  
  Serial.print("📨 Code HTTP invoice: ");
  Serial.println(httpCode);
  Serial.println("📨 Réponse invoice: " + response.substring(0, 200) + "...");
  
  http.end();
  
  if (httpCode != 200) {
    Serial.println("❌ Erreur HTTP invoice: " + String(httpCode));
    return "";
  }
  
  // Parser la réponse JSON
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, response);
  
  if (error) {
    Serial.println("❌ Erreur parsing JSON invoice");
    return "";
  }
  
  // Extraire l'invoice BOLT11
  if (!doc.containsKey("pr")) {
    Serial.println("❌ Pas d'invoice (pr) dans la réponse");
    return "";
  }
  
  String invoice = doc["pr"].as<String>();
  Serial.println("✅ Invoice générée: " + invoice.substring(0, 50) + "...");
  
  return invoice;
}

// =====================================================
// WITHDRAW BALANCE
// =====================================================
void withdrawBalance() {
  if (api_key == "" || api_key == "public") {
    Serial.println("Pas de clés API - impossible de retirer");
    return;
  }
  
  // Vérifier qu'une adresse Lightning est configurée
  if (lightning_address == "") {
    Serial.println("❌ Aucune adresse Lightning configurée");
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(20, 80);
    tft.println("ERREUR:");
    tft.setCursor(20, 110);
    tft.println("Adresse Lightning");
    tft.setCursor(20, 140);
    tft.println("non configurée!");
    tft.setCursor(20, 170);
    tft.println("Configurez-la dans");
    tft.setCursor(20, 200);
    tft.println("le portail AP.");
    delay(5000);
    showWalletScreen();
    return;
  }
  
  Serial.println("💰 Retrait de " + String(withdraw_amount_sats) + " sats");
  Serial.println("📧 Adresse Lightning: " + lightning_address);
  
  // Étape 1: Résoudre l'adresse Lightning en URL LNURL
  String lnurl = resolveLightningAddress(lightning_address);
  if (lnurl == "") {
    Serial.println("❌ Impossible de résoudre l'adresse Lightning");
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(20, 80);
    tft.println("ERREUR:");
    tft.setCursor(20, 110);
    tft.println("Adresse Lightning");
    tft.setCursor(20, 140);
    tft.println("invalide!");
    delay(3000);
    showWalletScreen();
    return;
  }
  
  // Étape 2: Obtenir l'URL de paiement depuis LNURL
  String payUrl = getPayUrl(lnurl);
  if (payUrl == "") {
    Serial.println("❌ Impossible d'obtenir l'URL de paiement");
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(20, 80);
    tft.println("ERREUR:");
    tft.setCursor(20, 110);
    tft.println("Service LNURL");
    tft.setCursor(20, 140);
    tft.println("indisponible!");
    delay(3000);
    showWalletScreen();
    return;
  }
  
  // Étape 3: Générer l'invoice avec le montant sélectionné
  String invoice = generateInvoice(payUrl, withdraw_amount_sats);
  if (invoice == "") {
    Serial.println("❌ Impossible de générer l'invoice");
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(20, 80);
    tft.println("ERREUR:");
    tft.setCursor(20, 110);
    tft.println("Génération invoice");
    tft.setCursor(20, 140);
    tft.println("échouée!");
    delay(3000);
    showWalletScreen();
    return;
  }
  
  Serial.println("✅ Invoice générée avec succès");
  
  // Construire le JSON pour l'API LN Markets
  String jsonData = "{";
  jsonData += "\"invoice\":\"" + invoice + "\"";
  jsonData += "}";
  
  Serial.println("📡 Envoi à LN Markets API...");
  
  // Appeler l'API - endpoint /account/withdraw/lightning
  String response = makeAuthenticatedRequest("POST", "/account/withdraw/lightning", jsonData);
  
  if (response != "") {
    Serial.println("Réponse withdraw: " + response);
    
    // Parser la réponse
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error) {
      if (doc["id"].is<String>()) {
        // Retrait réussi
        Serial.println("✅ Retrait initié avec succès!");
        tft.fillScreen(COLOR_BG);
        tft.setTextColor(COLOR_GREEN);
        tft.setTextSize(3);
        tft.setCursor(40, 100);
        tft.println("WITHDRAW");
        tft.setCursor(60, 130);
        tft.println("DONE!");
        tft.setTextSize(1);
        tft.setCursor(40, 160);
        tft.print(String(withdraw_amount_sats) + " sats");
        delay(3000);
        
        // Invalider cache wallet après retrait
        walletCache.valid = false; // 🔄 Invalider cache wallet
        updateWalletData(); // Rafraîchir le solde
        showWalletScreen();
      } else if (doc["message"].is<String>()) {
        // Erreur
        String errorMsg = doc["message"];
        Serial.println("❌ Erreur retrait: " + errorMsg);
        tft.fillScreen(COLOR_BG);
        tft.setTextColor(TFT_RED);
        tft.setTextSize(2);
        tft.setCursor(20, 100);
        tft.println("WITHDRAW ERROR:");
        tft.setCursor(20, 130);
        tft.println(errorMsg.substring(0, 20));
        delay(4000);
        showWalletScreen();
      }
    } else {
      Serial.println("Erreur parsing JSON réponse withdraw");
    }
  } else {
    Serial.println("Aucune réponse de l'API withdraw");
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(20, 100);
    tft.println("NO API RESPONSE");
    delay(3000);
    showWalletScreen();
  }
}

// =====================================================
// GET DEPOSIT ADDRESS
// =====================================================
void getDepositAddress() {
  fadeOut(); // Transition fluide
  
  Serial.println("📥 Fonction dépôt appelée");
  
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(3);
  tft.setCursor(80, 30);
  tft.println("DEPOSITS");
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 70);
  tft.println("Lightning deposits must");
  tft.setCursor(35, 95);
  tft.println("be made through the");
  tft.setCursor(10, 120);
  tft.println("LN Markets web interface:");
  
  tft.setTextColor(COLOR_GREEN);
  tft.setTextSize(2);
  tft.setCursor(90, 150);
  tft.println("lnmarkets.com");
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(50, 180);
  tft.println("Use your account to");
  tft.setCursor(70, 205);
  tft.println("make deposits.");
  
  Serial.println("✅ Écran dépôt affiché avec instructions");
  
  // Attendre 5 secondes puis retour auto
  delay(5000);
  showWalletScreen();
}

// =====================================================
// GET DEPOSIT HISTORY
// =====================================================
void getDepositHistory() {
  if (api_key == "" || api_key == "public") {
    Serial.println("Pas de clés API - impossible de récupérer l'historique des dépôts");
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(20, 100);
    tft.println("ERREUR:");
    tft.setCursor(20, 130);
    tft.println("Clés API requises");
    tft.setCursor(20, 160);
    tft.println("pour historique");
    delay(3000);
    showWalletScreen();
    return;
  }
  
  Serial.println("📥 Récupération historique dépôts Lightning...");
  
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(2);
  tft.setCursor(30, 100);
  tft.println("GETTING DEPOSIT");
  tft.setCursor(50, 130);
  tft.println("HISTORY...");
  
  // Clear previous deposits
  lightning_deposits_count = 0;
  for (int i = 0; i < 10; i++) {
    lightning_deposits_details[i] = "";
    lightning_deposits_amounts[i] = 0;
    lightning_deposits_dates[i] = "";
    lightning_deposits_hashes[i] = "";
    lightning_deposits_comments[i] = "";
  }
  
  // Appeler l'API LN Markets pour obtenir l'historique des dépôts Lightning
  Serial.println("📡 GET /account/deposits/lightning?limit=10");
  String response = makeAuthenticatedRequest("GET", "/account/deposits/lightning", "limit=10");

  if (response == "") {
    Serial.println("❌ Aucune réponse de l'API deposits/lightning");
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(20, 100);
    tft.println("ERREUR API");
    tft.setCursor(20, 130);
    tft.println("Pas de réponse");
    tft.setCursor(20, 160);
    tft.println("historique dépôts");
    delay(3000);
    showWalletScreen();
    return;
  }

  Serial.println("Réponse deposits history: " + response.substring(0, 200) + "...");

  // Parser la réponse JSON
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, response);

  if (error) {
    Serial.println("❌ Erreur parsing JSON deposits history");
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(20, 100);
    tft.println("ERREUR PARSING");
    tft.setCursor(20, 130);
    tft.println("JSON historique");
    tft.setCursor(20, 160);
    tft.println("invalide");
    delay(3000);
    showWalletScreen();
    return;
  }

  // Extraire les données des dépôts
  if (doc.containsKey("data")) {
    JsonArray deposits = doc["data"];
    lightning_deposits_count = deposits.size();
    
    Serial.print("✅ ");
    Serial.print(lightning_deposits_count);
    Serial.println(" dépôts trouvés");
    
    int depositIndex = 0;
    for (JsonVariant deposit : deposits) {
      if (depositIndex < 10) {
        String id = deposit["id"].as<String>();
        long amount = deposit["amount"].as<long>();
        String createdAt = deposit["createdAt"].as<String>();
        String paymentHash = deposit["paymentHash"].as<String>();
        String comment = deposit["comment"].as<String>();
        
        // Format date (take just the date part)
        if (createdAt.length() > 10) {
          createdAt = createdAt.substring(0, 10);
        }
        
        // Store data
        lightning_deposits_amounts[depositIndex] = amount;
        lightning_deposits_dates[depositIndex] = createdAt;
        lightning_deposits_hashes[depositIndex] = paymentHash;
        lightning_deposits_comments[depositIndex] = comment;
        
        // Create detail string
        String detail = String(amount) + " sats - " + id.substring(0, 8) + "...";
        lightning_deposits_details[depositIndex] = detail;
        
        depositIndex++;
      }
    }
    
    Serial.println("✅ Historique dépôts chargé");
    showDepositHistoryScreen();
  } else {
    Serial.println("❌ Aucune donnée trouvée dans la réponse");
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(20, 100);
    tft.println("ERREUR:");
    tft.setCursor(20, 130);
    tft.println("Aucune donnée");
    tft.setCursor(20, 160);
    tft.println("dans la réponse");
    delay(3000);
    showWalletScreen();
  }
}

// =====================================================
// SHOW DEPOSIT HISTORY SCREEN
// =====================================================
void showDepositHistoryScreen() {
  fadeOut(); // Transition fluide
  
  tft.fillScreen(COLOR_BG);
  
  // Header avec navigation
  tft.fillRect(0, 0, 80, 40, 0x39E7); // HOME gris
  tft.fillRect(80, 0, 80, 40, COLOR_RED); // WALLET rouge (actif)
  tft.fillRect(160, 0, 80, 40, 0x39E7); // POSITIONS gris
  tft.fillRect(240, 0, 80, 40, 0x39E7); // HISTORY gris
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 12);
  tft.println("HOME");
  tft.setCursor(95, 12);
  tft.println("WALLET");
  tft.setCursor(170, 12);
  tft.println("POS");
  tft.setCursor(250, 12);
  tft.println("HIST");
  
  // Titre
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(2);
  tft.setCursor(40, 50);
  tft.println("DEPOSIT HISTORY");
  
  // Afficher les dépôts
  if (lightning_deposits_count == 0) {
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(50, 100);
    tft.println("No deposits");
    tft.setTextSize(1);
    tft.setCursor(30, 130);
    tft.println("found in your account");
  } else {
    int y_pos = 80;
    for (int i = 0; i < 8 && lightning_deposits_details[i] != ""; i++) {
      // Détails du dépôt
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(1);
      tft.setCursor(10, y_pos);
      tft.print(lightning_deposits_details[i]);
      
      // Montant en sats
      tft.setCursor(200, y_pos);
      tft.setTextColor(COLOR_GREEN);
      tft.print(lightning_deposits_amounts[i]);
      tft.print(" sats");
      
      // Date
      tft.setTextColor(0x39E7);
      tft.setCursor(10, y_pos + 12);
      tft.print(lightning_deposits_dates[i]);
      
      // Comment si présent
      if (lightning_deposits_comments[i] != "" && lightning_deposits_comments[i] != "null") {
        tft.setTextColor(TFT_YELLOW);
        tft.setCursor(100, y_pos + 12);
        String comment = lightning_deposits_comments[i];
        if (comment.length() > 15) {
          comment = comment.substring(0, 12) + "...";
        }
        tft.print(comment);
      }
      
      y_pos += 30;
      if (y_pos > 220) break;
    }
    
    // Total des dépôts
    tft.setTextColor(COLOR_CYAN);
    tft.setTextSize(1);
    tft.setCursor(10, 225);
    tft.print("Total deposits: ");
    tft.print(lightning_deposits_count);
  }
  
  Serial.println("Écran historique dépôts affiché");
}

// =====================================================
// SHOW DEPOSIT TEXT SCREEN (sans QR)
// =====================================================
void showDepositTextScreen(String address) {
  Serial.println("🔄 Début showDepositTextScreen");
  tft.fillScreen(TFT_BLACK);  // Black background

  // Add black borders to completely hide any TFT artifacts
  tft.fillRect(0, 0, 320, 1, TFT_BLACK);     // top
  tft.fillRect(0, 239, 320, 1, TFT_BLACK);   // bottom
  tft.fillRect(0, 0, 1, 240, TFT_BLACK);     // left
  tft.fillRect(319, 0, 1, 240, TFT_BLACK);   // right

  // Titre
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(2);
  tft.setCursor(70, 10);
  tft.println("DEPOSIT");

  // Indication du type
  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(10, 30);
  if (address.indexOf("lnurl") != -1 || address.indexOf("@") != -1) {
    tft.println("Type: LNURL (Lightning Address)");
  } else {
    tft.println("Type: Invoice classique");
  }

  // Instructions
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 50);
  tft.println("Copiez cette adresse dans votre");
  tft.setCursor(10, 65);
  tft.println("wallet Lightning:");

  // Afficher l'adresse en gros caractères
  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(1);

  // Diviser l'adresse en lignes pour qu'elle rentre
  String addr = address;
  int maxCharsPerLine = 25; // Environ 25 caractères par ligne à taille 1

  int startY = 85;
  int lineHeight = 15;

  for (int i = 0; i < addr.length(); i += maxCharsPerLine) {
    String line = addr.substring(i, min(i + maxCharsPerLine, (int)addr.length()));
    tft.setCursor(10, startY);
    tft.println(line);
    startY += lineHeight;

    if (startY > 200) break; // Éviter de dépasser l'écran
  }

  // Instructions finales
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 220);
  tft.println("Montant: " + String(deposit_amount_sats) + " sats");

  Serial.println("✅ Écran dépôt texte affiché");

  // Attendre 2 minutes puis retour auto
  unsigned long startTime = millis();
  while (millis() - startTime < 120000) { // 2 minutes = 120 secondes
    delay(100); // Petit délai pour éviter de bloquer complètement

    // Vérifier si l'utilisateur touche l'écran pour annuler
    uint16_t touchX, touchY;
    if (tft.getTouch(&touchX, &touchY)) {
      Serial.println("👆 Touch détecté - retour anticipé");
      break; // Sortir de la boucle d'attente
    }
  }

  // Timeout
  Serial.println("⏰ Timeout écran dépôt, retour auto");
  showWalletScreen();
}

// =====================================================
// SHOW DEPOSIT QR SCREEN
// =====================================================
// PNG DRAW CALLBACK FOR QR CODE DISPLAY
// =====================================================
int pngDraw(PNGDRAW *pDraw) {
  uint16_t lineBuffer[320]; // Buffer pour une ligne d'écran (320 pixels max)
  
  // Initialiser le buffer avec du blanc
  memset(lineBuffer, 0xFF, sizeof(lineBuffer));
  
  // Utiliser l'objet PNG global
  pngGlobal.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xFFFF);
  
  // Calculer la position Y pour centrer verticalement
  int startY = globalQrY + pDraw->y;
  
  // Afficher la ligne sur l'écran TFT (centrer horizontalement à globalQrX)
  tft.pushImage(globalQrX, startY, pDraw->iWidth, 1, lineBuffer);
  
  return 1; // Continuer le décodage
}

// =====================================================
// SHOW DEPOSIT QR SCREEN
// =====================================================
void showDepositQRScreen(String address) {
  Serial.println("🔄 Début showDepositQRScreen");
  tft.fillScreen(TFT_BLACK);  // Black background to hide blue lines
  
  // Add black borders to completely hide any TFT artifacts
  tft.fillRect(0, 0, 320, 1, TFT_BLACK);     // top
  tft.fillRect(0, 239, 320, 1, TFT_BLACK);   // bottom
  tft.fillRect(0, 0, 1, 240, TFT_BLACK);     // left
  tft.fillRect(319, 0, 1, 240, TFT_BLACK);   // right
  
  // Titre
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(2);
  tft.setCursor(70, 10);
  tft.println("DEPOSIT");

  // Indication du type de QR
  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(10, 30);
  if (address.indexOf("lnurl") != -1 || address.indexOf("@") != -1) {
    tft.println("Type: LNURL (Lightning Address)");
  } else {
    tft.println("Type: Invoice classique");
  }

  // Instructions
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 225);
  tft.println("Scannez avec wallet Lightning");
  tft.setCursor(10, 235);
  tft.println("Montant: " + String(deposit_amount_sats) + " sats");
  
  Serial.println("🌐 Generation QR code via service web...");
  
  // Encoder l'adresse pour l'URL (remplacer les caractères spéciaux)
  String encodedAddress = address;
  encodedAddress.replace(" ", "%20");
  encodedAddress.replace(":", "%3A");
  encodedAddress.replace("/", "%2F");
  encodedAddress.replace("?", "%3F");
  encodedAddress.replace("&", "%26");
  encodedAddress.replace("=", "%3D");
  encodedAddress.replace("+", "%2B");
  
  // Taille QR test 150x150 pour améliorer la lisibilité
  int qrSize = 150;
  int qrX = (320 - qrSize) / 2;
  int qrY = 240 - qrSize - 10; // 10px de marge en bas
  globalQrX = qrX; // Mettre à jour les variables globales
  globalQrY = qrY;
  String qrUrl = "https://api.qrserver.com/v1/create-qr-code/?size=150x150&format=png&ecc=H&margin=0&data=" + encodedAddress;
    // Préparer le fond blanc pour le QR (en bas, centré)
    tft.fillRect(qrX, qrY, qrSize, qrSize, TFT_WHITE);
  
  Serial.println("📡 URL QR: " + qrUrl);
  
  // Faire l'appel HTTP pour récupérer l'image QR
  HTTPClient http;
  http.begin(qrUrl);
  http.setTimeout(30000); // 30 secondes timeout
  
  Serial.println("📥 Téléchargement image QR...");
  unsigned long startGetTime = millis();
  int httpCode = http.GET();
  unsigned long getDuration = millis() - startGetTime;
  Serial.printf("🔍 Code HTTP: %d (durée: %lu ms)\n", httpCode, getDuration);
  
  if (httpCode != 200) {
    Serial.printf("❌ Erreur HTTP: %d\n", httpCode);
    
    // Afficher le début de la réponse d'erreur
    String errorResponse = http.getString();
    Serial.println("📄 Réponse d'erreur (100 premiers caractères):");
    Serial.println(errorResponse.substring(0, 100));
    
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(20, 100);
    tft.println("ERREUR RESEAU");
    tft.setCursor(20, 130);
    tft.println("Code: " + String(httpCode));
    delay(3000);
    showWalletScreen();
    return;
  }
  
  // Pour les réponses chunkées, utilisons getString() qui gère automatiquement les chunks
  Serial.println("📖 Récupération réponse complète...");
  unsigned long startStringTime = millis();
  String imageData = http.getString();
  unsigned long stringDuration = millis() - startStringTime;
  
  size_t dataLength = imageData.length();
  Serial.printf("📏 Données reçues: %d bytes (durée: %lu ms)\n", dataLength, stringDuration);
  
  http.end();
  
  // Vérifier que la taille est raisonnable pour une image QR 160x160 (devrait être autour de 1-15KB)
  if (dataLength < 500) {
    Serial.println("❌ Image trop petite - probablement une erreur");
    Serial.printf("📏 Taille reçue: %d bytes (attendu: 500-50000)\n", dataLength);
    Serial.println("📄 Début du contenu (hex):");
    for (int i = 0; i < min(64, (int)dataLength); i++) {
      if (i % 16 == 0) Serial.printf("\n%04X: ", i);
      Serial.printf("%02X ", (uint8_t)imageData[i]);
    }
    Serial.println("\n📄 Signature PNG attendue: 89 50 4E 47 0D 0A 1A 0A");
    Serial.printf("📄 Signature reçue: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                  (uint8_t)imageData[0], (uint8_t)imageData[1], (uint8_t)imageData[2], (uint8_t)imageData[3],
                  (uint8_t)imageData[4], (uint8_t)imageData[5], (uint8_t)imageData[6], (uint8_t)imageData[7]);
    Serial.println("\n📄 Début du contenu (ASCII):");
    for (int i = 0; i < min(100, (int)dataLength); i++) {
      char c = imageData[i];
      if (c >= 32 && c <= 126) {
        Serial.print(c);
      } else {
        Serial.print('.');
      }
    }
    Serial.println();
    
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(20, 100);
    tft.println("ERREUR IMAGE");
    tft.setCursor(20, 130);
    tft.println("Contenu invalide");
    delay(3000);
    showWalletScreen();
    return;
  }
  
  if (dataLength > 50000) {
    Serial.println("❌ Image trop grande");
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(20, 100);
    tft.println("ERREUR IMAGE");
    tft.setCursor(20, 130);
    tft.println("Trop volumineuse");
    delay(3000);
    showWalletScreen();
    return;
  }
  
  // Vérifier la signature PNG
  if (dataLength >= 8 && 
      (uint8_t)imageData[0] == 0x89 && (uint8_t)imageData[1] == 0x50 && 
      (uint8_t)imageData[2] == 0x4E && (uint8_t)imageData[3] == 0x47 &&
      (uint8_t)imageData[4] == 0x0D && (uint8_t)imageData[5] == 0x0A &&
      (uint8_t)imageData[6] == 0x1A && (uint8_t)imageData[7] == 0x0A) {
    Serial.println("✅ Signature PNG valide");
  } else {
    Serial.println("❌ Signature PNG invalide");
    Serial.printf("📄 Signature reçue: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                  (uint8_t)imageData[0], (uint8_t)imageData[1], (uint8_t)imageData[2], (uint8_t)imageData[3],
                  (uint8_t)imageData[4], (uint8_t)imageData[5], (uint8_t)imageData[6], (uint8_t)imageData[7]);
    
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(20, 100);
    tft.println("ERREUR IMAGE");
    tft.setCursor(20, 130);
    tft.println("PNG invalide");
    delay(3000);
    showWalletScreen();
    return;
  }
  
  // Convertir la String en buffer uint8_t pour PNGdec
  uint8_t* imageBuffer = (uint8_t*)imageData.c_str();
  
  // Maintenant décoder et afficher l'image PNG
  Serial.println("🖼️ Décodage PNG...");
  
  int result = pngGlobal.openRAM(imageBuffer, dataLength, pngDraw);
  
  if (result != PNG_SUCCESS) {
    Serial.printf("❌ Erreur ouverture PNG: %d\n", result);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(20, 100);
    tft.println("ERREUR PNG");
    tft.setCursor(20, 130);
    tft.println("Code: " + String(result));
    delay(3000);
    showWalletScreen();
    return;
  }
  
  // Effacer la zone d'instructions
  tft.fillRect(10, 225, 310, 15, TFT_BLACK);
  
  // Afficher "Affichage QR code..."
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 225);
  tft.println("Affichage QR code...");
  
  // Préparer le fond noir pour le QR (150x150, centré en bas) - contraste optimal
  tft.fillRect(globalQrX, globalQrY, 150, 150, TFT_BLACK);

  // Décoder et afficher l'image (le PNG doit être centré sur qrX, qrY)
  // PNGdec dessine à partir de pngDraw, il faut que pngDraw utilise qrX et qrY pour le placement
  result = pngGlobal.decode(NULL, 0);
  
  if (result != PNG_SUCCESS) {
    Serial.printf("❌ Erreur décodage PNG: %d\n", result);
    pngGlobal.close();
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(20, 100);
    tft.println("ERREUR DECODE");
    tft.setCursor(20, 130);
    tft.println("Code: " + String(result));
    delay(3000);
    showWalletScreen();
    return;
  }
  
  pngGlobal.close();
  // Note: imageBuffer pointe vers imageData.c_str(), pas besoin de free()
  
  Serial.println("✅ QR code affiché!");
  
  // Libérer explicitement la mémoire de l'image PNG pour éviter les fuites RAM
  imageData = ""; // Vider la String pour libérer la RAM
  Serial.println("🧹 Mémoire PNG libérée");
  
  // Effacer les messages temporaires
  tft.fillRect(20, 35, 280, 40, TFT_BLACK);
  
  // Instructions finales (supprimées pour laisser le QR respirer)
  // tft.setTextColor(TFT_WHITE);
  // tft.setTextSize(1);
  // tft.setCursor(10, 225);
  // tft.println("Scannez avec wallet Lightning");
  // tft.setCursor(10, 235);
  // tft.print("Montant: ");
  // tft.print(deposit_amount_sats);
  // tft.println(" sats");
  
  Serial.println("✅ Écran dépôt avec QR code affiché");
  
  // Attendre 2 minutes puis retour auto (suffisant pour permettre le scan)
  unsigned long startTime = millis();
  while (millis() - startTime < 120000) { // 2 minutes = 120 secondes
    delay(100); // Petit délai pour éviter de bloquer complètement
    
    // Vérifier si l'utilisateur touche l'écran pour annuler
    uint16_t touchX, touchY;
    if (tft.getTouch(&touchX, &touchY)) {
      Serial.println("👆 Touch détecté - retour anticipé");
      break; // Sortir de la boucle d'attente
    }
  }
  
  // Timeout
  Serial.println("⏰ Timeout écran dépôt, retour auto");
  showWalletScreen();
}