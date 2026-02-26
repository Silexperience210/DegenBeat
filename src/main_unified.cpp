/*
 * DegenBeat - Version UNIFIÉE Multi-Hardware
 * 
 * Ce fichier montre comment structurer le main.cpp pour supporter
 * plusieurs hardwares via des directives de préprocesseur.
 * 
 * Hardwares supportés:
 * - HARDWARE_SUNTON: Sunton ESP32-2432S028R
 * - HARDWARE_LCDWIKI_S3: LCDWiki ESP32-S3 2.8inch Display
 */

// ============================================================================
// SECTION 1: INCLUDES ET CONFIGURATION HARDWARE
// ============================================================================

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <TFT_eSPI.h>
#include <mbedtls/md.h>
#include <mbedtls/aes.h>
#include <base64.h>
#include <time.h>
#include <PNGdec.h>
#include <qrcode.h>

// Configuration hardware
#if defined(HARDWARE_SUNTON)
    // ========== SUNTON ESP32-2432S028R ==========
    #include <XPT2046_Touchscreen.h>
    #include <SPI.h>
    
    #define TFT_BL 21
    #define TOUCH_CS 33
    #define TOUCH_IRQ 36
    #define TOUCH_MOSI 32
    #define TOUCH_MISO 39
    #define TOUCH_CLK 25
    
    #define LED_RED 4
    #define LED_GREEN 16
    #define LED_BLUE 17
    
    #define SCREEN_WIDTH 320
    #define SCREEN_HEIGHT 240
    #define SCREEN_ROTATION 1  // Paysage
    
    XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);
    
#elif defined(HARDWARE_LCDWIKI_S3)
    // ========== LCDWIKI ESP32-S3 2.8INCH ==========
    #include <Wire.h>
    #include <FT6336U.h>
    #include <FastLED.h>
    
    #define TFT_BL 45
    #define TOUCH_SDA 16
    #define TOUCH_SCL 15
    #define TOUCH_RST 18
    #define TOUCH_IRQ 17
    
    #define RGB_LED_PIN 42
    #define NUM_LEDS 1
    
    #define SCREEN_WIDTH 240
    #define SCREEN_HEIGHT 320
    #define SCREEN_ROTATION 0  // Portrait
    
    FT6336U ft6336u(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_IRQ);
    CRGB leds[NUM_LEDS];
    
#else
    #error "Aucun hardware défini! Utilisez -DHARDWARE_SUNTON ou -DHARDWARE_LCDWIKI_S3"
#endif

// ============================================================================
// SECTION 2: OBJETS ET VARIABLES GLOBALES (communs)
// ============================================================================

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite priceSprite = TFT_eSprite(&tft);
WebServer server(80);
DNSServer dnsServer;
PNG pngGlobal;

// Variables globales
String ssid_saved = "";
String password_saved = "";
String api_key = "";
String api_secret = "";
String api_passphrase = "";
String lightning_address = "";
String deposit_lnaddress = "";

bool config_mode = false;
bool api_connected = false;
volatile float btc_price = 0;

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

// Couleurs
#define COLOR_BG 0x0004
#define COLOR_RED 0xF800
#define COLOR_BLUE 0x001F
#define COLOR_CYAN 0x07FF
#define COLOR_GREEN 0x07E0

// ============================================================================
// SECTION 3: FONCTIONS HARDWARE-ABSTRAITES
// ============================================================================

// Initialisation du touch selon le hardware
void setupTouch() {
#if defined(HARDWARE_SUNTON)
    SPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    touch.begin();
    touch.setRotation(SCREEN_ROTATION);
    
#elif defined(HARDWARE_LCDWIKI_S3)
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    ft6336u.begin();
#endif
}

// Lecture du touch avec coordonnées normalisées
bool readTouch(int &x, int &y, int &z) {
#if defined(HARDWARE_SUNTON)
    if (!touch.touched()) return false;
    TS_Point p = touch.getPoint();
    if (p.z < 60) return false;
    
    // Mapping pour 320x240 paysage
    x = map(p.x, 200, 3700, 0, 320);
    y = map(p.y, 200, 3700, 0, 240);
    z = p.z;
    return true;
    
#elif defined(HARDWARE_LCDWIKI_S3)
    if (!ft6336u.read_touch()) return false;
    
    x = ft6336u.get_touch_x();
    y = ft6336u.get_touch_y();
    z = 100; // Le FT6336U n'a pas de valeur de pression
    
    // Vérifier les limites
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) return false;
    return true;
#else
    return false;
#endif
}

// Initialisation des LEDs
void setupLEDs() {
#if defined(HARDWARE_SUNTON)
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH);
    
#elif defined(HARDWARE_LCDWIKI_S3)
    FastLED.addLeds<WS2812, RGB_LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(50);
    leds[0] = CRGB::Black;
    FastLED.show();
#endif
}

// Contrôle LED RGB
void setLedRGB(int red, int green, int blue) {
#if defined(HARDWARE_SUNTON)
    digitalWrite(LED_RED, red ? LOW : HIGH);
    digitalWrite(LED_GREEN, green ? LOW : HIGH);
    digitalWrite(LED_BLUE, blue ? LOW : HIGH);
    
#elif defined(HARDWARE_LCDWIKI_S3)
    leds[0] = CRGB(red * 255, green * 255, blue * 255);
    FastLED.show();
#endif
}

// ============================================================================
// SECTION 4: FONCTIONS UI ADAPTATIVES (exemples)
// ============================================================================

// Affichage du prix adapté à la résolution
void displayPrice(float price) {
    priceSprite.fillSprite(COLOR_BG);
    priceSprite.setTextColor(TFT_WHITE);
    
#if defined(HARDWARE_SUNTON)
    // 320x240 - plus d'espace horizontal
    priceSprite.setTextSize(4);
    priceSprite.setCursor(2, 2);
    priceSprite.print("$");
    priceSprite.print((int)price);
    priceSprite.pushSprite(8, 58);
    
#elif defined(HARDWARE_LCDWIKI_S3)
    // 240x320 - portrait, texte plus petit
    priceSprite.setTextSize(3);
    priceSprite.setCursor(2, 2);
    priceSprite.print("$");
    priceSprite.print((int)price);
    priceSprite.pushSprite(10, 65);
#endif
}

// ============================================================================
// SECTION 5: SETUP ET LOOP (structure commune)
// ============================================================================

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== DEGENBEAT ===");
    
    // Init LEDs
    setupLEDs();
    setLedRGB(0, 0, 1);
    
    // Init Display
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    tft.init();
    tft.setRotation(SCREEN_ROTATION);
    
    // Init Sprite
    priceSprite.setColorDepth(16);
#if defined(HARDWARE_SUNTON)
    priceSprite.createSprite(172, 35);
#elif defined(HARDWARE_LCDWIKI_S3)
    priceSprite.createSprite(130, 35);
#endif
    
    // Init Touch
    setupTouch();
    
    setLedRGB(0, 1, 1);
    delay(300);
    setLedRGB(0, 0, 0);
    
    // Init SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS FAIL!");
        while(1);
    }
    
    tft_mutex = xSemaphoreCreateMutex();
    
    // Suite du setup...
    loadConfig();
    // ... etc
}

void loop() {
    // Loop commune...
}

// ============================================================================
// SECTION 6: FONCTIONS PLACEHOLDERS (à compléter)
// ============================================================================

void loadConfig() {}

// ... (toutes les autres fonctions du main.cpp original)

/*
 * NOTES POUR L'INTÉGRATION:
 * 
 * 1. Copier le contenu de votre main.cpp existant
 * 2. Remplacer les #define de pins par les blocs conditionnels ci-dessus
 * 3. Remplacer les initialisations touch/LED par les fonctions abstraites
 * 4. Adapter les coordonnées UI selon SCREEN_WIDTH/SCREEN_HEIGHT
 * 5. Tester avec: pio run -e sunton-2432s028r
 *                 pio run -e lcdwiki-esp32s3-28p
 */
