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

#define SD_CS 15
#define SD_MOSI 13
#define SD_MISO 12
#define SD_CLK 14

#define LED_RED 4
#define LED_GREEN 16
#define LED_BLUE 17

// ===== OBJETS =====
TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);
WebServer server(80);
DNSServer dnsServer;
PNG pngGlobal;

// ===== VARIABLES GLOBALES =====
String ssid_saved = "";
String password_saved = "";
String api_key = "";
String api_secret = "";
String api_passphrase = "";
String lightning_address = "";
String deposit_lnaddress = "";

bool config_mode = false;
bool api_connected = false;
float btc_price = 0;

SemaphoreHandle_t tft_mutex;
TaskHandle_t price_update_task = NULL;

// Navigation
enum Screen { SCREEN_HOME, SCREEN_WALLET, SCREEN_POSITIONS, SCREEN_HISTORY };
Screen current_screen = SCREEN_HOME;
bool screen_needs_redraw = true;

// Wallet data
long balance_sats = 0;
float balance_usd = 0.0;
long withdraw_amount_sats = 1000;
long deposit_amount_sats = 1000;

// Trade settings
int selected_leverage = 25;
String selected_side = "";
int selected_margin = 1000;

// Running trades
int running_trades_count = 0;
long margin_in_positions = 0;
String running_trades_details[5];
float running_trades_pnl[5];
long running_trades_pnl_sats[5];
String running_trades_ids[5];
float running_trades_entry[5];
int running_trades_leverage[5];
long running_trades_margin[5];

// Open orders
int open_orders_count = 0;
String open_orders_details[5];
String open_orders_ids[5];

// Closed trades
int closed_trades_count = 0;
String closed_trades_details[10];
float closed_trades_pnl[10];
String closed_trades_dates[10];

// Lightning deposits
int lightning_deposits_count = 0;
String lightning_deposits_details[10];
long lightning_deposits_amounts[10];
String lightning_deposits_dates[10];
String lightning_deposits_hashes[10];
String lightning_deposits_comments[10];

// LED RGB
bool led_blink_state = false;
unsigned long last_led_blink = 0;

// QR Code globals
int globalQrX = 80;
int globalQrY = 50;

// ===== COULEURS =====
#define COLOR_BG 0x0004
#define COLOR_RED 0xF800
#define COLOR_BLUE 0x001F
#define COLOR_CYAN 0x07FF
#define COLOR_GREEN 0x07E0

// ===== 🆕 OPTIMISATION : SYSTÈME NON-BLOQUANT =====
struct NonBlockingDelay {
  unsigned long start_time;
  unsigned long duration;
  bool active;
  void (*callback)();
  
  void set(unsigned long ms, void (*cb)() = nullptr) {
    start_time = millis();
    duration = ms;
    active = true;
    callback = cb;
  }
  
  bool check() {
    if (!active) return false;
    if (millis() - start_time >= duration) {
      active = false;
      if (callback) callback();
      return true;
    }
    return false;
  }
  
  void cancel() { active = false; }
};

NonBlockingDelay message_timer;
bool showing_message = false;

// ===== 🆕 OPTIMISATION : CACHE INTELLIGENT =====
struct CacheEntry {
  String data;
  unsigned long timestamp;
  unsigned long duration;
  bool valid;
  
  void set(String d, unsigned long dur) {
    data = d;
    timestamp = millis();
    duration = dur;
    valid = true;
  }
  
  bool isValid() {
    return valid && (millis() - timestamp < duration);
  }
  
  void invalidate() { valid = false; }
};

CacheEntry priceCache = {"", 0, 2000, false};
CacheEntry walletCache = {"", 0, 15000, false};
CacheEntry positionsCache = {"", 0, 10000, false};
CacheEntry ordersCache = {"", 0, 12000, false};
CacheEntry historyCache = {"", 0, 60000, false};
CacheEntry depositsCache = {"", 0, 60000, false};

void invalidateAllCache() {
  priceCache.invalidate();
  walletCache.invalidate();
  positionsCache.invalidate();
  ordersCache.invalidate();
  historyCache.invalidate();
  depositsCache.invalidate();
  Serial.println("🗑️ Cache invalidé");
}

// ===== 🆕 TOUCH DÉBOUNCE =====
unsigned long last_touch_time = 0;
int last_touch_x = -1;
int last_touch_y = -1;
const unsigned long TOUCH_DEBOUNCE = 120;

// ===== 🆕 ZONES TACTILES CONSTANTES =====
struct TouchZone {
  int x1, y1, x2, y2;
  bool contains(int x, int y) const {
    return x >= x1 && x <= x2 && y >= y1 && y <= y2;
  }
};

const TouchZone NAV_HOME = {0, 0, 80, 40};
const TouchZone NAV_WALLET = {80, 0, 160, 40};
const TouchZone NAV_POSITIONS = {160, 0, 240, 40};
const TouchZone NAV_HISTORY = {240, 0, 320, 40};

const TouchZone BTN_SELL = {250, 55, 320, 85};
const TouchZone BTN_BUY = {250, 90, 320, 120};
const TouchZone BTN_LEV_25 = {20, 140, 70, 160};
const TouchZone BTN_LEV_50 = {80, 140, 130, 160};
const TouchZone BTN_LEV_75 = {140, 140, 190, 160};
const TouchZone BTN_LEV_100 = {200, 140, 250, 160};
const TouchZone BTN_MARGIN_MINUS = {180, 195, 210, 215};
const TouchZone BTN_MARGIN_PLUS = {220, 195, 250, 215};
const TouchZone BTN_EXECUTE = {20, 220, 300, 240};

const TouchZone BTN_AMOUNT_100 = {20, 175, 65, 190};
const TouchZone BTN_AMOUNT_500 = {75, 175, 120, 190};
const TouchZone BTN_AMOUNT_1000 = {130, 175, 175, 190};
const TouchZone BTN_AMOUNT_5000 = {185, 175, 230, 190};

const TouchZone BTN_WITHDRAW = {250, 50, 315, 75};
const TouchZone BTN_DEPOSIT = {250, 85, 315, 110};
const TouchZone BTN_DEPOSIT_HISTORY = {250, 120, 315, 145};
const TouchZone BTN_RESET_CONFIG = {250, 210, 315, 235};
const TouchZone BTN_WITHDRAW_MINUS = {140, 220, 170, 240};
const TouchZone BTN_WITHDRAW_PLUS = {180, 220, 210, 240};

// ===== PROTOTYPES =====
void loadConfig();
bool saveConfig();
void startConfigPortal();
void handleRoot();
void handleSave();
void connectWiFi();
void testAPIConnection();
void syncTime();
void forceAPMode();

void showMainScreen();
void showWalletScreen();
void showPositionsScreen();
void showHistoryScreen();
void showDepositManagementScreen();
void showDepositHistoryScreen();
void showDepositQRScreen(String address);
void showDepositTextScreen(String address);
void switchScreen(Screen new_screen);

void updatePrice();
void updateWalletData();
void updateWalletBalance();
void updateRunningTrades();
void updateOpenOrders();
void updateClosedTrades();

void handleTouch();
void handleNavigation(int x, int y);
void handleHomeTouch(int x, int y);
void handleWalletTouch(int x, int y);
void handlePositionsTouch(int x, int y);

String makeAuthenticatedRequest(String method, String path, String data = "");
String generateSignature(String method, String path, String data, String timestamp);
void executeTrade(String side, int margin);
void closeTrade(String positionId);
void cancelOrder(String orderId);
void withdrawBalance();
void getDepositAddress();
void getDepositHistory();

void showError(String msg);
void showMessage(String msg, uint16_t color, unsigned long duration);
void fadeOut(int duration = 30);
void displayPrice(float price);
float fetchBTCPrice();

void setLedRGB(int red, int green, int blue);
void updateLedPnl();

void deriveEncryptionKey(uint8_t* key, size_t keySize);
String xorEncrypt(String data);
String xorDecrypt(String data);
String encryptData(const char* plaintext, size_t plaintextLen);
String decryptData(const char* ciphertext, size_t ciphertextLen);
bool validateLightningAddress(String address);
bool validateApiKey(String key);
bool validatePassphrase(String passphrase);
size_t base64Decode(const char* input, size_t inputLen, uint8_t* output, size_t outputMaxLen);

String resolveLightningAddress(String address);
String getPayUrl(String lnurl);
String generateInvoice(String payUrl, long amount_sats);

int pngDraw(PNGDRAW *pDraw);

// ===== 🆕 FADE OUT RAPIDE (ou désactivé) =====
void fadeOut(int duration) {
  // Option 1: Fade ultra-rapide
  int steps = 2;
  for(int i = 0; i < steps; i++) {
    tft.fillRect(0, i * 240 / steps, 320, 240 / steps, TFT_BLACK);
    delay(duration / steps);
  }
  
  // Option 2: Complètement désactivé (décommenter pour tester)
  // return;
}

// ===== 🆕 AFFICHAGE PRIX SÉPARÉ =====
void displayPrice(float price) {
  tft.fillRect(8, 58, 172, 35, COLOR_BG);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(4);
  tft.setCursor(10, 60);
  tft.print("$");
  tft.println((int)price);
}

// ===== 🆕 REQUÊTE PRIX SÉPARÉE =====
float fetchBTCPrice() {
  HTTPClient http;
  http.begin("https://api.lnmarkets.com/v3/futures/ticker");
  http.setTimeout(1500);
  
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    http.end();
    
    JsonDocument doc;
    if (deserializeJson(doc, payload) == DeserializationError::Ok) {
      return doc["index"].as<float>();
    }
  } else {
    http.end();
  }
  
  return 0;
}

// ===== 🆕 TÂCHE PRIX OPTIMISÉE =====
void priceUpdateTask(void *pvParameters) {
  Serial.println("🚀 Tâche prix optimisée");
  
  while (true) {
    if (api_connected && current_screen == SCREEN_HOME) {
      float new_price = fetchBTCPrice();
      
      if (new_price > 0 && abs(new_price - btc_price) > 0.01) {
        btc_price = new_price;
        
        if (xSemaphoreTake(tft_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
          displayPrice(btc_price);
          xSemaphoreGive(tft_mutex);
        }
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== LN MARKETS TOUCH OPTIMISÉ ===");
  
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  tft.init();
  tft.invertDisplay(true); // Invert display for ST7789
  // Initialize display and auto-detect rotation so the UI is shown in landscape
  tft.init();
  tft.fillScreen(COLOR_BG);
  // Try rotations 0..3 and pick one where width > height (landscape)
  int chosenRotation = 0;
  for (int r = 0; r < 4; r++) {
    tft.setRotation(r);
    // allow driver to apply rotation
    delay(50);
    int w = tft.width();
    int h = tft.height();
    if (w > h) {
      chosenRotation = r;
      break;
    }
  }
  // If auto-detect failed (driver may report static width/height), fallback to rotation 1
  // which commonly maps to landscape on many modules. Log chosen rotation for debugging.
  chosenRotation = 1; // Force rotation 1 (90°) for landscape
  Serial.printf("[INIT] FORCED rotation -> %d\n", chosenRotation);
  // Ensure rotation is applied
  tft.setRotation(chosenRotation);
  Serial.printf("[INIT] TFT rotation set to %d (w=%d h=%d)\n", chosenRotation, tft.width(), tft.height());
  tft.fillScreen(COLOR_BG);
  
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(2);
  tft.setCursor(50, 100);
  tft.println("BOOTING...");
  delay(500);
  
  SPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touch.begin();
  // Sync touchscreen rotation with the TFT rotation
  touch.setRotation(chosenRotation);
  
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  setLedRGB(0, 1, 1);
  delay(300);
  setLedRGB(0, 0, 0);
  
  if (!SPIFFS.begin(true)) {
    showError("SPIFFS FAIL!");
    while(1);
  }
  
  tft_mutex = xSemaphoreCreateMutex();
  
  loadConfig();
  
  bool shouldStartAP = false;
  
  if (ssid_saved == "" || password_saved == "") {
    shouldStartAP = true;
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid_saved.c_str(), password_saved.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
      shouldStartAP = true;
      WiFi.disconnect();
    }
  }
  
  if (shouldStartAP) {
    startConfigPortal();
  } else {
    connectWiFi();
  }
}

// ===== LOOP =====
void loop() {
  if (config_mode) {
    dnsServer.processNextRequest();
    server.handleClient();
    updateLedPnl();
    return;
  }
  
  message_timer.check();
  
  if (WiFi.status() != WL_CONNECTED) {
    showError("WiFi perdu");
    message_timer.set(3000, []() {
      connectWiFi();
    });
    return;
  }
  
  handleTouch();
  updateLedPnl();
  
  delay(10);
}

// ===== SWITCH SCREEN =====
void switchScreen(Screen new_screen) {
  if (new_screen == current_screen && !screen_needs_redraw) return;
  
  current_screen = new_screen;
  screen_needs_redraw = true;
  
  if (xSemaphoreTake(tft_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
    switch (new_screen) {
      case SCREEN_HOME:
        showMainScreen();
        break;
      case SCREEN_WALLET:
        updateWalletBalance();
        showWalletScreen();
        break;
      case SCREEN_POSITIONS:
        updateRunningTrades();
        updateOpenOrders();
        showPositionsScreen();
        break;
      case SCREEN_HISTORY:
        updateClosedTrades();
        showHistoryScreen();
        break;
    }
    xSemaphoreGive(tft_mutex);
    screen_needs_redraw = false;
  }
}

// ===== HANDLE TOUCH =====
void handleTouch() {
  if (millis() - last_touch_time < TOUCH_DEBOUNCE) return;
  
  if (!touch.touched()) {
    last_touch_x = last_touch_y = -1;
    return;
  }
  
  TS_Point p = touch.getPoint();
  if (p.z < 60) return;
  
  int x = map(p.x, 200, 3700, 0, 320);
  int y = map(p.y, 200, 3700, 0, 240);
  
  if (abs(x - last_touch_x) < 20 && abs(y - last_touch_y) < 20) return;
  
  last_touch_time = millis();
  last_touch_x = x;
  last_touch_y = y;
  
  if (y < 40) {
    handleNavigation(x, y);
    return;
  }
  
  switch (current_screen) {
    case SCREEN_HOME:
      handleHomeTouch(x, y);
      break;
    case SCREEN_WALLET:
      handleWalletTouch(x, y);
      break;
    case SCREEN_POSITIONS:
      handlePositionsTouch(x, y);
      break;
    default:
      break;
  }
}

// ===== NAVIGATION =====
void handleNavigation(int x, int y) {
  Screen new_screen = current_screen;
  
  if (NAV_HOME.contains(x, y)) {
    new_screen = SCREEN_HOME;
  } else if (NAV_WALLET.contains(x, y)) {
    new_screen = SCREEN_WALLET;
  } else if (NAV_POSITIONS.contains(x, y)) {
    new_screen = SCREEN_POSITIONS;
  } else if (NAV_HISTORY.contains(x, y)) {
    getDepositHistory();
    return;
  }
  
  if (new_screen != current_screen) {
    switchScreen(new_screen);
  }
}

// ===== HOME TOUCH =====
void handleHomeTouch(int x, int y) {
  bool needs_refresh = false;
  
  if (BTN_SELL.contains(x, y)) {
    selected_side = "sell";
    needs_refresh = true;
  } else if (BTN_BUY.contains(x, y)) {
    selected_side = "buy";
    needs_refresh = true;
  } else if (BTN_LEV_25.contains(x, y)) {
    selected_leverage = 25;
    needs_refresh = true;
  } else if (BTN_LEV_50.contains(x, y)) {
    selected_leverage = 50;
    needs_refresh = true;
  } else if (BTN_LEV_75.contains(x, y)) {
    selected_leverage = 75;
    needs_refresh = true;
  } else if (BTN_LEV_100.contains(x, y)) {
    selected_leverage = 100;
    needs_refresh = true;
  } else if (BTN_AMOUNT_100.contains(x, y)) {
    selected_margin = 100;
    needs_refresh = true;
  } else if (BTN_AMOUNT_500.contains(x, y)) {
    selected_margin = 500;
    needs_refresh = true;
  } else if (BTN_AMOUNT_1000.contains(x, y)) {
    selected_margin = 1000;
    needs_refresh = true;
  } else if (BTN_AMOUNT_5000.contains(x, y)) {
    selected_margin = 5000;
    needs_refresh = true;
  } else if (BTN_MARGIN_MINUS.contains(x, y)) {
    selected_margin -= 100;
    if (selected_margin < 100) selected_margin = 100;
    needs_refresh = true;
  } else if (BTN_MARGIN_PLUS.contains(x, y)) {
    selected_margin += 100;
    if (selected_margin > 10000) selected_margin = 10000;
    needs_refresh = true;
  } else if (BTN_EXECUTE.contains(x, y)) {
    if (selected_side != "") {
      executeTrade(selected_side, selected_margin);
    }
    return;
  }
  
  if (needs_refresh) {
    screen_needs_redraw = true;
    switchScreen(SCREEN_HOME);
  }
}

// ===== WALLET TOUCH =====
void handleWalletTouch(int x, int y) {
  bool needs_refresh = false;
  
  if (y >= 195 && y <= 210) {
    if (x >= 20 && x <= 65) {
      withdraw_amount_sats = 100;
      needs_refresh = true;
    } else if (x >= 75 && x <= 120) {
      withdraw_amount_sats = 500;
      needs_refresh = true;
    } else if (x >= 130 && x <= 175) {
      withdraw_amount_sats = 1000;
      needs_refresh = true;
    } else if (x >= 185 && x <= 230) {
      withdraw_amount_sats = 2000;
      needs_refresh = true;
    }
  } else if (BTN_WITHDRAW_MINUS.contains(x, y)) {
    withdraw_amount_sats -= 100;
    if (withdraw_amount_sats < 100) withdraw_amount_sats = 100;
    needs_refresh = true;
  } else if (BTN_WITHDRAW_PLUS.contains(x, y)) {
    withdraw_amount_sats += 100;
    if (withdraw_amount_sats > 100000) withdraw_amount_sats = 100000;
    needs_refresh = true;
  } else if (BTN_WITHDRAW.contains(x, y)) {
    withdrawBalance();
    return;
  } else if (BTN_DEPOSIT.contains(x, y)) {
    getDepositAddress();
    return;
  } else if (BTN_DEPOSIT_HISTORY.contains(x, y)) {
    getDepositHistory();
    return;
  } else if (BTN_RESET_CONFIG.contains(x, y)) {
    showMessage("RESET CONFIG...", COLOR_RED, 2000);
    ssid_saved = "";
    password_saved = "";
    api_key = "public";
    api_secret = "public";
    api_passphrase = "public";
    lightning_address = "";
    deposit_lnaddress = "";
    SPIFFS.remove("/config.json");
    message_timer.set(2000, []() {
      ESP.restart();
    });
    return;
  }
  
  if (needs_refresh) {
    screen_needs_redraw = true;
    switchScreen(SCREEN_WALLET);
  }
}

// ===== POSITIONS TOUCH =====
void handlePositionsTouch(int x, int y) {
  int y_pos = 85;
  
  for (int i = 0; i < running_trades_count && i < 5; i++) {
    if (x >= 20 && x <= 70 && y >= y_pos + 25 && y <= y_pos + 45) {
      closeTrade(running_trades_ids[i]);
      return;
    }
    y_pos += 50;
    if (y_pos > 200) break;
  }
  
  if (open_orders_count > 0 && y_pos < 180) {
    y_pos += 15;
    for (int i = 0; i < open_orders_count && i < 5; i++) {
      if (x >= 20 && x <= 75 && y >= y_pos + 12 && y <= y_pos + 32) {
        cancelOrder(open_orders_ids[i]);
        return;
      }
      y_pos += 35;
      if (y_pos > 220) break;
    }
  }
}

// ===== UPDATE FUNCTIONS =====
void updateWalletData() {
  updateWalletBalance();
  updateRunningTrades();
  updateOpenOrders();
  updateClosedTrades();
}

void updateWalletBalance() {
  if (walletCache.isValid()) return;
  if (api_key == "" || api_key == "public") return;
  
  String response = makeAuthenticatedRequest("GET", "/account", "");
  
  if (response != "") {
    walletCache.set(response, 15000);
    
    JsonDocument doc;
    if (deserializeJson(doc, response) == DeserializationError::Ok) {
      if (doc["balance"].is<long>()) {
        balance_sats = doc["balance"].as<long>();
        if (btc_price > 0) {
          balance_usd = (balance_sats / 100000000.0) * btc_price;
        }
      }
    }
  }
}

void updateRunningTrades() {
  if (positionsCache.isValid()) return;
  if (api_key == "" || api_key == "public") return;
  
  String response = makeAuthenticatedRequest("GET", "/futures/isolated/trades/running", "");
  
  if (response != "") {
    positionsCache.set(response, 10000);
    
    running_trades_count = 0;
    margin_in_positions = 0;
    
    for (int i = 0; i < 5; i++) {
      running_trades_details[i] = "";
      running_trades_pnl[i] = 0.0;
      running_trades_pnl_sats[i] = 0;
      running_trades_ids[i] = "";
      running_trades_entry[i] = 0.0;
      running_trades_leverage[i] = 0;
      running_trades_margin[i] = 0;
    }
    
    JsonDocument doc;
    if (deserializeJson(doc, response) == DeserializationError::Ok && doc.is<JsonArray>()) {
      JsonArray trades = doc.as<JsonArray>();
      running_trades_count = trades.size();
      
      int idx = 0;
      for (JsonVariant trade : trades) {
        long margin = trade["margin"].as<long>();
        margin_in_positions += margin;
        
        if (idx < 5) {
          running_trades_ids[idx] = trade["id"].as<String>();
          running_trades_margin[idx] = margin;
          running_trades_leverage[idx] = trade["leverage"].as<int>();
          running_trades_entry[idx] = trade["entryPrice"].as<float>();
          running_trades_pnl_sats[idx] = trade["pl"].as<long>();
          
          String side = trade["side"].as<String>();
          running_trades_details[idx] = side + " " + String(running_trades_leverage[idx]) + "x " + String(margin) + "s";
          
          float pnl_percent = (margin > 0) ? (running_trades_pnl_sats[idx] / (float)margin) * 100.0 : 0;
          running_trades_pnl[idx] = pnl_percent;
          
          idx++;
        }
      }
    }
  }
}

void updateOpenOrders() {
  if (ordersCache.isValid()) return;
  if (api_key == "" || api_key == "public") return;
  
  String response = makeAuthenticatedRequest("GET", "/futures/isolated/trades/open", "");
  
  if (response != "") {
    ordersCache.set(response, 12000);
    
    open_orders_count = 0;
    for (int i = 0; i < 5; i++) {
      open_orders_details[i] = "";
      open_orders_ids[i] = "";
    }
    
    JsonDocument doc;
    if (deserializeJson(doc, response) == DeserializationError::Ok && doc.is<JsonArray>()) {
      JsonArray orders = doc.as<JsonArray>();
      open_orders_count = orders.size();
      
      int idx = 0;
      for (JsonVariant order : orders) {
        if (idx < 5) {
          open_orders_ids[idx] = order["id"].as<String>();
          String side = order["side"].as<String>();
          String type = order["type"].as<String>();
          int leverage = order["leverage"].as<int>();
          long margin = order["margin"].as<long>();
          float price = order["price"].as<float>();
          
          open_orders_details[idx] = side + " " + type + " " + String(leverage) + "x " + String(margin) + "s @" + String((int)price);
          idx++;
        }
      }
    }
  }
}

void updateClosedTrades() {
  if (historyCache.isValid()) return;
  if (api_key == "" || api_key == "public") return;
  
  String response = makeAuthenticatedRequest("GET", "/futures/isolated/trades/closed", "limit=10");
  
  if (response != "") {
    historyCache.set(response, 60000);
    
    closed_trades_count = 0;
    for (int i = 0; i < 10; i++) {
      closed_trades_details[i] = "";
      closed_trades_pnl[i] = 0.0;
      closed_trades_dates[i] = "";
    }
    
    JsonDocument doc;
    if (deserializeJson(doc, response) == DeserializationError::Ok && doc.is<JsonArray>()) {
      JsonArray trades = doc.as<JsonArray>();
      closed_trades_count = trades.size();
      
      int idx = 0;
      for (JsonVariant trade : trades) {
        if (idx < 10) {
          String id = trade["id"].as<String>();
          String side = trade["side"].as<String>();
          int leverage = trade["leverage"].as<int>();
          long pl = trade["pl"].as<long>();
          String closedAt = trade["closedAt"].as<String>();
          
          if (closedAt.length() > 10) {
            closedAt = closedAt.substring(0, 10);
          }
          
          closed_trades_details[idx] = side + " " + String(leverage) + "x " + id.substring(0, 8) + "...";
          closed_trades_pnl[idx] = pl / 1000.0;
          closed_trades_dates[idx] = closedAt;
          idx++;
        }
      }
    }
  }
}

// ===== MESSAGE NON-BLOQUANT =====
void showMessage(String msg, uint16_t color, unsigned long duration) {
  if (xSemaphoreTake(tft_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(color);
    tft.setTextSize(2);
    tft.setCursor(40, 100);
    tft.println(msg);
    xSemaphoreGive(tft_mutex);
  }
  
  showing_message = true;
  message_timer.set(duration, []() {
    showing_message = false;
    screen_needs_redraw = true;
    switchScreen(current_screen);
  });
}

// ===== SHOW SCREENS =====
void showMainScreen() {
  tft.fillScreen(COLOR_BG);
  
  tft.fillRect(0, 0, 80, 40, COLOR_BLUE);
  tft.fillRect(80, 0, 80, 40, 0x39E7);
  tft.fillRect(160, 0, 80, 40, 0x39E7);
  tft.fillRect(240, 0, 80, 40, 0x39E7);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 12); tft.println("HOME");
  tft.setCursor(95, 12); tft.println("WALLET");
  tft.setCursor(170, 12); tft.println("POS");
  tft.setCursor(250, 12); tft.println("HIST");
  
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(1);
  tft.setCursor(20, 50);
  tft.println("BITCOIN PRICE");
  
  displayPrice(btc_price);
  
  uint16_t sellColor = (selected_side == "sell") ? COLOR_RED : 0x39E7;
  uint16_t buyColor = (selected_side == "buy") ? COLOR_GREEN : 0x39E7;
  
  tft.fillRoundRect(250, 55, 70, 30, 8, sellColor);
  tft.fillRoundRect(250, 90, 70, 30, 8, buyColor);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(268, 63); tft.println("SELL");
  tft.setCursor(268, 98); tft.println("BUY");
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 130);
  tft.println("LEVERAGE:");
  
  uint16_t color25 = (selected_leverage == 25) ? TFT_YELLOW : 0x39E7;
  uint16_t color50 = (selected_leverage == 50) ? TFT_YELLOW : 0x39E7;
  uint16_t color75 = (selected_leverage == 75) ? TFT_YELLOW : 0x39E7;
  uint16_t color100 = (selected_leverage == 100) ? TFT_YELLOW : 0x39E7;
  
  tft.fillRoundRect(20, 140, 50, 20, 3, color25);
  tft.fillRoundRect(80, 140, 50, 20, 3, color50);
  tft.fillRoundRect(140, 140, 50, 20, 3, color75);
  tft.fillRoundRect(200, 140, 50, 20, 3, color100);
  
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(30, 145); tft.println("25x");
  tft.setCursor(90, 145); tft.println("50x");
  tft.setCursor(145, 145); tft.println("75x");
  tft.setCursor(205, 145); tft.println("100x");
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 165);
  tft.println("QUICK AMOUNTS:");
  
  tft.fillRoundRect(20, 175, 45, 15, 3, 0x39E7);
  tft.fillRoundRect(75, 175, 45, 15, 3, 0x39E7);
  tft.fillRoundRect(130, 175, 45, 15, 3, 0x39E7);
  tft.fillRoundRect(185, 175, 45, 15, 3, 0x39E7);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(25, 180); tft.println("100");
  tft.setCursor(80, 180); tft.println("500");
  tft.setCursor(135, 180); tft.println("1000");
  tft.setCursor(190, 180); tft.println("5000");
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 200);
  tft.print("MARGIN: ");
  tft.print(selected_margin);
  tft.println(" sats");
  
  tft.fillRoundRect(180, 195, 30, 20, 3, 0x39E7);
  tft.fillRoundRect(220, 195, 30, 20, 3, 0x39E7);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(190, 200); tft.println("-");
  tft.setCursor(230, 200); tft.println("+");
  
  tft.fillRoundRect(20, 220, 280, 20, 5, 0x39E7);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(100, 225);
  tft.println("EXECUTER");
}

void showWalletScreen() {
  tft.fillScreen(COLOR_BG);
  
  tft.fillRect(0, 0, 80, 40, 0x39E7);
  tft.fillRect(80, 0, 80, 40, COLOR_BLUE);
  tft.fillRect(160, 0, 80, 40, 0x39E7);
  tft.fillRect(240, 0, 80, 40, 0x39E7);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 12); tft.println("HOME");
  tft.setCursor(95, 12); tft.println("WALLET");
  tft.setCursor(170, 12); tft.println("POS");
  tft.setCursor(250, 12); tft.println("HIST");
  
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(2);
  tft.setCursor(80, 50);
  tft.println("MY WALLET");
  
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
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 185);
  tft.println("MONTANTS RAPIDES:");
  
  tft.fillRoundRect(20, 195, 45, 15, 3, 0x39E7);
  tft.fillRoundRect(75, 195, 45, 15, 3, 0x39E7);
  tft.fillRoundRect(130, 195, 45, 15, 3, 0x39E7);
  tft.fillRoundRect(185, 195, 45, 15, 3, 0x39E7);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(25, 200); tft.println("100");
  tft.setCursor(80, 200); tft.println("500");
  tft.setCursor(135, 200); tft.println("1000");
  tft.setCursor(190, 200); tft.println("2000");
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 215);
  tft.print("AJUSTER RETRAIT: ");
  
  tft.fillRoundRect(140, 210, 30, 20, 3, 0x39E7);
  tft.fillRoundRect(180, 210, 30, 20, 3, 0x39E7);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(150, 215); tft.println("-");
  tft.setCursor(190, 215); tft.println("+");
  
  tft.fillRoundRect(250, 50, 65, 25, 5, 0xFD20);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(255, 58); tft.println("WITHDRAW");
  
  tft.fillRoundRect(250, 85, 65, 25, 5, 0x07E0);
  tft.setCursor(260, 93); tft.println("DEPOSIT");
  
  tft.fillRoundRect(250, 120, 65, 25, 5, 0x39E7);
  tft.setCursor(255, 128); tft.println("HISTORY");
  
  tft.fillRoundRect(260, 210, 55, 20, 5, 0x4208);
  tft.setCursor(265, 215); tft.println("RESET");
}

void showPositionsScreen() {
  tft.fillScreen(COLOR_BG);
  
  tft.fillRect(0, 0, 80, 40, 0x39E7);
  tft.fillRect(80, 0, 80, 40, 0x39E7);
  tft.fillRect(160, 0, 80, 40, COLOR_BLUE);
  tft.fillRect(240, 0, 80, 40, 0x39E7);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 12); tft.println("HOME");
  tft.setCursor(95, 12); tft.println("WALLET");
  tft.setCursor(170, 12); tft.println("POS");
  tft.setCursor(250, 12); tft.println("HIST");
  
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(2);
  tft.setCursor(60, 50);
  tft.println("POSITIONS & ORDERS");
  
  int y_pos = 80;
  int total_items = 0;
  
  if (running_trades_count > 0) {
    tft.setTextColor(COLOR_CYAN);
    tft.setTextSize(1);
    tft.setCursor(20, y_pos - 15);
    tft.println("RUNNING POSITIONS:");
    y_pos += 5;
    
    for (int i = 0; i < running_trades_count && i < 5; i++) {
      if (running_trades_details[i] == "") break;
      
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(1);
      tft.setCursor(20, y_pos);
      tft.print(running_trades_details[i]);
      
      tft.setCursor(20, y_pos + 12);
      tft.print("Entry: $");
      tft.print((int)running_trades_entry[i]);
      
      tft.setCursor(120, y_pos + 12);
      tft.print("Now: $");
      tft.print((int)btc_price);
      
      long pnl_sats = running_trades_pnl_sats[i];
      float pnl_percent = running_trades_pnl[i];
      
      tft.setCursor(200, y_pos + 12);
      if (pnl_sats >= 0) {
        tft.setTextColor(COLOR_GREEN);
        tft.print("+");
      } else {
        tft.setTextColor(COLOR_RED);
      }
      tft.print(pnl_sats);
      tft.print("s ");
      
      if (pnl_percent >= 0) {
        tft.setTextColor(COLOR_GREEN);
        tft.print("+");
      } else {
        tft.setTextColor(COLOR_RED);
      }
      tft.print(pnl_percent, 1);
      tft.print("%");
      
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
  
  if (open_orders_count > 0 && y_pos < 180) {
    tft.setTextColor(COLOR_CYAN);
    tft.setTextSize(1);
    tft.setCursor(20, y_pos);
    tft.println("OPEN ORDERS:");
    y_pos += 15;
    
    for (int i = 0; i < open_orders_count && i < 5; i++) {
      if (open_orders_details[i] == "") break;
      
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(1);
      tft.setCursor(20, y_pos);
      tft.print(open_orders_details[i]);
      
      tft.fillRoundRect(20, y_pos + 12, 55, 20, 3, 0xFD20);
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(1);
      tft.setCursor(25, y_pos + 17);
      tft.println("CANCEL");
      
      y_pos += 35;
      total_items++;
      if (y_pos > 220) break;
    }
  }
  
  if (total_items == 0) {
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(40, 100);
    tft.println("No positions or orders");
  }
}

void showHistoryScreen() {
  tft.fillScreen(COLOR_BG);
  
  tft.fillRect(0, 0, 80, 40, 0x39E7);
  tft.fillRect(80, 0, 80, 40, 0x39E7);
  tft.fillRect(160, 0, 80, 40, 0x39E7);
  tft.fillRect(240, 0, 80, 40, COLOR_BLUE);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 12); tft.println("HOME");
  tft.setCursor(95, 12); tft.println("WALLET");
  tft.setCursor(170, 12); tft.println("POS");
  tft.setCursor(250, 12); tft.println("HIST");
  
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(2);
  tft.setCursor(70, 50);
  tft.println("TRADE HISTORY");
  
  if (closed_trades_count == 0) {
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(50, 100);
    tft.println("No closed trades");
  } else {
    int y_pos = 80;
    for (int i = 0; i < closed_trades_count && i < 10; i++) {
      if (closed_trades_details[i] == "") break;
      
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(1);
      tft.setCursor(10, y_pos);
      tft.print(closed_trades_details[i]);
      
      float pnl = closed_trades_pnl[i];
      tft.setCursor(200, y_pos);
      if (pnl >= 0) {
        tft.setTextColor(COLOR_GREEN);
        tft.print("+");
      } else {
        tft.setTextColor(COLOR_RED);
      }
      tft.print(pnl, 2);
      tft.print(" sats");
      
      tft.setTextColor(0x39E7);
      tft.setCursor(10, y_pos + 12);
      tft.print(closed_trades_dates[i]);
      
      y_pos += 30;
      if (y_pos > 220) break;
    }
  }
}

// ===== GET DEPOSIT ADDRESS =====
void getDepositAddress() {
  Serial.println("📥 getDepositAddress() appelée");
  
  if (deposit_lnaddress == "") {
    showMessage("Adresse non configurée", COLOR_RED, 3000);
    message_timer.set(3000, []() {
      switchScreen(SCREEN_WALLET);
    });
    return;
  }
  
  // Utiliser QR code local avec QRCode library
  showDepositManagementScreen();
}

// ===== SHOW DEPOSIT MANAGEMENT SCREEN =====
void showDepositManagementScreen() {
  if (deposit_lnaddress == "") {
    showMessage("Adresse non configurée", COLOR_RED, 3000);
    message_timer.set(3000, []() {
      switchScreen(SCREEN_WALLET);
    });
    return;
  }
  
  Serial.println("📥 Génération QR code dépôt...");
  
  if (xSemaphoreTake(tft_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
    tft.fillScreen(TFT_BLACK);
    
    QRCode qrcode;
    uint8_t qrcodeData[qrcode_getBufferSize(6)];
    qrcode_initText(&qrcode, qrcodeData, 6, ECC_LOW, deposit_lnaddress.c_str());
    
    int qrSize = qrcode.size * 4;
    int qrX = (320 - qrSize) / 2;
    int qrY = (240 - qrSize) / 2;
    
    tft.fillRect(qrX, qrY, qrSize, qrSize, TFT_BLACK);
    
    for (uint8_t y = 0; y < qrcode.size; y++) {
      for (uint8_t x = 0; x < qrcode.size; x++) {
        if (qrcode_getModule(&qrcode, x, y)) {
          tft.fillRect(qrX + x * 4, qrY + y * 4, 4, 4, TFT_WHITE);
        }
      }
    }
    
    xSemaphoreGive(tft_mutex);
  }
  
  Serial.println("✅ QR code dépôt affiché");
  
  message_timer.set(120000, []() {
    switchScreen(SCREEN_WALLET);
  });
}

// ===== 🆕 SHOW DEPOSIT QR SCREEN (avec PNG web) =====
void showDepositQRScreen(String address) {
  Serial.println("🔄 showDepositQRScreen avec PNG web");
  
  if (xSemaphoreTake(tft_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
    tft.fillScreen(TFT_BLACK);
    
    tft.setTextColor(COLOR_CYAN);
    tft.setTextSize(2);
    tft.setCursor(70, 10);
    tft.println("DEPOSIT");
    
    tft.setTextColor(TFT_YELLOW);
    tft.setTextSize(1);
    tft.setCursor(10, 30);
    if (address.indexOf("lnurl") != -1 || address.indexOf("@") != -1) {
      tft.println("Type: LNURL");
    } else {
      tft.println("Type: Invoice");
    }
    
    xSemaphoreGive(tft_mutex);
  }
  
  // Encoder l'adresse
  String encodedAddress = address;
  encodedAddress.replace(" ", "%20");
  encodedAddress.replace(":", "%3A");
  encodedAddress.replace("/", "%2F");
  encodedAddress.replace("?", "%3F");
  encodedAddress.replace("&", "%26");
  encodedAddress.replace("=", "%3D");
  encodedAddress.replace("+", "%2B");
  
  int qrSize = 150;
  int qrX = (320 - qrSize) / 2;
  int qrY = 50;
  globalQrX = qrX;
  globalQrY = qrY;
  
  String qrUrl = "https://api.qrserver.com/v1/create-qr-code/?size=150x150&format=png&ecc=H&margin=0&data=" + encodedAddress;
  
  HTTPClient http;
  http.begin(qrUrl);
  http.setTimeout(30000);
  
  int httpCode = http.GET();
  
  if (httpCode != 200) {
    http.end();
    showMessage("Erreur QR", COLOR_RED, 2000);
    return;
  }
  
  String imageData = http.getString();
  http.end();
  
  if (imageData.length() < 500 || imageData.length() > 50000) {
    showMessage("Image invalide", COLOR_RED, 2000);
    return;
  }
  
  uint8_t* imageBuffer = (uint8_t*)imageData.c_str();
  
  if (xSemaphoreTake(tft_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
    tft.fillRect(qrX, qrY, 150, 150, TFT_BLACK);
    
    int result = pngGlobal.openRAM(imageBuffer, imageData.length(), pngDraw);
    
    if (result == PNG_SUCCESS) {
      pngGlobal.decode(NULL, 0);
      pngGlobal.close();
    }
    
    xSemaphoreGive(tft_mutex);
  }
  
  Serial.println("✅ QR PNG affiché");
  
  message_timer.set(120000, []() {
    switchScreen(SCREEN_WALLET);
  });
}

// ===== 🆕 SHOW DEPOSIT TEXT SCREEN =====
void showDepositTextScreen(String address) {
  if (xSemaphoreTake(tft_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
    tft.fillScreen(TFT_BLACK);
    
    tft.setTextColor(COLOR_CYAN);
    tft.setTextSize(2);
    tft.setCursor(70, 10);
    tft.println("DEPOSIT");
    
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(10, 50);
    tft.println("Copiez cette adresse:");
    
    tft.setTextColor(TFT_GREEN);
    tft.setTextSize(1);
    
    int y_pos = 85;
    int maxChars = 25;
    
    for (int i = 0; i < address.length(); i += maxChars) {
      String line = address.substring(i, min(i + maxChars, (int)address.length()));
      tft.setCursor(10, y_pos);
      tft.println(line);
      y_pos += 15;
      if (y_pos > 200) break;
    }
    
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(10, 220);
    tft.print("Montant: ");
    tft.print(deposit_amount_sats);
    tft.println(" sats");
    
    xSemaphoreGive(tft_mutex);
  }
  
  message_timer.set(120000, []() {
    switchScreen(SCREEN_WALLET);
  });
}

// ===== GET DEPOSIT HISTORY =====
void getDepositHistory() {
  if (api_key == "" || api_key == "public") {
    showMessage("Clés API requises", COLOR_RED, 2000);
    message_timer.set(2000, []() {
      switchScreen(SCREEN_WALLET);
    });
    return;
  }
  
  showMessage("Loading deposits...", COLOR_CYAN, 0);
  
  lightning_deposits_count = 0;
  for (int i = 0; i < 10; i++) {
    lightning_deposits_details[i] = "";
    lightning_deposits_amounts[i] = 0;
    lightning_deposits_dates[i] = "";
    lightning_deposits_hashes[i] = "";
    lightning_deposits_comments[i] = "";
  }
  
  String response = makeAuthenticatedRequest("GET", "/account/deposits/lightning", "limit=10");
  
  if (response == "") {
    showMessage("Erreur API", COLOR_RED, 2000);
    message_timer.set(2000, []() {
      switchScreen(SCREEN_WALLET);
    });
    return;
  }
  
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, response) == DeserializationError::Ok && doc["data"].is<JsonArray>()) {
    JsonArray deposits = doc["data"];
    lightning_deposits_count = deposits.size();
    
    int idx = 0;
    for (JsonVariant deposit : deposits) {
      if (idx < 10) {
        String id = deposit["id"].as<String>();
        long amount = deposit["amount"].as<long>();
        String createdAt = deposit["createdAt"].as<String>();
        
        if (createdAt.length() > 10) {
          createdAt = createdAt.substring(0, 10);
        }
        
        lightning_deposits_amounts[idx] = amount;
        lightning_deposits_dates[idx] = createdAt;
        lightning_deposits_hashes[idx] = deposit["paymentHash"].as<String>();
        lightning_deposits_comments[idx] = deposit["comment"].as<String>();
        lightning_deposits_details[idx] = String(amount) + " sats - " + id.substring(0, 8) + "...";
        
        idx++;
      }
    }
  }
  
  message_timer.cancel();
  showDepositHistoryScreen();
}

// ===== SHOW DEPOSIT HISTORY SCREEN =====
void showDepositHistoryScreen() {
  if (xSemaphoreTake(tft_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
    tft.fillScreen(COLOR_BG);
    
    tft.fillRect(0, 0, 80, 40, 0x39E7);
    tft.fillRect(80, 0, 80, 40, COLOR_BLUE);
    tft.fillRect(160, 0, 80, 40, 0x39E7);
    tft.fillRect(240, 0, 80, 40, 0x39E7);
    
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(20, 12); tft.println("HOME");
    tft.setCursor(95, 12); tft.println("WALLET");
    tft.setCursor(170, 12); tft.println("POS");
    tft.setCursor(250, 12); tft.println("HIST");
    
    tft.setTextColor(COLOR_CYAN);
    tft.setTextSize(2);
    tft.setCursor(40, 50);
    tft.println("DEPOSIT HISTORY");
    
    if (lightning_deposits_count == 0) {
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(2);
      tft.setCursor(50, 100);
      tft.println("No deposits");
    } else {
      int y_pos = 80;
      for (int i = 0; i < 8 && lightning_deposits_details[i] != ""; i++) {
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(1);
        tft.setCursor(10, y_pos);
        tft.print(lightning_deposits_details[i]);
        
        tft.setCursor(200, y_pos);
        tft.setTextColor(COLOR_GREEN);
        tft.print(lightning_deposits_amounts[i]);
        tft.print(" sats");
        
        tft.setTextColor(0x39E7);
        tft.setCursor(10, y_pos + 12);
        tft.print(lightning_deposits_dates[i]);
        
        if (lightning_deposits_comments[i] != "" && lightning_deposits_comments[i] != "null") {
          tft.setTextColor(TFT_YELLOW);
          tft.setCursor(100, y_pos + 12);
          String comment = lightning_deposits_comments[i];
          if (comment.length() > 15) comment = comment.substring(0, 12) + "...";
          tft.print(comment);
        }
        
        y_pos += 30;
        if (y_pos > 220) break;
      }
      
      tft.setTextColor(COLOR_CYAN);
      tft.setTextSize(1);
      tft.setCursor(10, 225);
      tft.print("Total: ");
      tft.print(lightning_deposits_count);
    }
    
    xSemaphoreGive(tft_mutex);
  }
}

// ===== EXECUTE TRADE =====
void executeTrade(String side, int margin) {
  if (api_key == "" || api_key == "public") {
    showMessage("Clés API requises", COLOR_RED, 2000);
    return;
  }
  
  Serial.printf("🚀 Trade: %s %d sats\n", side.c_str(), margin);
  
  String jsonData = "{";
  jsonData += "\"type\":\"market\",";
  jsonData += "\"side\":\"" + side + "\",";
  jsonData += "\"margin\":" + String(margin) + ",";
  jsonData += "\"leverage\":" + String(selected_leverage);
  jsonData += "}";
  
  String response = makeAuthenticatedRequest("POST", "/futures/isolated/trade", jsonData);
  
  if (response != "") {
    JsonDocument doc;
    if (deserializeJson(doc, response) == DeserializationError::Ok) {
      if (doc["id"].is<String>()) {
        invalidateAllCache();
        showMessage("TRADE REUSSI!", COLOR_GREEN, 2000);
        message_timer.set(2000, []() {
          switchScreen(SCREEN_HOME);
        });
      } else if (doc["message"].is<String>()) {
        String errorMsg = doc["message"];
        showMessage("ERREUR: " + errorMsg.substring(0, 15), COLOR_RED, 3000);
        message_timer.set(3000, []() {
          switchScreen(SCREEN_HOME);
        });
      }
    }
  }
}

// ===== CLOSE TRADE =====
void closeTrade(String positionId) {
  if (api_key == "" || api_key == "public") return;
  
  Serial.println("🔴 Close: " + positionId);
  
  String jsonData = "{\"id\":\"" + positionId + "\"}";
  String response = makeAuthenticatedRequest("POST", "/futures/isolated/trade/close", jsonData);
  
  if (response != "") {
    JsonDocument doc;
    if (deserializeJson(doc, response) == DeserializationError::Ok) {
      if (doc["id"].is<String>() || doc["pl"].is<long>()) {
        invalidateAllCache();
        showMessage("POSITION CLOSED!", COLOR_GREEN, 2000);
        message_timer.set(2000, []() {
          updateWalletData();
          switchScreen(SCREEN_POSITIONS);
        });
      }
    }
  }
}

// ===== CANCEL ORDER =====
void cancelOrder(String orderId) {
  if (api_key == "" || api_key == "public") return;
  
  Serial.println("🟠 Cancel: " + orderId);
  
  String jsonData = "{\"id\":\"" + orderId + "\"}";
  String response = makeAuthenticatedRequest("POST", "/futures/isolated/trade/cancel", jsonData);
  
  if (response != "") {
    JsonDocument doc;
    if (deserializeJson(doc, response) == DeserializationError::Ok) {
      if (doc["id"].is<String>()) {
        ordersCache.invalidate();
        showMessage("ORDER CANCELED!", 0xFD20, 2000);
        message_timer.set(2000, []() {
          updateOpenOrders();
          switchScreen(SCREEN_POSITIONS);
        });
      }
    }
  }
}

// ===== WITHDRAW BALANCE =====
void withdrawBalance() {
  if (api_key == "" || api_key == "public") return;
  
  if (lightning_address == "") {
    showMessage("Adresse non configurée", COLOR_RED, 3000);
    message_timer.set(3000, []() {
      switchScreen(SCREEN_WALLET);
    });
    return;
  }
  
  Serial.printf("💰 Withdraw %ld sats to %s\n", withdraw_amount_sats, lightning_address.c_str());
  
  String lnurl = resolveLightningAddress(lightning_address);
  if (lnurl == "") {
    showMessage("Adresse invalide", COLOR_RED, 2000);
    return;
  }
  
  String payUrl = getPayUrl(lnurl);
  if (payUrl == "") {
    showMessage("LNURL indisponible", COLOR_RED, 2000);
    return;
  }
  
  String invoice = generateInvoice(payUrl, withdraw_amount_sats);
  if (invoice == "") {
    showMessage("Invoice échouée", COLOR_RED, 2000);
    return;
  }
  
  String jsonData = "{\"invoice\":\"" + invoice + "\"}";
  String response = makeAuthenticatedRequest("POST", "/account/withdraw/lightning", jsonData);
  
  if (response != "") {
    JsonDocument doc;
    if (deserializeJson(doc, response) == DeserializationError::Ok) {
      if (doc["id"].is<String>()) {
        walletCache.invalidate();
        showMessage("WITHDRAW DONE!", COLOR_GREEN, 2000);
        message_timer.set(2000, []() {
          updateWalletBalance();
          switchScreen(SCREEN_WALLET);
        });
      } else if (doc["message"].is<String>()) {
        String errorMsg = doc["message"];
        showMessage("ERREUR: " + errorMsg.substring(0, 15), COLOR_RED, 3000);
      }
    }
  }
}

// ===== MAKE AUTHENTICATED REQUEST =====
String makeAuthenticatedRequest(String method, String path, String data) {
  HTTPClient http;
  String url = "https://api.lnmarkets.com/v3" + path;
  
  String queryString = "";
  if ((method == "GET" || method == "DELETE") && data != "") {
    queryString = data;
    url += "?" + queryString;
  }
  
  http.begin(url);
  http.setTimeout(1500);
  
  if (api_key != "" && api_key != "public" && api_secret != "" && api_secret != "public") {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long long timestamp_ms = (unsigned long long)(tv.tv_sec) * 1000ULL + (unsigned long long)(tv.tv_usec) / 1000ULL;
    String timestamp = String(timestamp_ms);
    
    String dataForSignature = "";
    if (method == "GET" || method == "DELETE") {
      if (data != "") dataForSignature = "?" + data;
    } else {
      dataForSignature = data;
    }
    
    String fullPath = "/v3" + path;
    String signature = generateSignature(method, fullPath, dataForSignature, timestamp);
    
    if (method == "POST" || method == "PUT") {
      http.addHeader("Content-Type", "application/json");
    }
    
    http.addHeader("LNM-ACCESS-KEY", api_key);
    http.addHeader("LNM-ACCESS-PASSPHRASE", api_passphrase);
    http.addHeader("LNM-ACCESS-TIMESTAMP", timestamp);
    http.addHeader("LNM-ACCESS-SIGNATURE", signature);
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
    if (httpCode >= 400) {
      Serial.printf("❌ HTTP %d\n", httpCode);
    }
  }
  
  http.end();
  return response;
}

// ===== GENERATE SIGNATURE =====
String generateSignature(String method, String path, String data, String timestamp) {
  String methodLower = method;
  methodLower.toLowerCase();
  String payload = timestamp + methodLower + path + data;
  
  byte hmacResult[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char*)api_secret.c_str(), api_secret.length());
  mbedtls_md_hmac_update(&ctx, (const unsigned char*)payload.c_str(), payload.length());
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);
  
  return base64::encode(hmacResult, 32);
}

// ===== LED RGB P&L =====
void updateLedPnl() {
  if (config_mode) {
    static unsigned long last_beat = 0;
    static int beat_phase = 0;
    unsigned long now = millis();
    
    switch (beat_phase) {
      case 0:
        if (now - last_beat > 1500) {
          last_beat = now;
          beat_phase = 1;
          setLedRGB(0, 1, 1);  // Azur (cyan)
        }
        break;
      case 1:
        if (now - last_beat > 150) {
          last_beat = now;
          beat_phase = 2;
          setLedRGB(0, 0, 0);  // Off
        }
        break;
      case 2:
        if (now - last_beat > 100) {
          last_beat = now;
          beat_phase = 3;
          setLedRGB(0, 1, 1);  // Azur (cyan)
        }
        break;
      case 3:
        if (now - last_beat > 150) {
          last_beat = now;
          beat_phase = 0;
          setLedRGB(0, 0, 0);  // Off
        }
        break;
    }
    return;
  }
  
  if (running_trades_count == 0) {
    setLedRGB(0, 0, 0);
    return;
  }
  
  float total_margin = 0;
  float total_pnl_sats = 0;
  
  for (int i = 0; i < running_trades_count; i++) {
    total_margin += running_trades_margin[i];
    total_pnl_sats += running_trades_pnl_sats[i];
  }
  
  float pnl_percent = (total_margin > 0) ? (total_pnl_sats / total_margin) * 100.0 : 0;
  float pnl_abs = abs(pnl_percent);
  
  int current_blink_speed = 0;
  bool continuous_mode = false;
  
  if (pnl_abs < 5) {
    current_blink_speed = 2000;
  } else if (pnl_abs < 25) {
    current_blink_speed = 1000;
  } else if (pnl_abs < 50) {
    current_blink_speed = 500;
  } else if (pnl_abs < 75) {
    current_blink_speed = 250;
  } else if (pnl_abs < 100) {
    current_blink_speed = 125;
  } else {
    continuous_mode = true;
  }
  
  if (continuous_mode) {
    if (pnl_percent >= 0) {
      setLedRGB(0, 1, 0);
    } else {
      setLedRGB(1, 0, 0);
    }
    return;
  }
  
  if (millis() - last_led_blink > current_blink_speed) {
    led_blink_state = !led_blink_state;
    last_led_blink = millis();
    
    if (led_blink_state) {
      if (pnl_percent >= 0) {
        setLedRGB(0, 1, 0);
      } else {
        setLedRGB(1, 0, 0);
      }
    } else {
      setLedRGB(0, 0, 0);
    }
  }
}

void setLedRGB(int red, int green, int blue) {
  digitalWrite(LED_RED, red ? LOW : HIGH);
  digitalWrite(LED_GREEN, green ? LOW : HIGH);
  digitalWrite(LED_BLUE, blue ? LOW : HIGH);
}

// ===== LIGHTNING FUNCTIONS =====
String resolveLightningAddress(String address) {
  int atIndex = address.indexOf('@');
  if (atIndex == -1) return "";
  
  String user = address.substring(0, atIndex);
  String domain = address.substring(atIndex + 1);
  
  return "https://" + domain + "/.well-known/lnurlp/" + user;
}

String getPayUrl(String lnurl) {
  HTTPClient http;
  http.begin(lnurl);
  http.setTimeout(5000);
  
  int httpCode = http.GET();
  String response = http.getString();
  http.end();
  
  if (httpCode != 200) return "";
  
  JsonDocument doc;
  if (deserializeJson(doc, response) != DeserializationError::Ok) return "";
  
  if (!doc["callback"].is<String>()) return "";
  
  return doc["callback"].as<String>();
}

String generateInvoice(String payUrl, long amount_sats) {
  long amount_msats = amount_sats * 1000;
  String url = payUrl + "?amount=" + String(amount_msats);
  
  HTTPClient http;
  http.begin(url);
  http.setTimeout(5000);
  
  int httpCode = http.GET();
  String response = http.getString();
  http.end();
  
  if (httpCode != 200) return "";
  
  JsonDocument doc;
  if (deserializeJson(doc, response) != DeserializationError::Ok) return "";
  
  if (!doc["pr"].is<String>()) return "";
  
  return doc["pr"].as<String>();
}

// ===== CONFIG PORTAL =====
void startConfigPortal() {
  config_mode = true;
  
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(40, 30);
  tft.println("MODE CONFIG");
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 80);
  tft.println("1. Connecte WiFi a:");
  
  tft.setTextColor(COLOR_GREEN);
  tft.setTextSize(2);
  tft.setCursor(40, 100);
  tft.print("DEGEN");
  tft.setTextColor(COLOR_RED);
  tft.println("BEAT");
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 140);
  tft.println("2. Ouvre navigateur:");
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(60, 160);
  tft.println("192.168.4.1");
  
  WiFi.softAP("DEGENBEAT");
  dnsServer.start(53, "*", WiFi.softAPIP());
  
  server.on("/", handleRoot);
  server.on("/save", []() {
    if (server.method() == HTTP_POST) {
      handleSave();
    } else {
      server.sendHeader("Location", "/", true);
      server.send(302, "text/plain", "");
    }
  });
  server.on("/favicon.ico", []() { server.send(204); });
  server.onNotFound([]() { handleRoot(); });
  
  server.begin();
}

void handleRoot() {
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
  </style>
</head>
<body>
  <div class='container'>
    <h1>⚡ LN MARKETS TOUCH</h1>
    <form action='/save' method='POST'>
      <label>WiFi SSID</label>
      <input type='text' name='ssid' required>
      <label>WiFi Password</label>
      <input type='password' name='password' required>
      <label>API Key</label>
      <input type='text' name='api_key' placeholder='Optionnel'>
      <label>API Secret</label>
      <input type='password' name='api_secret' placeholder='Optionnel'>
      <label>Passphrase</label>
      <input type='password' name='api_passphrase' placeholder='Optionnel'>
      <label>Lightning Address</label>
      <input type='text' name='lightning_address' placeholder='user@domain.com'>
      <label>Deposit LN Address</label>
      <input type='text' name='deposit_lnaddress' placeholder='user@domain.com'>
      <button type='submit'>💾 SAUVEGARDER</button>
    </form>
  </div>
</body>
</html>
  )rawliteral";
  
  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.args() == 0) {
    server.send(400, "text/html", "<h1>Erreur: Aucun argument</h1>");
    return;
  }
  
  ssid_saved = server.arg("ssid");
  password_saved = server.arg("password");
  
  String raw_api_key = server.arg("api_key");
  String raw_api_secret = server.arg("api_secret");
  String raw_api_passphrase = server.arg("api_passphrase");
  
  lightning_address = server.arg("lightning_address");
  deposit_lnaddress = server.arg("deposit_lnaddress");
  
  if (raw_api_key != "" && !validateApiKey(raw_api_key)) {
    server.send(400, "text/html", "<h1>Erreur: API Key invalide</h1>");
    return;
  }
  
  if (raw_api_secret != "" && !validateApiKey(raw_api_secret)) {
    server.send(400, "text/html", "<h1>Erreur: API Secret invalide</h1>");
    return;
  }
  
  if (raw_api_passphrase != "" && !validatePassphrase(raw_api_passphrase)) {
    server.send(400, "text/html", "<h1>Erreur: Passphrase invalide</h1>");
    return;
  }
  
  api_key = (raw_api_key == "") ? "public" : raw_api_key;
  api_secret = (raw_api_secret == "") ? "public" : raw_api_secret;
  api_passphrase = (raw_api_passphrase == "") ? "public" : raw_api_passphrase;
  
  saveConfig();
  
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta http-equiv='refresh' content='3;url=/'>
  <style>
    body { font-family: Arial; background: #0a0a0a; color: #00ff00; text-align: center; padding: 50px; }
    h1 { font-size: 48px; }
  </style>
</head>
<body>
  <h1>✓ SAUVEGARDÉ!</h1>
  <p>Redémarrage...</p>
</body>
</html>
  )rawliteral";
  
  server.send(200, "text/html", html);
  delay(3000);
  ESP.restart();
}

// ===== LOAD CONFIG =====
void loadConfig() {
  if (!SPIFFS.exists("/config.json")) return;
  
  fs::File file = SPIFFS.open("/config.json", "r");
  if (!file) return;
  
  String content = file.readString();
  file.close();
  
  JsonDocument doc;
  if (deserializeJson(doc, content) != DeserializationError::Ok) return;
  
  ssid_saved = doc["ssid"].as<String>();
  password_saved = doc["password"].as<String>();
  
  String enc_key = doc["api_key"].as<String>();
  String enc_secret = doc["api_secret"].as<String>();
  String enc_pass = doc["api_passphrase"].as<String>();
  
  if (enc_key.startsWith("XOR:")) {
    String decrypted = xorDecrypt(enc_key.substring(4));
    api_key = (validateApiKey(decrypted)) ? decrypted : "public";
  } else if (enc_key.startsWith("ENC:")) {
    String decrypted = decryptData(enc_key.substring(4).c_str(), enc_key.length() - 4);
    api_key = (validateApiKey(decrypted)) ? decrypted : "public";
  } else {
    api_key = (enc_key != "" && validateApiKey(enc_key)) ? enc_key : "public";
  }
  
  if (enc_secret.startsWith("XOR:")) {
    String decrypted = xorDecrypt(enc_secret.substring(4));
    api_secret = (validateApiKey(decrypted)) ? decrypted : "public";
  } else if (enc_secret.startsWith("ENC:")) {
    String decrypted = decryptData(enc_secret.substring(4).c_str(), enc_secret.length() - 4);
    api_secret = (validateApiKey(decrypted)) ? decrypted : "public";
  } else {
    api_secret = (enc_secret != "" && validateApiKey(enc_secret)) ? enc_secret : "public";
  }
  
  if (enc_pass.startsWith("XOR:")) {
    String decrypted = xorDecrypt(enc_pass.substring(4));
    api_passphrase = (validatePassphrase(decrypted)) ? decrypted : "public";
  } else if (enc_pass.startsWith("ENC:")) {
    String decrypted = decryptData(enc_pass.substring(4).c_str(), enc_pass.length() - 4);
    api_passphrase = (validatePassphrase(decrypted)) ? decrypted : "public";
  } else {
    api_passphrase = (enc_pass != "" && validatePassphrase(enc_pass)) ? enc_pass : "public";
  }
  
  lightning_address = doc["lightning_address"].as<String>();
  deposit_lnaddress = doc["deposit_lnaddress"].as<String>();
}

// ===== SAVE CONFIG =====
bool saveConfig() {
  JsonDocument doc;
  
  doc["ssid"] = ssid_saved;
  doc["password"] = password_saved;
  
  if (api_key != "" && api_key != "public") {
    String encrypted = xorEncrypt(api_key);
    doc["api_key"] = "XOR:" + encrypted;
  } else {
    doc["api_key"] = api_key;
  }
  
  if (api_secret != "" && api_secret != "public") {
    String encrypted = xorEncrypt(api_secret);
    doc["api_secret"] = "XOR:" + encrypted;
  } else {
    doc["api_secret"] = api_secret;
  }
  
  if (api_passphrase != "" && api_passphrase != "public") {
    String encrypted = xorEncrypt(api_passphrase);
    doc["api_passphrase"] = "XOR:" + encrypted;
  } else {
    doc["api_passphrase"] = api_passphrase;
  }
  
  doc["lightning_address"] = lightning_address;
  doc["deposit_lnaddress"] = deposit_lnaddress;
  
  fs::File file = SPIFFS.open("/config.json", "w");
  if (!file) return false;
  
  size_t written = serializeJson(doc, file);
  file.close();
  
  return (written > 0);
}

// ===== CONNECT WIFI =====
void connectWiFi() {
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(2);
  tft.setCursor(30, 100);
  tft.println("Connexion WiFi...");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid_saved.c_str(), password_saved.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi: %s\n", WiFi.localIP().toString().c_str());
    syncTime();
    testAPIConnection();
  } else {
    showError("WiFi ECHEC!");
    delay(2000);
    forceAPMode();
  }
}

// ===== 🆕 FORCE AP MODE =====
void forceAPMode() {
  Serial.println("🔄 FORÇAGE MODE AP");
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  delay(1000);
  startConfigPortal();
}

// ===== SYNC TIME =====
void syncTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  
  int timeout = 0;
  while (time(nullptr) < 1000000000 && timeout < 20) {
    delay(500);
    timeout++;
  }
}

// ===== TEST API =====
void testAPIConnection() {
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(2);
  tft.setCursor(50, 100);
  tft.println("Test API...");
  
  HTTPClient http;
  http.begin("https://api.lnmarkets.com/v3/futures/ticker");
  http.setTimeout(3000);
  
  int httpCode = http.GET();
  http.end();
  
  if (httpCode == 200) {
    api_connected = true;
    
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(COLOR_GREEN);
    tft.setTextSize(3);
    tft.setCursor(60, 100);
    tft.println("CONNECTE!");
    
    delay(1000);
    
    xTaskCreatePinnedToCore(
      priceUpdateTask,
      "PriceUpdate",
      8192,
      NULL,
      1,
      &price_update_task,
      1
    );
    
    switchScreen(SCREEN_HOME);
  } else {
    api_connected = false;
    showError("API ERREUR!");
    delay(3000);
    forceAPMode();
  }
}

// ===== SHOW ERROR =====
void showError(String msg) {
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_RED);
  tft.setTextSize(2);
  tft.setCursor(20, 100);
  tft.println(msg);
}

// ===== SECURITY FUNCTIONS =====
void deriveEncryptionKey(uint8_t* key, size_t keySize) {
  if (password_saved.length() == 0) {
    memset(key, 0, keySize);
    return;
  }
  
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  uint8_t hash[32];
  
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char*)password_saved.c_str(), password_saved.length());
  mbedtls_md_finish(&ctx, hash);
  mbedtls_md_free(&ctx);
  
  memcpy(key, hash, keySize);
}

String xorEncrypt(String data) {
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

// ===== 🆕 AES ENCRYPT/DECRYPT (pour compatibilité legacy) =====
String encryptData(const char* plaintext, size_t plaintextLen) {
  static uint8_t paddedPlaintext[256];
  static uint8_t encrypted[256];
  
  if (plaintextLen > 200) return "";
  
  uint8_t key[32];
  deriveEncryptionKey(key, sizeof(key));
  
  uint8_t iv[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
  
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  
  int ret = mbedtls_aes_setkey_enc(&aes, key, 256);
  if (ret != 0) {
    mbedtls_aes_free(&aes);
    return "";
  }
  
  size_t paddedLen = ((plaintextLen + 15) / 16) * 16;
  
  memset(paddedPlaintext, 0, sizeof(paddedPlaintext));
  memcpy(paddedPlaintext, plaintext, plaintextLen);
  uint8_t padding = 16 - (plaintextLen % 16);
  memset(paddedPlaintext + plaintextLen, padding, padding);
  
  memset(encrypted, 0, sizeof(encrypted));
  ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, paddedLen, iv, paddedPlaintext, encrypted);
  if (ret != 0) {
    mbedtls_aes_free(&aes);
    return "";
  }
  
  String base64Result = base64::encode(encrypted, paddedLen);
  mbedtls_aes_free(&aes);
  
  return base64Result;
}

String decryptData(const char* ciphertext, size_t ciphertextLen) {
  static uint8_t encrypted[256];
  static uint8_t decrypted[256];
  
  uint8_t key[32];
  deriveEncryptionKey(key, sizeof(key));
  
  uint8_t iv[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
  
  memset(encrypted, 0, sizeof(encrypted));
  size_t encryptedLen = base64Decode(ciphertext, ciphertextLen, encrypted, sizeof(encrypted));
  if (encryptedLen == 0 || encryptedLen % 16 != 0 || encryptedLen > 200) {
    return "";
  }
  
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  
  int ret = mbedtls_aes_setkey_dec(&aes, key, 256);
  if (ret != 0) {
    mbedtls_aes_free(&aes);
    return "";
  }
  
  memset(decrypted, 0, sizeof(decrypted));
  ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, encryptedLen, iv, encrypted, decrypted);
  if (ret != 0) {
    mbedtls_aes_free(&aes);
    return "";
  }
  
  uint8_t padding = decrypted[encryptedLen - 1];
  if (padding > 16 || padding == 0) {
    mbedtls_aes_free(&aes);
    return "";
  }
  
  for (size_t i = encryptedLen - padding; i < encryptedLen; i++) {
    if (decrypted[i] != padding) {
      mbedtls_aes_free(&aes);
      return "";
    }
  }
  
  size_t actualLen = encryptedLen - padding;
  String result = String((char*)decrypted, actualLen);
  
  mbedtls_aes_free(&aes);
  
  return result;
}

bool validateLightningAddress(String address) {
  if (address.length() == 0) return true;
  
  int atIndex = address.indexOf('@');
  if (atIndex <= 0 || atIndex >= address.length() - 1) return false;
  
  String domain = address.substring(atIndex + 1);
  return (domain.indexOf('.') != -1);
}

bool validateApiKey(String key) {
  if (key.length() == 0 || key == "public") return true;
  if (key.length() < 20) return false;
  
  for (char c : key) {
    if (!isalnum(c) && c != '/' && c != '+' && c != '=') return false;
  }
  
  return true;
}

bool validatePassphrase(String passphrase) {
  if (passphrase.length() == 0 || passphrase == "public") return true;
  if (passphrase.length() < 8) return false;
  
  for (char c : passphrase) {
    if (!isalnum(c)) return false;
  }
  
  return true;
}

size_t base64Decode(const char* input, size_t inputLen, uint8_t* output, size_t outputMaxLen) {
  static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  
  size_t outputLen = 0;
  int bits = 0;
  int bits_count = 0;
  
  for (size_t i = 0; i < inputLen && outputLen < outputMaxLen; ++i) {
    char c = input[i];
    if (c == '=') break;
    
    const char* pos = strchr(base64_chars, c);
    if (!pos) continue;
    
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

// ===== PNG DRAW CALLBACK =====
int pngDraw(PNGDRAW *pDraw) {
  uint16_t lineBuffer[320];
  memset(lineBuffer, 0xFF, sizeof(lineBuffer));
  
  pngGlobal.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xFFFF);
  
  int startY = globalQrY + pDraw->y;
  tft.pushImage(globalQrX, startY, pDraw->iWidth, 1, lineBuffer);
  
  return 1;
}