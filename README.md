# esp-phisher v1.8.2

**Educational use only. Do not deploy without explicit permission.**

`esp-phisher` is a proof-of-concept tool built on the ESP32 platform that demonstrates how attackers can use fake Wi-Fi captive portals to phish credentials from unsuspecting users. The tool is designed to educate users about the dangers of blindly trusting public login pages and raise awareness around social engineering attacks over Wi-Fi.

GitHub: [github.com/socalit](https://github.com/socalit)

---
## 📸 Demo

<img src="https://raw.githubusercontent.com/socalit/esp-phisher/1.8.2/demo/demo1-1.JPG" width="40%">
<img src="https://raw.githubusercontent.com/socalit/esp-phisher/1.8.2/demo/demo1-5.JPG" width="40%">
<img src="https://raw.githubusercontent.com/socalit/esp-phisher/1.8.2/demo/demo2-3.JPG" width="40%">


## Overview

* Runs on ESP-WROOM-32 with a 128x32 OLED display
* Creates a fake Wi-Fi access point with customizable SSID
* Serves a realistic captive portal mimicking login screens (Google, Facebook, Instagram, TikTok)
* Captures and logs submitted credentials to `log.txt` on SD card
* Admin panel at `192.168.4.1/?key=3000` for managing logs and settings
* OLED-based menu system with:

  * SSID selection
  * View Logins
  * Clear Logs
  * Reboot
* LED indicators for system status and activity

---

## Version: v1.8.2 (June 2025)

### New Features

* OLED boot splash showing version + GitHub (`esp-phisher v1.8.2`, `github.com/socalit`)
* Log viewer now scrolls with Up/Down buttons if over 3 entries
* Samsung and Android captive portal triggers improved (`/generate_204`, `/connecttest.txt`, etc.)
* SD card debug: required file check with OLED + Serial error output
* Auto MAC spoofing as UniFi AP (OUI `28:70:4E`)
* EEPROM-stored SSID persistence
* Background image selection via admin panel (bg1–bg4)

### New Web UI Features

#### `index.html` (Captive Portal)

* Responsive, mobile-friendly layout
* Background image fetched dynamically via `/bg`
* Platform selection with icons (Google, Facebook, Instagram, TikTok)
* Dynamic form placeholder (email or username)
* Terms and Conditions checkbox (required to continue)
* Auto-restore of platform and terms selection via `localStorage`
* Error display for incorrect input or invalid email (Google only)

#### `admin.html` (Admin Panel)

* Monospaced log display with scrollable `<pre>` block
* Buttons to clear logs and mark them as read
* Admin key protection for clearing logs
* Background image selector
* SSID updater (with reboot)
* AJAX requests for smoother UX

### Bug Fixes

* OLED initialization order corrected (prevents crash on boot)
* Log size EEPROM sync fixed
* Captive portal reliably triggers on more Android/Samsung devices
* Improved error handling when files are missing on SD card

---

## Current Features

* Captive portal styled login page (HTML + background)
* OLED-based SSID selector and menu navigation (buttons: Up, Down, Enter, Back)
* SD card logging to `log.txt`
* Admin web UI to:

  * View/clear logs
  * Update SSID
  * Mark log as read
  * Change background image
* OLED home screen shows:

  * SSID name
  * Connected client count
  * New login alert
* LED indicators:

  * **TX (Blue): GPIO 12** – New login captured (solid)
  * **RX (Red): GPIO 13** – Client / SSID (blinks when broadcasting solid on client connect)
  * **PWR (Green): GPIO 14** – System status (blinks/missing file warning)

---

## Required Files (Place on SD card)

```
/index.html
/admin.html
/success.html
/terms.html
/log.txt (optional)
/img/facebook.png
/img/google.png
/img/instagram.png
/img/tiktok.png
/img/poweredByUniFi.svg
/img/bg1.jpg
/img/bg2.jpg
/img/bg3.jpg
/img/bg4.jpg
```

---

## Hardware Requirements

* **ESP32 WROOM-32** (AITRIP DevKit or equivalent)
* **OLED:** 0.91" 128x32 I2C SSD1306 (GPIO 21 SDA, 22 SCL)
* **Buttons:**

  * Up: GPIO 32
  * Down: GPIO 33
  * Enter: GPIO 25
  * Back: GPIO 26
* **LEDs:**

  * TX (Red): GPIO 12
  * RX (Blue): GPIO 13
  * PWR (Green): GPIO 14
* **SD Card Module:** CS on GPIO 5

---

## Setup Instructions

1. Clone the repo and open the `.ino` sketch in Arduino IDE
2. Install dependencies:

   * Adafruit\_GFX
   * Adafruit\_SSD1306
   * ESPAsyncWebServer
   * DNSServer
   * EEPROM
   * SD
3. Wire the components as listed
4. Format an SD card as FAT32 and copy the required files
5. Upload the sketch to the ESP32
6. Connect to the generated Wi-Fi and test

---

## Educational Purpose

This project was created to educate users about the risks of trusting captive portals without verification. It raises awareness of phishing tactics over open Wi-Fi, and promotes safe practices such as:

* Using VPNs on public networks
* Verifying captive portal URLs
* Enabling multi-factor authentication

---

### 🧪 Future Feature Roadmap *(If Project Gains Popularity)*

The following features are planned **only if `esp-phisher` receives significant community interest and GitHub stars**:

- **Wi-Fi Passthrough Mode**  
- **QR Code Support**  
- **Offline Log Export**  
- **OTA Updates via Admin Panel**  
- **Session Timer**  

---

## Legal Disclaimer

This software is for educational and research purposes only.
**Do not use on unauthorized networks.**

Deploying this tool on any network without clear, written permission is illegal and may violate the Computer Fraud and Abuse Act and similar laws.

You are solely responsible for how you use this code.

---

## Author

**SoCal IT**  
Ethical hacker & Wi-Fi tools developer  
Creator of [`alfa-wifi`](https://github.com/socalit/alfa-wifi), and more.

---

##  Support the Project

If this helped your research, education, or DEFCON demo, consider giving it a star 🌟
