// User_Setup.h for LCDWiki ESP32-S3 2.8inch Display
// ILI9341 240x320 display

#define USER_SETUP_INFO "LCDWiki ESP32-S3 2.8inch ILI9341"

// Driver
#define ILI9341_DRIVER

// Display dimensions (portrait)
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// SPI pins for ESP32-S3
#define TFT_MISO 13
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_CS   10
#define TFT_DC   46
#define TFT_RST  -1  // Reset is shared with ESP32-S3

// Backlight
#define TFT_BL   45
#define TFT_BACKLIGHT_ON HIGH

// SPI frequency
#define SPI_FREQUENCY  80000000
#define SPI_READ_FREQUENCY  40000000
#define SPI_TOUCH_FREQUENCY  2500000

// DMA (optional but recommended)
#define USE_DMA_TO_TFT

// Fonts
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// No touch in TFT_eSPI (we use FT6336U separately)
#define SUPPORT_TRANSACTIONS
