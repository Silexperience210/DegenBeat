<div align="center">

<img src="https://img.shields.io/badge/Platform-ESP32-E7352C?style=for-the-badge&logo=espressif&logoColor=white"/>
<img src="https://img.shields.io/badge/Framework-Arduino-00979D?style=for-the-badge&logo=arduino&logoColor=white"/>
<img src="https://img.shields.io/badge/PlatformIO-Compatible-F5822A?style=for-the-badge&logo=platformio&logoColor=white"/>
<img src="https://img.shields.io/badge/Display-ST7789%20320×240-8A2BE2?style=for-the-badge"/>
<img src="https://img.shields.io/badge/Bitcoin-Lightning-F7931A?style=for-the-badge&logo=bitcoin&logoColor=white"/>
<img src="https://img.shields.io/badge/Release-v2.1.0-22C55E?style=for-the-badge"/>
<img src="https://img.shields.io/badge/License-MIT-FACC15?style=for-the-badge"/>

# ⚡ DegenBeat

### A standalone Bitcoin trading terminal built on ESP32.<br/>Touch screen · Lightning deposits · Live P&L RGB LED · LN Markets API v3.

</div>

---

## Table of Contents

- [Overview](#overview)
- [Hardware](#hardware)
- [Features](#features)
- [Screen Layout](#screen-layout)
- [Installation](#installation)
- [Configuration](#configuration)
- [Usage](#usage)
- [Adaptive RGB LED](#adaptive-rgb-led)
- [Technical Architecture](#technical-architecture)
- [RAM Budget](#ram-budget)
- [Troubleshooting](#troubleshooting)
- [Libraries](#libraries)
- [License](#license)

---

## Overview

**DegenBeat** turns a Sunton ESP32-2432S028R into a self-contained Bitcoin trading station. It connects directly to [LN Markets](https://lnmarkets.com) over WiFi, shows a live BTC price updated every 2 seconds, lets you open and close leveraged positions with a single tap, generate Lightning deposit QR codes on-device, and visualizes your aggregate P&L in real time through the built-in RGB LED — all without a phone or computer.

Everything runs on a single `main.cpp` file with FreeRTOS dual-core scheduling, non-blocking timers, a 6-layer API cache, and AES-256-CBC credential storage on SPIFFS.

---

## Hardware

| Component | Part | Notes |
|-----------|------|-------|
| MCU | **Sunton ESP32-2432S028R** | WROOM-32, 4MB flash, 520KB RAM, no PSRAM |
| Display | **ST7789 IPS 320×240** | Built-in, dedicated SPI bus |
| Touch | **XPT2046** | Separate SPI bus from TFT |
| RGB LED | **Common-cathode RGB** | Pins 4 (R) · 16 (G) · 17 (B) |
| WiFi | **802.11 b/g/n 2.4GHz** | WPA2 — captive portal built-in |

> **This firmware targets the Sunton ESP32-2432S028R exclusively.** Pin mapping, SPI bus assignments, and driver flags are hard-coded for this board. Other ESP32 boards require pin and driver changes.

### Pin Reference

```
TFT  — MOSI:13  MISO:12  SCLK:14  CS:15  DC:2   RST:-1  BL:21
Touch — MOSI:32  MISO:39  SCLK:25  CS:33  IRQ:36
LED  — R:4   G:16   B:17
```

---

## Features

### Trading
- Live BTC/USD price — refreshed every 2 s via a dedicated FreeRTOS task (Core 0)
- Market orders — SELL / BUY with 25×, 50×, 75×, or 100× leverage
- Quick amounts — 100, 500, 1 000, 5 000 sats presets + fine ±100 sats adjustment
- Close running positions — tap CLOSE per trade on the POS screen
- Cancel open orders — tap CANCEL per order on the POS screen
- Trade history — last 10 closed trades with side, leverage, P&L, and date

### Wallet & Lightning
- Live balance — sats balance + live USD equivalent
- Lightning withdrawal — configurable amount sent to your Lightning address
- Deposit QR code — generated **locally** on-device via the QRCode library (no web dependency)
- Deposit history — last 10 Lightning deposits with amount, date, comment, and payment hash

### Infrastructure
- **WiFi captive portal** — configure network + API credentials from any browser, no code needed
- **Encrypted credentials** — AES-256-CBC storage on SPIFFS, key derived from the ESP32 MAC address
- **6-layer API cache** — avoids redundant network calls between screen refreshes
- **Non-blocking WiFi watchdog** — auto-reconnect via timer callback, never freezes the UI
- **Adaptive RGB LED** — live P&L indicator with 7 distinct blink patterns (see below)

---

## Screen Layout

```
┌──────────────────────────────────────────────────────┐
│  HOME  │  WALLET  │   POS   │   HIST                 │  ← 4-zone tap bar
├──────────────────────────────────────────────────────┤

  HOME                           WALLET
  ┌────────────────────────┐     ┌────────────────────────┐
  │ BITCOIN PRICE          │     │ Balance: 24 500 sats    │
  │ $97 420     [SELL]     │     │ ≈ $23.80 USD            │
  │             [BUY ]     │     │ Margin in positions:    │
  │ LEVERAGE:              │     │ 12 000 sats             │
  │ [25x][50x][75x][100x]  │     │ Withdraw amount: 1 000s │
  │ QUICK: [100][500][1000]│     │ [100][500][1000][2000]  │
  │ MARGIN: 1000s  [-][+]  │     │ [-][+]                  │
  │ [======EXECUTE======]  │     │ [WITHDRAW][DEPOSIT]     │
  └────────────────────────┘     │ [HISTORY] [RESET]       │
                                 └────────────────────────┘

  POSITIONS                      HISTORY
  ┌────────────────────────┐     ┌────────────────────────┐
  │ RUNNING POSITIONS:     │     │ TRADE HISTORY          │
  │ buy 50x 2000s          │     │ buy 25x 9a3f2b1c...    │
  │ Entry:$95k  Now:$97k   │     │ +2.45 sats  2025-01-30 │
  │ +1042s  +52.1%         │     │ sell 100x 3b7e9d2a...  │
  │ [CLOSE]                │     │ -890 sats   2025-01-28 │
  │ OPEN ORDERS:           │     │ ...                    │
  │ buy limit 25x 1000s    │     └────────────────────────┘
  │ [CANCEL]               │
  └────────────────────────┘
```

---

## Installation

### Prerequisites

- [PlatformIO](https://platformio.org/install) (VSCode extension recommended)
- [Git](https://git-scm.com/)
- USB cable for your board

### Clone & Flash

```bash
git clone https://github.com/Silexperience210/DegenBeat.git
cd DegenBeat

# Build only
pio run

# Build + upload
pio run --target upload

# Serial monitor
pio device monitor --baud 115200
```

> The `platformio.ini` is pre-configured for the Sunton ESP32-2432S028R with the correct ST7789 driver flags, SPI pins, and DMA settings. Do not change `build_flags` unless you know your exact board variant.

---

## Configuration

### First Boot — Captive Portal

On first boot (or after a RESET), the terminal creates a WiFi access point:

```
SSID      :  LNMarkets-Setup
Password  :  (none)
Config URL:  http://192.168.4.1
```

Connect your phone or PC to this network. Your browser will open the configuration form automatically (captive portal). Fill in all fields and tap **Save** — the ESP32 reboots and connects.

### Configuration Fields

| Field | Description |
|-------|-------------|
| **WiFi SSID** | Your 2.4 GHz network name |
| **WiFi Password** | WiFi password |
| **API Key** | LN Markets API key — Settings → API |
| **API Secret** | LN Markets API secret |
| **API Passphrase** | LN Markets API passphrase (min. 8 chars) |
| **Lightning Address** | Your Lightning address for withdrawals (e.g. `you@wallet.com`) |
| **Deposit LN Address** | Lightning address shown as QR code for deposits |

### Getting LN Markets API Keys

1. Log in at [lnmarkets.com](https://lnmarkets.com)
2. Go to **Settings → API**
3. Create a new key pair with permissions: `Read`, `Trade`, `Withdraw`
4. Copy the Key, Secret, and Passphrase — they are shown only once

> **Security** — Credentials are stored AES-256-CBC encrypted on SPIFFS. The encryption key is derived from the unique ESP32 MAC address via PBKDF2. They are never transmitted in plain text.

### Factory Reset

From the **WALLET** screen, tap **RESET** (bottom-right corner). The device wipes the SPIFFS config file and reboots into captive portal mode.

---

## Usage

### Navigation

The top bar is divided into 4 tap zones (80 px each):

| Zone | Screen | Content |
|------|--------|---------|
| `HOME` | Main trading screen | Live BTC price, SELL/BUY, leverage, execute |
| `WALLET` | Wallet screen | Balance, withdraw, deposit QR, deposit history |
| `POS` | Positions screen | Running trades, open orders, close/cancel |
| `HIST` | History screen | Last 10 closed trades with P&L and date |

### Opening a Trade

1. Go to **HOME**
2. Tap **SELL** or **BUY** — the selected button highlights in color
3. Select leverage: `25x`, `50x`, `75x`, or `100x`
4. Choose a quick amount or fine-tune with `[−]` / `[+]`
5. Tap **EXECUTE TRADE**

The trade is submitted as a market order. On success, all caches are invalidated and the screen refreshes with your new position.

### Lightning Deposit

1. Go to **WALLET** → tap **DEPOSIT**
2. A QR code is generated locally from your configured `deposit_lnaddress`
3. Scan from any Lightning wallet to send sats

### Lightning Withdrawal

1. Go to **WALLET**
2. Set the withdrawal amount (quick amounts or `[−]` / `[+]`)
3. Tap **WITHDRAW** — the payment is sent to your configured `lightning_address`

---

## Adaptive RGB LED

The built-in RGB LED is a **real-time position health indicator**. It reads the aggregated P&L across all your active positions every loop cycle and adjusts its behavior continuously — no manual interaction needed.

### How P&L Is Computed

The LED does not look at individual positions. It computes a single **portfolio P&L percentage** across all open trades:

```
portfolio_pnl% = (sum of all pl_sats) / (sum of all margins) × 100
```

This means if you have three positions — one up 80%, one flat, one down 20% — the LED reflects the net result of your entire book.

### LED Behavior Map

| LED State | Color | Blink Period | P&L Condition |
|-----------|-------|-------------|---------------|
| **Off** | — | — | No open positions |
| **Cyan double-pulse** | Cyan | Heartbeat | AP config mode (setup) |
| **Slow blink** | Green / Red | 2 000 ms | \|P&L\| < 5% |
| **Normal blink** | Green / Red | 1 000 ms | \|P&L\| 5% – 25% |
| **Fast blink** | Green / Red | 500 ms | \|P&L\| 25% – 50% |
| **Rapid blink** | Green / Red | 250 ms | \|P&L\| 50% – 75% |
| **Frantic blink** | Green / Red | 125 ms | \|P&L\| 75% – 100% |
| **Solid Green** | Green | Continuous | P&L ≥ +100% — moon mode |
| **Solid Red** | Red | Continuous | P&L ≤ −100% — liquidation zone |

**Color meaning:**
- **Green** = portfolio P&L is positive (you are in profit)
- **Red** = portfolio P&L is negative (you are in loss)

The blink speed reflects **how extreme** the move is. A slow green blink is a small gain — a frantic red blink at 125 ms means you're approaching −75% on your aggregate margin and should pay attention. Solid red at −100% is your last warning before liquidation.

### Practical Examples

```
No positions open        →  LED off
1 trade, +3% P&L         →  slow GREEN blink  (2s period)
1 trade, −18% P&L        →  normal RED blink  (1s period)
2 trades, net +40% P&L   →  fast GREEN blink  (500ms period)
3 trades, net −60% P&L   →  rapid RED blink   (250ms period)
1 trade, +200% P&L       →  solid GREEN
1 trade, −110% P&L       →  solid RED  ← act fast
```

> The LED runs inside `loop()` on Core 1, updated every tick with zero blocking. It never interferes with screen rendering or API calls.

---

## Technical Architecture

### FreeRTOS Dual-Core Split

```
Core 0 (App CPU)           Core 1 (Pro CPU)
─────────────────────────  ─────────────────────────────────────
priceUpdateTask            loop()
  └─ fetchBTCPrice()         ├─ handleTouch()
  └─ displayPrice()          │    ├─ handleNavigation()
     (sprite push via DMA)   │    ├─ handleHomeTouch()
  (every 2 000 ms)           │    ├─ handleWalletTouch()
                             │    └─ handlePositionsTouch()
                             ├─ updateLedPnl()
                             ├─ message_timer.check()
                             └─ WiFi watchdog (non-blocking)
```

TFT access from both cores is protected by `tft_mutex` (FreeRTOS mutex). The price task takes the mutex for ≤5 ms to push the sprite; the UI task takes it for full screen redraws.

### Non-Blocking Timer System

All delays use the `NonBlockingDelay` struct — a callback-based timer that fires from `loop()`:

```cpp
message_timer.set(2000, []() {
    showing_message = false;
    switchScreen(current_screen);
});
```

There are zero `delay()` calls in the main execution path. Touch latency stays under 20 ms even during active HTTP requests.

### API Cache

| Cache | TTL | Endpoint |
|-------|-----|----------|
| `priceCache` | 2 s | `/v3/futures/ticker` |
| `walletCache` | 15 s | `/v3/account` |
| `positionsCache` | 10 s | `/v3/futures/isolated/trades/running` |
| `ordersCache` | 12 s | `/v3/futures/isolated/trades/open` |
| `historyCache` | 60 s | `/v3/futures/isolated/trades/closed` |
| `depositsCache` | 60 s | `/v3/account/deposits/lightning` |

`markValid()` stamps the cache without storing the JSON response in heap — the response is parsed immediately and the string is freed, saving 30–40 KB per refresh cycle.

### Request Authentication

Every API call is signed with HMAC-SHA256:

```
signature = HMAC-SHA256(api_secret, timestamp + method_lower + /v3/path + body)
signature_b64 = base64(signature)
```

Headers: `LNM-ACCESS-KEY`, `LNM-ACCESS-PASSPHRASE`, `LNM-ACCESS-TIMESTAMP`, `LNM-ACCESS-SIGNATURE`.

### Antiflicker Sprite Rendering

The BTC price area (172×35 px) uses `TFT_eSprite`:

```
CPU renders into sprite buffer  →  single DMA push to TFT
```

This replaces the previous `fillRect()` + `print()` sequence which caused a visible white flash at every price update. The sprite push is atomic from the display's perspective.

---

## RAM Budget

ESP32 WROOM-32: 520 KB total RAM. At rest after boot:

| Item | Before v2.1.0 | After v2.1.0 | Delta |
|------|--------------|-------------|-------|
| API cache (6×) | ~45 KB (JSON strings in heap) | ~1 KB (timestamps only) | **−44 KB** |
| Data arrays | ~3 KB (Arduino String heap) | ~2 KB (char[] in BSS) | **−1 KB + no fragmentation** |
| Price sprite buffer | 0 | 172×35×2 = ~12 KB | −12 KB (new allocation) |
| **Free heap at rest** | ~90 KB | **~130–150 KB** | **+40–60 KB** |

No PSRAM available on WROOM-32. The `char[][]` conversion eliminates heap fragmentation from repeated Arduino `String` allocations across screen refreshes.

---

## Troubleshooting

### Screen stays black after boot

- Check that pin 21 (`TFT_BL`) is set `OUTPUT HIGH` in `setup()`
- Verify SPI pins: MOSI=13, MISO=12, SCLK=14, CS=15, DC=2
- Confirm `platformio.ini` contains `-DST7789_DRIVER=1` and `-DTFT_INVERSION_OFF=1`

### Colors are wrong or inverted

- This board requires `-DTFT_RGB_ORDER=TFT_BGR`
- Do **not** use `ILI9341_DRIVER` — the Sunton 2432S028R uses ST7789
- Do **not** call `tft.invertDisplay()` — it is handled by the build flag

### Touch doesn't respond or is misaligned

- Touch rotation must match TFT rotation: `touch.setRotation(1)`
- Touch SPI uses dedicated pins: CLK=25, MISO=39, MOSI=32, CS=33, IRQ=36

### API returns 401 Unauthorized

- Regenerate API keys on lnmarkets.com (keys expire)
- Ensure passphrase is at least 8 characters
- Verify NTP time sync — the HMAC signature includes a timestamp

### "Erreur API" on all screens

- WiFi may have dropped — check for "WiFi perdu" on screen
- If `api_key == "public"`, only the BTC price displays; wallet/positions require real API keys

### SPIFFS FAIL on boot

- Re-format: `pio run --target uploadfs`, then re-flash firmware
- Some clone ESP32 boards ship with SPIFFS disabled; `SPIFFS.begin(true)` auto-formats on first call

### LED stays off with open positions

- Positions data comes from `positionsCache` — wait up to 10 s for the first refresh
- If balance shows 0 and positions are empty, check the API key permissions (`Trade` + `Read`)

---

## Libraries

| Library | Version | Purpose |
|---------|---------|---------|
| [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) | ^2.5.43 | TFT driver, sprites, DMA |
| [XPT2046_Touchscreen](https://github.com/PaulStoffregen/XPT2046_Touchscreen) | latest | Resistive touch input |
| [ArduinoJson](https://arduinojson.org/) | ^7.2.0 | JSON parsing (API responses) |
| [QRCode](https://github.com/ricmoo/qrcode) | ^0.0.1 | On-device QR code generation |
| [PNGdec](https://github.com/bitbank2/PNGdec) | ^1.1.6 | PNG image decoding |
| mbedTLS | ESP-IDF built-in | AES-256-CBC, HMAC-SHA256 |

---

## License

```
MIT License

Copyright (c) 2025 Silexperience210

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

<div align="center">

**Built for degens, by degens.**

*Leveraged Bitcoin trading carries significant risk of capital loss. This project is provided for educational purposes. Trade responsibly.*

[![GitHub Stars](https://img.shields.io/github/stars/Silexperience210/DegenBeat?style=social)](https://github.com/Silexperience210/DegenBeat)
[![GitHub Forks](https://img.shields.io/github/forks/Silexperience210/DegenBeat?style=social)](https://github.com/Silexperience210/DegenBeat/fork)
[![GitHub Issues](https://img.shields.io/github/issues/Silexperience210/DegenBeat?style=social)](https://github.com/Silexperience210/DegenBeat/issues)

</div>
