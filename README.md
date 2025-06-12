> ⚠️ **Educational use only. Do not deploy without explicit permission.**

`esp-phisher` is a proof-of-concept tool built on the ESP32 platform that demonstrates how attackers can use fake Wi-Fi captive portals to phish credentials from unsuspecting users. The tool is designed to **educate users about the dangers of blindly trusting public login pages** and raise awareness around social engineering attacks over Wi-Fi.

---

## 📷 Overview

- Runs on ESP-WROOM-32 with a 128x32 OLED display
- Creates a fake Wi-Fi access point
- Serves a realistic captive portal mimicking login screens (Google, Facebook, Instagram, TikTok)
- Captures and logs submitted credentials to `log.txt`
- Includes a built-in admin panel with:
  - OLED SSID selection
  - Login viewer (web-based)
  - Clear logs
  - OLED menu for viewing connected devices and new login alerts
- Admin page at `192.168.4.1/?key=3000`

---

## 🛡️ Educational Purpose

> This project was created **to educate users about the dangers of blindly trusting login screens**, especially on public Wi-Fi networks. It demonstrates how easily a convincing phishing portal can be deployed on inexpensive hardware like the ESP32.

> The goal is to **raise awareness** of network-based social engineering risks and promote better cybersecurity hygiene — such as verifying captive portals, using VPNs, and enabling 2FA.

---

## ✅ Current Features

- Captive portal styled login page
- OLED-based SSID selector and menu navigation (buttons: Up, Down, Enter, Back)
- SD card logging (`log.txt`)
- EEPROM-stored SSID persistence
- Admin web UI to view/clear logs and update SSID
- Visual login alert on OLED when new data is captured
- Wi-Fi access point automatically starts with selected SSID
- **LED indicators for status**:
  - 🔴 **PWR (GPIO 12):** Off means no new logs. Solid means a new login was captured.
  - 🔵 **TX (GPIO 13):** Blinking means broadcasting ssid. Solid when a Client connected to the SSID.
  - 🟢 **RX (GPIO 14):** Blinking means a problem; see OLED for details. Solid means the system is operational.

---

## 🧪 Planned Features

- [ ] Wi-Fi passthrough support
- [ ] `sslstrip` integration to downgrade HTTPS to HTTP
- [ ] Full MITM (Man-in-the-Middle) capability for traffic inspection
- [ ] Browser certificate spoof warning emulation
- [ ] Cross-platform captive portal detection bypass

---

## 🛠️ Hardware Requirements

- ESP32 WROOM-32 board (e.g., AITRIP DevKit)
- 0.91" 128x32 I2C OLED (SSD1306)
- 4x push buttons (Up, Down, Enter, Back) — connected to GPIOs 32, 33, 25, 26
- MicroSD card module (CS on GPIO 5)
- **LEDs for status feedback:**
  - **Red (GPIO 12):**
  - **Blue (GPIO 13):**
  - **Green (GPIO 14):**

---

## ⚙️ Setup

1. Clone this repo and open the `.ino` sketch in Arduino IDE
2. Install dependencies:
   - `Adafruit_GFX`
   - `Adafruit_SSD1306`
   - `ESPAsyncWebServer`
   - `DNSServer`
   - `EEPROM`
   - `SD`
3. Connect:
   - OLED to I2C (GPIO 21 SDA, GPIO 22 SCL)
   - Buttons: GPIO 32 (Up), 33 (Down), 25 (Enter), 26 (Back)
   - LEDs: GPIO 12 (Red), 13 (Blue), 14 (Green)
   - SD card module: GPIO 5 (CS)
4. Upload via Arduino
5. On boot, device will start as a Wi-Fi access point

---

## 🔒 Legal Disclaimer

This software is for educational and research purposes only.  
**You are responsible for using this code in legal and ethical contexts.**

> Deploying this tool on networks without consent is **illegal** and violates the [Computer Fraud and Abuse Act](https://www.law.cornell.edu/uscode/text/18/1030).

---
## 📸 Demo Screenshots

### 🔧 Hardware Setup
<img src="demo/hardware.jpg" width="40%">

### 🌐 Captive Portal View
<img src="demo/captive.jpg" width="40%">

### 🛠️ Admin Interface
<img src="demo/admin.jpg" width="40%">

---

## Author

**SoCal IT**  
Ethical hacker & Wi-Fi tools developer  
Creator of [`alfa-wifi`](https://github.com/socal-it/alfa-wifi), and more.

---

## ⭐️ Support the Project

If this helped your research, education, or DEFCON demo, consider giving it a star 🌟
