# esp-phisher v2.0.0


`esp-phisher` is a Wi-Fi phishing proof-of-concept built on an ESP32 that simulates a public captive portal to capture credentials. Version 2.0 introduces a modern Bootstrap UI, credit card capture mode, RTC time sync, improved OLED navigation, SD file serving, and a powerful admin dashboard.

GitHub: [github.com/socalit](https://github.com/socalit)

---

## Demo

<img src="https://raw.githubusercontent.com/socalit/esp-phisher/1.8.2/demo/demo1-1.JPG" width="40%">
<img src="https://raw.githubusercontent.com/socalit/esp-phisher/1.8.2/demo/demo1-5.JPG" width="40%">
<img src="https://raw.githubusercontent.com/socalit/esp-phisher/1.8.2/demo/demo2-3.JPG" width="40%">
<img src="https://github.com/socalit/esp-phisher/blob/2.0.0/demo_2.0/2.0demo1.JPG" width="40%">
<img src="https://github.com/socalit/esp-phisher/blob/2.0.0/demo_2.0/2.0demo2.JPG" width="40%">
<img src="https://github.com/socalit/esp-phisher/blob/2.0.0/demo_2.0/2.0settings.JPG" width="40%">
<img src="https://github.com/socalit/esp-phisher/blob/2.0.0/demo_2.0/captive2.0.JPG" width="40%">
<img src="https://github.com/socalit/esp-phisher/blob/2.0.0/demo_2.0/captive2.0cards.JPG" width="40%">

---

## Overview

- Runs on ESP-WROOM-32 with 128x32 OLED display
- Fake Wi-Fi AP with spoofed MAC and customizable SSID
- Captive portal login options:
  - Google, Facebook, Instagram, TikTok
  - Optional credit card form ($10 High-Speed Wi-Fi)
- Captures credentials + User-Agent with timestamp
- Admin panel at `192.168.4.1/admin.html?key=3000`
- Stores logs and content on SD card
- OLED menu for SSID, logs, reboot

---

## New in Version 2.0.0

### Captive Portal (`index.html`)
- Rewritten in Bootstrap 5.3.7
- Credit card form with:
  - Name, number, expiration, CVC, billing address
  - Auto card type detection
  - Input formatting + validation
- Terms & Conditions checkbox
- Dynamic background (`/bg` from SD card)

### Admin Dashboard (`admin.html`)
- Bootstrap + Chart.js UI
- Stats:
  - Total log count
  - Platform breakdown
  - Cards captured
- Charts:
  - Platform Split (bar)
  - Logins per Day (7-day line chart)
- SSID + Background selection
- RTC time sync (`/time`)
- Log viewer with:
  - Timestamp grouping
  - Pretty-printed credit card and platform entries
- Actions: Clear log, Mark as Read

### Logging & RTC
- Logs stored as `[YYYY-MM-DD HH:MM:SS] [Platform] ...`
- Captures full `User-Agent`
- Timestamp via DS3231 RTC module
- EEPROM used to track unread entries

---

## Hardware Requirements

| Component            | Connection Details                    |
|----------------------|---------------------------------------|
| ESP32 Dev Board      | ESP-WROOM-32                          |
| SD Card Module       | (CS: GPIO 5) VCC 5v                   |
| SSD1306 OLED (0.91") | I2C (SDA: GPIO 21, SCL: GPIO 22)      |
| RTC DS3231 Module    | I2C shared with OLED + VCC 3.3v       |
| Button – Up          | GPIO 32          (optional)           |
| Button – Down        | GPIO 33          (optional)           |
| Button – Enter       | GPIO 25          (optional)           |
| Button – Back        | GPIO 26          (optional)           |
| LED 1 (Logs)         | GPIO 12          (optional)           |
| LED 2 (Client)       | GPIO 13          (optional)           |
| LED 3 (System status)| GPIO 14          (optional)           |

> SDCard Module uses SPI (MISO: GPIO 19, MOSI: GPIO 23, SCK: GPIO 18)

---

## SD Card File Structure
/index.html
/admin.html
/success.html
/terms.html
/log.txt
/bootstrap-5.3.7/css/bootstrap.min.css
/bootstrap-5.3.7/js/bootstrap.bundle.min.js
/chart.umd.min.js
/img/bg.jpg <-- selected background
/img/bg1.jpg … bg5.jpg <-- optional images
/img/google.png
/img/facebook.png
/img/instagram.png
/img/tiktok.png
/img/feather/*.svg

---

## OLED Menu System

- Navigated via 4 physical buttons
- Menu options:
  - View log entries
  - Clear logs
  - Reboot device
  - Change SSID
- OLED home screen:
  - SSID name
  - Client count
  - New login indicator

---

## Captive Portal Flow

- Starts AP with spoofed UniFi-style MAC
- Captures all DNS and redirects to `index.html`
- Accepts POSTs to `/submit`
- Saves formatted logs to `log.txt`
- Redirects to `index.html` with a wrong username or password error

---

## Admin Access

Visit: http://192.168.4.1/admin.html?key=3000


Features:
- Log viewing + platform parsing
- SSID + background update
- Set current time (RTC)
- Charts for usage analytics
- Clear or mark logs as read

---

## Setup Instructions

1. Wire ESP32 + SD + OLED + Buttons as listed
2. Format SD card (FAT32) and copy the required files in the data folder
3. Open Arduino IDE and install libraries:
   - Adafruit_SSD1306
   - Adafruit_GFX
   - SD
   - EEPROM
   - DNSServer
   - ESPAsyncWebServer
4. Upload `esp-phisher` sketch
5. Join the ESP32 Wi-Fi and test via browser

---

## Educational Purpose

This project educates users about:
- Phishing threats over public Wi-Fi
- Dangers of unverified captive portals
- The importance of MFA and VPN usage

---

## Legal Disclaimer

This software is for educational and research use only.  
Using this tool on unauthorized networks is illegal and unethical.

You are solely responsible for how you use this code.

---

## Author

**SoCal IT**  
Ethical hacker & Wi-Fi tools developer  
GitHub: [https://github.com/socalit](https://github.com/socalit)

---

⭐ Star the project if you found it useful for red teaming, research, or CTF demos.

---

## Credits

- **Feather Icons**  
  Simple and elegant SVG icons used in the admin panel  
  https://feathericons.com

- **Chart.js**  
  JavaScript charting library used for platform/daily analytics  
  https://www.chartjs.org/

- **Bootstrap 5.3.7**  
  Responsive front-end framework used in `index.html` and `admin.html`  
  https://getbootstrap.com

All third-party resources are included under their respective open source licenses.
