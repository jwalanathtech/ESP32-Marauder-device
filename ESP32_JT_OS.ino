/*
  JT OS - ESP32 Wireless Diagnostic & Games Device
  Main sketch

  Fixed wiring:
  TFT: SCK=17, MOSI=4, CS=25, DC=26, RST=27
  Joystick: X=35, Y=34, SW=32
  nRF24 #1 (HSPI): SCK=14, MISO=12, MOSI=13, CSN=15, CE=16
  nRF24 #2 (VSPI): SCK=18, MISO=19, MOSI=23, CSN=21, CE=22
  Buttons: BACK=33, ROTATE=5

  Libraries:
    Adafruit GFX
    Adafruit ST7735
    RF24 by TMRh20
    ESP32 Arduino core
*/

// Declared before the Arduino preprocessor-generated function prototypes.
// This prevents ArduinoDroid from generating a prototype for readJoyDirection()
// before the JoyDir type is known.
struct JoyDir {
  signed char x; // -1 left, 0 center, +1 right
  signed char y; // -1 up,   0 center, +1 down
};

#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <RF24.h>
#include "JT_Logo.h"
#include "soc/soc.h"
#include "soc/gpio_reg.h"

// Custom light-grey color (RGB565) for Adafruit GFX versions
// that do not define JT_LIGHTGREY.
#define JT_LIGHTGREY 0xC618
#define JT_DARKBLUE   0x18C3
#define JT_TEAL       0x04FF
#define JT_PURPLE     0x780F
#define JT_ORANGE     0xFD20
#define JT_PINK       0xF81F
#define JT_LIME       0x87E0
#define JT_SKY        0x5D9F
#define JT_GOLD       0xFEA0
#define JT_PANEL      0x10A2

// -------------------- TFT --------------------
#define JT_TFT_HW_SPI 0

#if JT_TFT_HW_SPI
  #define TFT_SCLK 18
  #define TFT_MOSI 23
#else
  #define TFT_SCLK 17
  #define TFT_MOSI 4
#endif
#define TFT_CS   25
#define TFT_DC   26
#define TFT_RST  27

// User requested default orientation.
#define DEFAULT_ROTATION 2

#if JT_TFT_HW_SPI
SPIClass vspi(VSPI);   // must exist before tft is constructed
Adafruit_ST7735 tft(&vspi, TFT_CS, TFT_DC, TFT_RST);
#else
Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
#endif

// -------------------- Joystick --------------------
#define JOY_X  35
#define JOY_Y  34
#define JOY_SW 32

// -------------------- Buttons --------------------
#define BACK_BUTTON   33
#define ROTATE_BUTTON 5

// -------------------- nRF24 #1 - HSPI --------------------
#define NRF1_SCK  14
#define NRF1_MISO 12
#define NRF1_MOSI 13
#define NRF1_CSN  15
#define NRF1_CE   16

// -------------------- nRF24 #2 - VSPI --------------------
#define NRF2_SCK  18
#define NRF2_MISO 19
#define NRF2_MOSI 23
#define NRF2_CSN  21
#define NRF2_CE   22

SPIClass hspi(HSPI);
#if !JT_TFT_HW_SPI
SPIClass vspi(VSPI);
#endif

RF24 radio3(NRF1_CE, NRF1_CSN);
RF24 radio2(NRF2_CE, NRF2_CSN);

Preferences prefs;

// -------------------- UI state --------------------
uint8_t screenRotation = DEFAULT_ROTATION;
int menuIndex = 0;
bool radio1OK = false;
bool radio2OK = false;

// All items by id (ids are also used for icons / colours).
const int ITEM_TOTAL = 16;
const char* mainMenu[ITEM_TOTAL] = {
  "WiFi Scanner",
  "WiFi Details",
  "Games",
  "nRF24 Test",
  "nRF Settings",
  "Joystick",
  "Hardware",
  "Software Info",
  "JT 1",
  "JT 2",
  "JT 3",
  "JT 5",
  "JT 6",
  "JT 7",
  "JT 8"
};

const int MAIN_MENU_COUNT = 12;
const uint8_t mainMenuIds[MAIN_MENU_COUNT] = { 0, 1, 3, 5, 8, 9, 10, 11, 12, 13, 14, 15 };

// Games menu items
const int GAMES_COUNT = 16;
const uint8_t gamesMenuIds[GAMES_COUNT] = { 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115 };
const char* gamesMenuNames[GAMES_COUNT] = { "Block Fall", "Neon Snake", "Star Raid", "Breaker", "2048", "Road Racer", "Sky Hopper", "Invaders", "Tank Battle", "Sky Hunter", "Turbo Road", "Pong Duel", "Frog Hop", "Maze Muncher", "Memory Match", "Cloud Jump" };

// Settings menu items
const int SETTINGS_COUNT = 3;
const uint8_t settingsIds[SETTINGS_COUNT] = { 4, 6, 7 };   // nRF Settings, Hardware, Software Info

// Active menu pointer and title
const uint8_t* menuIds = mainMenuIds;
int MENU_COUNT = MAIN_MENU_COUNT;
const char* menuHeading = nullptr;

// -------------------- Button debounce --------------------
struct DebouncedButton {
  uint8_t pin;
  bool stableState;
  bool lastRaw;
  unsigned long changedAt;
};

DebouncedButton backBtn   = {BACK_BUTTON, HIGH, HIGH, 0};
DebouncedButton rotateBtn = {ROTATE_BUTTON, HIGH, HIGH, 0};
DebouncedButton joyBtn    = {JOY_SW, HIGH, HIGH, 0};

bool buttonPressed(DebouncedButton &b) {
  bool raw = digitalRead(b.pin);
  if (raw != b.lastRaw) {
    b.lastRaw = raw;
    b.changedAt = millis();
  }
  if ((millis() - b.changedAt) > 30 && raw != b.stableState) {
    b.stableState = raw;
    if (b.stableState == LOW) return true;
  }
  return false;
}

bool backPressed()   { return buttonPressed(backBtn); }
bool rotatePressed() { return buttonPressed(rotateBtn); }
bool joyPressed()    { return buttonPressed(joyBtn); }

// -------------------- Joystick --------------------
int joyCenterX = 2048;
int joyCenterY = 2048;
int joyStateX = 0;
int joyStateY = 0;

void calibrateJoystick() {
  long sx = 0;
  long sy = 0;
  for (int i = 0; i < 32; i++) {
    sx += analogRead(JOY_X);
    sy += analogRead(JOY_Y);
    delay(2);
  }
  joyCenterX = sx / 32;
  joyCenterY = sy / 32;
}

int joystickAxisState(int raw, int center, int previous, bool invert) {
  int v = raw - center;
  if (invert) v = -v;

  const int ENTER = 420;
  const int EXIT  = 250;

  if (previous == 0) {
    if (v > ENTER) return 1;
    if (v < -ENTER) return -1;
    return 0;
  }

  if (previous > 0) {
    if (v < -ENTER) return -1;
    if (v < EXIT) return 0;
    return 1;
  }

  if (v > ENTER) return 1;
  if (v > -EXIT) return 0;
  return -1;
}

JoyDir readJoyDirection() {
  int rawX = analogRead(JOY_X);
  int rawY = analogRead(JOY_Y);

  joyStateX = joystickAxisState(rawX, joyCenterX, joyStateX, false);
  joyStateY = joystickAxisState(rawY, joyCenterY, joyStateY, true);

  JoyDir d;
  switch (screenRotation & 3) {
    case 0:
      d.x =  joyStateY;
      d.y =  joyStateX;
      break;
    case 1:
      d.x =  joyStateX;
      d.y = -joyStateY;
      break;
    case 2:
      d.x = -joyStateY;
      d.y = -joyStateX;
      break;
    case 3:
    default:
      d.x = -joyStateX;
      d.y =  joyStateY;
      break;
  }
  return d;
}

// -------------------- Fast solid fill --------------------
#ifndef JT_FASTFILL_NOPS
#define JT_FASTFILL_NOPS 8
#endif

static inline void jtDelayNops() {
  for (int i = 0; i < JT_FASTFILL_NOPS; i++) __asm__ __volatile__("nop");
}

void fastFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
#if JT_TFT_HW_SPI
  tft.fillRect(x, y, w, h, color);
  return;
#endif
  if (w <= 0 || h <= 0 || x < 0 || y < 0 || x + w > tft.width() || y + h > tft.height()) {
    tft.fillRect(x, y, w, h, color);
    return;
  }

  const uint32_t clk = (1UL << TFT_SCLK);
  const uint32_t mo  = (1UL << TFT_MOSI);
  uint32_t on[16], off[16];
  for (int b = 0; b < 16; b++) {
    bool bit = (color >> (15 - b)) & 1;
    on[b]  = bit ? mo : 0;
    off[b] = (bit ? 0 : mo) | clk;
  }

  tft.startWrite();
  tft.setAddrWindow(x, y, w, h);

  uint32_t pixels = (uint32_t)w * (uint32_t)h;
  while (pixels--) {
    for (int b = 0; b < 16; b++) {
      REG_WRITE(GPIO_OUT_W1TC_REG, off[b]);
      REG_WRITE(GPIO_OUT_W1TS_REG, on[b]);
      REG_WRITE(GPIO_OUT_W1TS_REG, clk);
      jtDelayNops();
    }
  }
  REG_WRITE(GPIO_OUT_W1TC_REG, clk);
  tft.endWrite();
}

void jtFillScreen(uint16_t color) {
  fastFillRect(0, 0, tft.width(), tft.height(), color);
}

// -------------------- Rotation --------------------
void applyRotation() {
  tft.setRotation(screenRotation);
}

void changeRotation() {
  screenRotation = (screenRotation + 1) % 4;
  prefs.putUChar("rotation", screenRotation);
  applyRotation();

  jtFillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(2);
  tft.setCursor(8, 8);
  tft.print("JT OS");
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(8, 38);
  tft.print("Rotation: ");
  tft.print(screenRotation);
  tft.setCursor(8, 55);
  tft.print("Joystick: screen-relative");
  delay(250);
}

// -------------------- Common UI --------------------
void header(const char* title) {
  fastFillRect(0, 20, tft.width(), tft.height() - 20, ST77XX_BLACK);
  fastFillRect(0, 0, tft.width(), 20, ST77XX_BLUE);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(4, 6);
  tft.print(title);
}

void footer(const char* text) {
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(3, tft.height() - 10);
  tft.print(text);
}

void footer() {
  footer("SW/BACK: return");
}

void printPad(int x, int y, const String &s, uint16_t fg, int chars) {
  String t = s;
  while ((int)t.length() < chars) t += ' ';
  if ((int)t.length() > chars) t = t.substring(0, chars);
  tft.setTextSize(1);
  tft.setTextColor(fg, ST77XX_BLACK);
  tft.setCursor(x, y);
  tft.print(t);
}

int colsFrom(int x) {
  return (tft.width() - x) / 6;
}

void waitBack() {
  while (true) {
    if (backPressed() || joyPressed()) return;
    if (rotatePressed()) {
      changeRotation();
    }
    delay(10);
  }
}

bool handleGlobalButtons() {
  if (rotatePressed()) {
    changeRotation();
    return true;
  }
  return false;
}

// -------------------- WiFi --------------------
String securityName(wifi_auth_mode_t auth) {
  switch (auth) {
    case WIFI_AUTH_OPEN: return "OPEN";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
    case WIFI_AUTH_WPA3_PSK: return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
    default: return "OTHER";
  }
}

void wifiScanner() {
  header("WiFi Scanner");
  printPad(4, 28, "Scanning...", ST77XX_YELLOW, 18);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  delay(100);
  int n = WiFi.scanNetworks(false, true);
  if (n < 0) n = 0;

  int start = 0;
  bool fullRedraw = true;
  bool redraw = true;
  unsigned long lastMove = 0;

  while (true) {
    if (backPressed()) return;
    if (rotatePressed()) {
      changeRotation();
      fullRedraw = true;
      redraw = true;
    }

    int rows = (tft.height() - 21 - 14) / 16;
    if (rows < 1) rows = 1;
    int maxStart = max(0, n - rows);
    start = constrain(start, 0, maxStart);

    JoyDir d = readJoyDirection();
    if (d.y != 0 && (millis() - lastMove) > 160) {
      lastMove = millis();
      start += d.y;
      if (start < 0) start = maxStart;
      if (start > maxStart) start = 0;
      redraw = true;
    }

    if (fullRedraw) {
      jtFillScreen(ST77XX_BLACK);
      footer("UP/DN scroll  BACK");
      fullRedraw = false;
    }

    if (redraw) {
      String title = String("WiFi: ") + n + " networks";
      printPad(3, 3, title, ST77XX_CYAN, colsFrom(3));

      for (int i = 0; i < rows; i++) {
        int idx = start + i;
        int y = 21 + i * 16;
        if (idx < n) {
          String ssid = WiFi.SSID(idx);
          if (ssid.length() == 0) ssid = "<hidden>";
          uint16_t col = (i == 0) ? ST77XX_WHITE : ST77XX_GREEN;
          printPad(2, y, ssid, col, 14);
          String info = String(WiFi.RSSI(idx)) + " " + WiFi.channel(idx);
          printPad(90, y, info, col, colsFrom(90));
        } else {
          printPad(2, y, "", ST77XX_BLACK, colsFrom(2));
        }
      }
      redraw = false;
    }
    delay(10);
  }
}

void wifiDetails() {
  header("WiFi Details");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  delay(100);
  int n = WiFi.scanNetworks(false, true);

  if (n <= 0) {
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(5, 35);
    tft.print("No networks found");
    footer();
    waitBack();
    return;
  }

  int selected = 0;
  bool fullRedraw = true;
  bool redraw = true;

  while (true) {
    if (backPressed()) return;

    if (rotatePressed()) {
      changeRotation();
      fullRedraw = true;
      redraw = true;
    }

    JoyDir d = readJoyDirection();
    if (d.y != 0) {
      selected += d.y;
      if (selected < 0) selected = n - 1;
      if (selected >= n) selected = 0;
      redraw = true;
      delay(150);
    }

    if (fullRedraw) {
      header("WiFi Details");
      footer("UP/DN next  BACK exit");
      fullRedraw = false;
    }

    if (redraw) {
      int c = colsFrom(4);

      String cnt = String(selected + 1) + "/" + n;
      tft.setTextSize(1);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLUE);
      tft.setCursor(tft.width() - 6 * 7 - 4, 6);
      while (cnt.length() < 7) cnt = " " + cnt;
      tft.print(cnt);

      String ssid = WiFi.SSID(selected);
      if (ssid.length() == 0) ssid = "<hidden>";

      printPad(4, 25,  String("SSID: ") + ssid, ST77XX_WHITE, c);
      printPad(4, 40,  String("RSSI: ") + WiFi.RSSI(selected) + " dBm", ST77XX_WHITE, c);
      printPad(4, 55,  String("CH: ") + WiFi.channel(selected), ST77XX_WHITE, c);
      printPad(4, 70,  String("SEC: ") + securityName(WiFi.encryptionType(selected)), ST77XX_WHITE, c);
      printPad(4, 85,  "BSSID:", ST77XX_WHITE, c);
      printPad(4, 100, WiFi.BSSIDstr(selected), ST77XX_WHITE, c);
      redraw = false;
    }

    delay(8);
  }
}

// -------------------- nRF24 --------------------
const byte LINK_ADDR[6] = "JT24A";

void setupRadioCommon(RF24 &r) {
  r.setAutoAck(true);
  r.setRetries(5, 15);
  r.setPALevel(RF24_PA_LOW);
  r.setDataRate(RF24_1MBPS);
  r.setChannel(76);
  r.setCRCLength(RF24_CRC_16);
}

void initRadios() {
  hspi.begin(NRF1_SCK, NRF1_MISO, NRF1_MOSI, NRF1_CSN);
  vspi.begin(NRF2_SCK, NRF2_MISO, NRF2_MOSI, NRF2_CSN);

  radio1OK = radio3.begin(&hspi);
  radio2OK = radio2.begin(&vspi);

  if (radio1OK) {
    setupRadioCommon(radio3);
    radio3.openWritingPipe(LINK_ADDR);
    radio3.openReadingPipe(1, LINK_ADDR);
    radio3.stopListening();
  }

  if (radio2OK) {
    setupRadioCommon(radio2);
    radio2.openWritingPipe(LINK_ADDR);
    radio2.openReadingPipe(1, LINK_ADDR);
    radio2.startListening();
  }
}

uint8_t nrfRawRead(SPIClass &bus, int csn, uint8_t reg, uint32_t hz) {
  bus.beginTransaction(SPISettings(hz, MSBFIRST, SPI_MODE0));
  digitalWrite(csn, LOW);
  delayMicroseconds(10);
  bus.transfer(reg & 0x1F);
  uint8_t v = bus.transfer(0xFF);
  digitalWrite(csn, HIGH);
  bus.endTransaction();
  return v;
}

String hex2(uint8_t v) {
  const char* h = "0123456789ABCDEF";
  String r;
  r += h[v >> 4];
  r += h[v & 15];
  return r;
}

const char* nrfVerdict(uint8_t aw) {
  if (aw >= 1 && aw <= 3) return "chip responds";
  if (aw == 0xFF) return "no reply (FF)";
  if (aw == 0x00) return "stuck low (00)";
  return "bad data";
}

void nrfDiagBlock(int y, const char* name, SPIClass &bus, int csn, int ce, int c) {
  uint8_t aw1 = nrfRawRead(bus, csn, 0x03, 1000000);
  uint8_t aw8 = nrfRawRead(bus, csn, 0x03, 8000000);
  uint8_t cfg = nrfRawRead(bus, csn, 0x00, 1000000);
  uint8_t sta = nrfRawRead(bus, csn, 0x07, 1000000);
  printPad(4, y,      String(name) + " CS" + csn + " CE" + ce, ST77XX_CYAN, c);
  printPad(4, y + 10, String("AW 1M:") + hex2(aw1) + " 8M:" + hex2(aw8), ST77XX_WHITE, c);
  printPad(4, y + 20, String("CFG:") + hex2(cfg) + " STA:" + hex2(sta), ST77XX_WHITE, c);
  bool ok = (aw1 >= 1 && aw1 <= 3);
  printPad(4, y + 30, nrfVerdict(aw1), ok ? ST77XX_GREEN : ST77XX_RED, c);
}

bool nrfDiagScreen() {
  header("nRF24 Diagnostic");
  footer("SW retry  BACK exit");

  pinMode(NRF1_CSN, OUTPUT); digitalWrite(NRF1_CSN, HIGH);
  pinMode(NRF2_CSN, OUTPUT); digitalWrite(NRF2_CSN, HIGH);
  pinMode(NRF1_CE, OUTPUT);  digitalWrite(NRF1_CE, LOW);
  pinMode(NRF2_CE, OUTPUT);  digitalWrite(NRF2_CE, LOW);

  bool fullRedraw = false;
  unsigned long last = 0;

  while (true) {
    if (backPressed()) return false;
    if (rotatePressed()) {
      changeRotation();
      fullRedraw = true;
      last = 0;
    }
    if (joyPressed()) {
      printPad(4, 112, "Retrying...", ST77XX_YELLOW, colsFrom(4));
      initRadios();
      if (radio1OK && radio2OK) return true;
      printPad(4, 112, "Still failing", ST77XX_RED, colsFrom(4));
      delay(500);
      last = 0;
    }
    if (fullRedraw) {
      header("nRF24 Diagnostic");
      footer("SW retry  BACK exit");
      fullRedraw = false;
    }
    if (last == 0 || millis() - last >= 700) {
      last = millis();
      int c = colsFrom(4);
      nrfDiagBlock(24, "R3 HSPI", hspi, NRF1_CSN, NRF1_CE, c);
      nrfDiagBlock(66, "R2 VSPI", vspi, NRF2_CSN, NRF2_CE, c);
      printPad(4, 112, "", ST77XX_BLACK, c);
    }
    delay(10);
  }
}

void nrfTest() {
  header("nRF24 Link Test");
  if (!radio1OK || !radio2OK) {
    if (!nrfDiagScreen()) return;
    header("nRF24 Link Test");
  }

  footer("SW pause  BACK exit");

  uint32_t tx = 0, rx = 0, fail = 0;
  uint8_t payload[16];
  bool paused = false;
  bool fullRedraw = false;
  unsigned long lastDraw = 0;

  while (true) {
    if (backPressed()) return;
    if (rotatePressed()) {
      changeRotation();
      fullRedraw = true;
      lastDraw = 0;
    }

    if (!paused) {
      radio2.startListening();
      for (int i = 0; i < 16; i++) payload[i] = (uint8_t)(millis() + i);

      radio3.stopListening();
      bool ok = radio3.write(&payload, sizeof(payload));
      tx++;
      if (!ok) fail++;

      delay(5);
      if (radio2.available()) {
        uint8_t buf[16];
        while (radio2.available()) radio2.read(&buf, sizeof(buf));
        rx++;
      }
    }

    if (joyPressed()) {
      paused = !paused;
      delay(120);
      lastDraw = 0;
    }

    if (fullRedraw) {
      header("nRF24 Link Test");
      footer("SW pause  BACK exit");
      fullRedraw = false;
    }

    if (lastDraw == 0 || millis() - lastDraw >= 120) {
      lastDraw = millis();
      int c = colsFrom(5);
      printPad(5, 28,  String("Radio3 TX: ") + tx,   ST77XX_WHITE, c);
      printPad(5, 43,  String("Radio2 RX: ") + rx,   ST77XX_WHITE, c);
      printPad(5, 58,  String("TX fail:   ") + fail, ST77XX_WHITE, c);
      printPad(5, 73,  "Channel:   76",               ST77XX_WHITE, c);
      printPad(5, 88,  "Rate:      1Mbps",            ST77XX_WHITE, c);
      printPad(5, 103, "PA:        LOW",              ST77XX_WHITE, c);
      printPad(5, 120, paused ? "PAUSED" : "",        ST77XX_YELLOW, c);
    }
    delay(2);
  }
}

void nrfSettings() {
  int channel = 76;
  int rate = 1;
  int pa = 1;
  int item = 0;
  bool fullRedraw = true;
  bool redraw = true;

  while (true) {
    if (backPressed()) return;
    if (rotatePressed()) {
      changeRotation();
      fullRedraw = true;
      redraw = true;
    }

    JoyDir d = readJoyDirection();

    if (d.y != 0) {
      item += d.y;
      if (item < 0) item = 2;
      if (item > 2) item = 0;
      redraw = true;
      delay(150);
    }

    if (d.x != 0) {
      if (item == 0) {
        channel += d.x;
        channel = constrain(channel, 0, 125);
      } else if (item == 1) {
        rate += d.x;
        if (rate < 0) rate = 2;
        if (rate > 2) rate = 0;
      } else {
        pa += d.x;
        if (pa < 0) pa = 3;
        if (pa > 3) pa = 0;
      }
      redraw = true;
      delay(120);
    }

    if (joyPressed()) {
      if (radio1OK) {
        radio3.setChannel(channel);
        radio3.setPALevel((rf24_pa_dbm_e)pa);
        radio3.setDataRate(rate == 0 ? RF24_250KBPS : rate == 1 ? RF24_1MBPS : RF24_2MBPS);
      }
      if (radio2OK) {
        radio2.setChannel(channel);
        radio2.setPALevel((rf24_pa_dbm_e)pa);
        radio2.setDataRate(rate == 0 ? RF24_250KBPS : rate == 1 ? RF24_1MBPS : RF24_2MBPS);
      }
      printPad(8, 90, "Applied", ST77XX_GREEN, colsFrom(8));
      delay(400);
      printPad(8, 90, "", ST77XX_GREEN, colsFrom(8));
    }

    if (fullRedraw) {
      header("nRF Settings");
      footer("UD item LR val SW ok");
      fullRedraw = false;
    }

    if (redraw) {
      int c = colsFrom(8);
      printPad(8, 30, String(item == 0 ? "> " : "  ") + "Channel: " + channel, ST77XX_WHITE, c);

      String r = (rate == 0) ? "250K" : (rate == 1) ? "1M" : "2M";
      printPad(8, 48, String(item == 1 ? "> " : "  ") + "Rate: " + r, ST77XX_WHITE, c);

      String p = (pa == 0) ? "MIN" : (pa == 1) ? "LOW" : (pa == 2) ? "HIGH" : "MAX";
      printPad(8, 66, String(item == 2 ? "> " : "  ") + "PA: " + p, ST77XX_WHITE, c);
      redraw = false;
    }
    delay(8);
  }
}

// -------------------- Joystick diagnostic --------------------
void joystickScreen() {
  header("Joystick Test");
  footer("BACK exit  ROTATE");
  unsigned long lastDraw = 0;

  while (true) {
    if (backPressed()) return;
    if (rotatePressed()) {
      changeRotation();
      header("Joystick Test");
      footer("BACK exit  ROTATE");
      lastDraw = 0;
    }

    int x = analogRead(JOY_X);
    int y = analogRead(JOY_Y);
    JoyDir d = readJoyDirection();

    if (lastDraw == 0 || millis() - lastDraw >= 80) {
      lastDraw = millis();
      int c = colsFrom(5);
      printPad(5, 30,  String("Raw X: ") + x, ST77XX_WHITE, c);
      printPad(5, 45,  String("Raw Y: ") + y, ST77XX_WHITE, c);
      printPad(5, 60,  String("Dir X: ") + (int)d.x, ST77XX_WHITE, c);
      printPad(5, 75,  String("Dir Y: ") + (int)d.y, ST77XX_WHITE, c);
      printPad(5, 90,  String("SW: ") + (digitalRead(JOY_SW) == LOW ? "PRESSED" : "OPEN"), ST77XX_WHITE, c);
      printPad(5, 105, String("Center: ") + joyCenterX + "/" + joyCenterY, ST77XX_WHITE, c);
    }
    delay(5);
  }
}

// -------------------- Hardware / Software Info --------------------
void hardwareScreen() {
  header("Hardware");
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(4, 28);  tft.print("MCU: ESP32");
  tft.setCursor(4, 43);  tft.print("TFT: ST7735 128x160");
  tft.setCursor(4, 58);  tft.print("nRF1: "); tft.print(radio1OK ? "OK" : "FAIL");
  tft.setCursor(4, 73);  tft.print("nRF2: "); tft.print(radio2OK ? "OK" : "FAIL");
  tft.setCursor(4, 88);  tft.print("Joystick: GPIO35/34/32");
  tft.setCursor(4, 103); tft.print("Buttons: 33 / 5");
  tft.setCursor(4, 118); tft.print("Rotation: "); tft.print(screenRotation);
  footer();
  waitBack();
}

void softwareInfo() {
  header("Software Info");
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(2);
  tft.setCursor(6, 30); tft.print("JT OS");
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(6, 55); tft.print("Wireless Utility");
  tft.setCursor(6, 72); tft.print("Created by JwalaTech");
  tft.setCursor(6, 88); tft.print("Version: 1.1.0");
  tft.setCursor(6, 104); tft.print("ESP32 Edition");
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(6, 120); tft.print("JT OS");
  footer("BACK: return");
  waitBack();
}

// -------------------- Games & Feature Modules --------------------
#include "JT_Games.h"
#include "JT1.h"
#include "JT2.h"
#include "JT3.h"
#include "JT4.h"
#include "JT5.h"
#include "JT6.h"
#include "JT7.h"
#include "JT8.h"

typedef void (*JTMenuIconFn)(int x, int y, uint16_t color);
typedef void (*JTFeatureFn)();

struct JTModule {
  const char* title;
  uint16_t color;
  JTMenuIconFn drawIcon;
  JTFeatureFn run;
};

const JTModule jtModules[8] = {
  { JT1_TITLE, JT1_COLOR, jt1DrawIcon, jt1Feature },
  { JT2_TITLE, JT2_COLOR, jt2DrawIcon, jt2Feature },
  { JT3_TITLE, JT3_COLOR, jt3DrawIcon, jt3Feature },
  { JT4_TITLE, JT4_COLOR, jt4DrawIcon, jt4Feature },
  { JT5_TITLE, JT5_COLOR, jt5DrawIcon, jt5Feature },
  { JT6_TITLE, JT6_COLOR, jt6DrawIcon, jt6Feature },
  { JT7_TITLE, JT7_COLOR, jt7DrawIcon, jt7Feature },
  { JT8_TITLE, JT8_COLOR, jt8DrawIcon, jt8Feature }
};

void drawMenuIcon(int idx, int x, int y, uint16_t c) {
  switch (idx) {
    case 0:
      tft.fillCircle(x + 7, y + 11, 1, c);
      tft.drawLine(x + 4, y + 8, x + 7, y + 5, c);
      tft.drawLine(x + 7, y + 5, x + 10, y + 8, c);
      tft.drawLine(x + 1, y + 5, x + 7, y + 1, c);
      tft.drawLine(x + 7, y + 1, x + 13, y + 5, c);
      break;
    case 1:
      tft.drawCircle(x + 6, y + 5, 4, c);
      tft.drawLine(x + 9, y + 8, x + 13, y + 12, c);
      break;
    case 2:
      tft.drawRoundRect(x, y + 3, 14, 8, 3, c);
      tft.drawLine(x + 3, y + 7, x + 6, y + 7, c);
      tft.drawLine(x + 4, y + 6, x + 4, y + 8, c);
      tft.fillCircle(x + 10, y + 6, 1, c);
      tft.fillCircle(x + 12, y + 8, 1, c);
      break;
    case 3:
      tft.drawRect(x + 3, y + 3, 8, 8, c);
      tft.drawLine(x + 7, y + 3, x + 7, y, c);
      tft.drawCircle(x + 7, y + 1, 1, c);
      tft.drawLine(x + 1, y + 4, x - 1, y + 2, c);
      tft.drawLine(x + 13, y + 4, x + 15, y + 2, c);
      break;
    case 4:
      tft.drawCircle(x + 7, y + 7, 4, c);
      tft.drawCircle(x + 7, y + 7, 1, c);
      tft.drawLine(x + 7, y, x + 7, y + 3, c);
      tft.drawLine(x + 7, y + 11, x + 7, y + 14, c);
      tft.drawLine(x, y + 7, x + 3, y + 7, c);
      tft.drawLine(x + 11, y + 7, x + 14, y + 7, c);
      break;
    case 5:
      tft.drawCircle(x + 7, y + 4, 3, c);
      tft.drawLine(x + 7, y + 7, x + 7, y + 11, c);
      tft.drawLine(x + 4, y + 11, x + 10, y + 11, c);
      break;
    case 6:
      tft.drawRect(x + 3, y + 3, 8, 8, c);
      for (int p = 0; p < 3; p++) {
        tft.drawLine(x + 1 + p * 4, y, x + 1 + p * 4, y + 3, c);
        tft.drawLine(x + 1 + p * 4, y + 11, x + 1 + p * 4, y + 14, c);
      }
      break;
    case 7:
      tft.drawCircle(x + 7, y + 7, 6, c);
      tft.setTextSize(1); tft.setTextColor(c); tft.setCursor(x + 6, y + 3); tft.print("i");
      break;
    default:
      break;
  }
}

// -------------------- Menu display styles --------------------
enum { MENU_STYLE_LIST = 0, MENU_STYLE_FULL, MENU_STYLE_WHEEL, MENU_STYLE_GRID, MENU_STYLE_COUNT };
uint8_t menuStyle = MENU_STYLE_LIST;
const char* menuStyleName[MENU_STYLE_COUNT] = { "List", "Fullscreen", "Wheel", "Grid" };
const char* menuShortName[8] = { "WiFi", "Detail", "Games", "nRF", "nRFset", "Joy", "HW", "Info" };
#define MENU_LONG_PRESS_MS 700

void menuTopBarTitle(const char* sub) {
  fastFillRect(0, 0, tft.width(), 20, JT_DARKBLUE);
  fastFillRect(0, 19, tft.width(), 2, ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(4, 6);
  tft.print("JT OS");
  tft.setTextColor(JT_LIGHTGREY);
  tft.setCursor(38, 6);
  tft.print(sub);
  tft.setTextColor(JT_GOLD);
  tft.setCursor(tft.width() - 17, 6);
  tft.print("R"); tft.print(screenRotation);
}

void menuTopBar() { menuTopBarTitle(menuHeading ? menuHeading : menuStyleName[menuStyle]); }

// -------------------- Main menu --------------------
int menuFirst = -1;
int menuDrawnIndex = -1;

uint16_t menuColorId(int id) {
  if (id >= 100) {
    const uint16_t gc[GAMES_COUNT] = { ST77XX_GREEN, JT_SKY, JT_GOLD, JT_ORANGE, JT_PINK, ST77XX_RED, ST77XX_CYAN, JT_LIME, JT_TEAL, JT_SKY, JT_PINK, JT_ORANGE, ST77XX_GREEN, ST77XX_YELLOW, ST77XX_MAGENTA, JT_SKY };
    return gc[id - 100];
  }
  if (id >= 8) return jtModules[id - 8].color;
  switch (id) {
    case 0: return ST77XX_CYAN;
    case 1: return JT_SKY;
    case 2: return JT_GOLD;
    case 3: return ST77XX_GREEN;
    case 4: return JT_ORANGE;
    case 5: return JT_PURPLE;
    case 6: return JT_TEAL;
    default: return JT_PINK;
  }
}

uint16_t menuColor(int idx) { return menuColorId(menuIds[idx]); }

String menuTitleId(int id) {
  if (id >= 100) return String(gamesMenuNames[id - 100]);
  return (id >= 8) ? String(jtModules[id - 8].title) : String(mainMenu[id]);
}

String menuTitle(int idx) { return menuTitleId(menuIds[idx]); }

String menuShort(int idx) {
  int id = menuIds[idx];
  String t;
  if (id >= 100) t = String(gamesMenuNames[id - 100]);
  else t = (id >= 8) ? String(jtModules[id - 8].title) : String(menuShortName[id]);
  if (t.length() > 6) t = t.substring(0, 6);
  return t;
}

void menuIconId(int id, int x, int y, uint16_t c) {
  if (id >= 100) {
    switch (id) {
      case 100:
        tft.fillRect(x + 5, y, 4, 4, c); tft.fillRect(x + 1, y + 4, 4, 4, c);
        tft.fillRect(x + 5, y + 4, 4, 4, c); tft.fillRect(x + 9, y + 4, 4, 4, c);
        tft.fillRect(x + 1, y + 9, 12, 2, c);
        break;
      case 101:
        tft.fillRect(x + 3, y + 1, 9, 2, c); tft.fillRect(x + 3, y + 1, 2, 5, c);
        tft.fillRect(x + 3, y + 5, 9, 2, c); tft.fillRect(x + 10, y + 5, 2, 5, c);
        tft.fillRect(x + 3, y + 9, 9, 2, c);
        break;
      case 102:
        tft.fillTriangle(x + 7, y, x + 2, y + 10, x + 12, y + 10, c);
        tft.fillRect(x + 6, y + 10, 3, 2, c);
        break;
      case 103:
        tft.fillRect(x + 1, y, 5, 3, c); tft.fillRect(x + 8, y, 5, 3, c);
        tft.fillRect(x + 4, y + 4, 5, 3, c);
        tft.fillCircle(x + 7, y + 8, 1, c); tft.fillRect(x + 3, y + 10, 8, 2, c);
        break;
      case 104:
        tft.drawRoundRect(x + 1, y, 12, 12, 2, c);
        tft.setTextSize(1); tft.setTextColor(c); tft.setCursor(x + 5, y + 2); tft.print("2");
        break;
      case 105:
        tft.fillRoundRect(x + 4, y, 6, 12, 2, c);
        tft.fillRect(x + 2, y + 2, 2, 3, c); tft.fillRect(x + 10, y + 2, 2, 3, c);
        tft.fillRect(x + 2, y + 8, 2, 3, c); tft.fillRect(x + 10, y + 8, 2, 3, c);
        break;
      case 106:   // Sky Hopper: bird / jet
        tft.fillRoundRect(x + 1, y + 4, 10, 6, 2, c);
        tft.fillTriangle(x + 11, y + 5, x + 11, y + 8, x + 14, y + 6, c);
        tft.fillTriangle(x + 3, y + 5, x + 7, y + 5, x + 3, y, c);
        break;
      case 107:
        tft.fillRect(x + 3, y + 2, 8, 5, c); tft.fillRect(x + 1, y + 4, 12, 3, c);
        tft.fillRect(x + 2, y + 8, 2, 3, c); tft.fillRect(x + 10, y + 8, 2, 3, c);
        tft.fillRect(x + 5, y + 8, 4, 2, c);
        break;
      case 108:
        tft.fillRect(x + 1, y + 6, 12, 5, c); tft.fillRect(x + 4, y + 3, 6, 3, c);
        tft.fillRect(x + 9, y + 4, 5, 1, c);
        break;
      case 109:
        tft.drawCircle(x + 7, y + 6, 5, c);
        tft.drawFastHLine(x, y + 6, 14, c); tft.drawFastVLine(x + 7, y, 12, c);
        break;
      case 110:   // Turbo Road
        tft.drawLine(x + 6, y, x, y + 11, c); tft.drawLine(x + 8, y, x + 13, y + 11, c);
        tft.drawFastVLine(x + 7, y + 1, 2, c); tft.drawFastVLine(x + 7, y + 5, 3, c); tft.drawFastVLine(x + 7, y + 9, 3, c);
        break;
      case 111:   // Pong Duel
        tft.fillRect(x, y + 2, 2, 7, c); tft.fillRect(x + 12, y + 3, 2, 7, c);
        tft.fillRect(x + 6, y + 5, 3, 3, c);
        break;
      case 112:   // Frog Hop
        tft.fillRoundRect(x + 2, y + 4, 10, 7, 3, c);
        tft.fillCircle(x + 4, y + 3, 2, c); tft.fillCircle(x + 10, y + 3, 2, c);
        tft.fillRect(x + 1, y + 10, 3, 2, c); tft.fillRect(x + 10, y + 10, 3, 2, c);
        break;
      case 113:   // Maze Muncher
        tft.fillCircle(x + 6, y + 6, 5, c);
        tft.fillTriangle(x + 6, y + 6, x + 13, y + 2, x + 13, y + 10, ST77XX_BLACK);
        tft.fillRect(x + 11, y + 5, 2, 2, c);
        break;
      case 114:   // Memory Match
        tft.drawRect(x, y, 6, 5, c); tft.fillRect(x + 8, y, 6, 5, c);
        tft.fillRect(x, y + 7, 6, 5, c); tft.drawRect(x + 8, y + 7, 6, 5, c);
        break;
      case 115:   // Cloud Jump
        tft.fillCircle(x + 4, y + 6, 3, c); tft.fillCircle(x + 8, y + 4, 4, c);
        tft.fillCircle(x + 11, y + 7, 3, c); tft.fillRect(x + 3, y + 7, 9, 3, c);
        break;
      default:
        tft.drawCircle(x + 7, y + 6, 6, c); tft.drawCircle(x + 7, y + 6, 3, c); tft.fillCircle(x + 7, y + 6, 1, c);
        break;
    }
  }
  else if (id >= 8) jtModules[id - 8].drawIcon(x, y, c);
  else drawMenuIcon(id, x, y, c);
}

void menuIcon(int idx, int x, int y, uint16_t c) { menuIconId(menuIds[idx], x, y, c); }

void drawCentered(const String &s, int cx, int y, uint8_t size, uint16_t col) {
  tft.setTextSize(size);
  tft.setTextColor(col);
  int w = (int)s.length() * 6 * size;
  tft.setCursor(cx - w / 2, y);
  tft.print(s);
  tft.setTextSize(1);
}

int menuBodyTop()    { return 22; }
int menuBodyBottom() { return tft.height() - 18; }

void drawMenuCounter() {
  String s = String(menuIndex + 1) + "/" + MENU_COUNT;
  while (s.length() < 6) s += ' ';
  tft.setTextSize(1);
  tft.setTextColor(JT_LIGHTGREY, ST77XX_BLACK);
  tft.setCursor(tft.width() - 38, tft.height() - 14);
  tft.print(s);
}

// ---------- Style 1: LIST ----------
int menuVisibleRows() {
  int rows = (tft.height() - 21 - 18) / 15;
  if (rows > 7) rows = 7;
  if (rows < 1) rows = 1;
  return rows;
}

int menuFirstFor(int idx) {
  int rows = menuVisibleRows();
  int first = idx - rows / 2;
  if (first > MENU_COUNT - rows) first = MENU_COUNT - rows;
  if (first < 0) first = 0;
  return first;
}

void drawMenuRow(int idx, int slot) {
  int y = 25 + slot * 15;
  uint16_t c = menuColor(idx);

  fastFillRect(0, y - 4, tft.width(), 15, ST77XX_BLACK);

  if (idx == menuIndex) {
    tft.fillRoundRect(1, y - 3, tft.width() - 2, 14, 3, JT_PANEL);
    tft.drawRoundRect(1, y - 3, tft.width() - 2, 14, 3, c);
    tft.setTextColor(ST77XX_WHITE);
  } else {
    tft.setTextColor(JT_LIGHTGREY);
  }

  menuIcon(idx, 4, y - 2, c);
  tft.setTextSize(1);
  tft.setCursor(23, y);
  tft.print(menuTitle(idx));
}

void drawMenuList(bool fullRedraw) {
  int rows = menuVisibleRows();
  int first = menuFirstFor(menuIndex);

  if (fullRedraw || first != menuFirst) {
    for (int i = 0; i < rows && (first + i) < MENU_COUNT; i++) drawMenuRow(first + i, i);
  } else if (menuDrawnIndex != menuIndex) {
    if (menuDrawnIndex >= first && menuDrawnIndex < first + rows) {
      drawMenuRow(menuDrawnIndex, menuDrawnIndex - first);
    }
    drawMenuRow(menuIndex, menuIndex - first);
  }
  menuFirst = first;
}

// ---------- Style 2: FULLSCREEN ----------
void drawMenuFull() {
  int top = menuBodyTop();
  int bot = menuBodyBottom();
  int W = tft.width();
  int cx = W / 2;
  int bodyH = bot - top;
  uint16_t c = menuColor(menuIndex);

  fastFillRect(0, top, W, bodyH, ST77XX_BLACK);

  int cardH = bodyH - 14;
  tft.fillRoundRect(4, top + 2, W - 8, cardH, 8, JT_PANEL);
  tft.drawRoundRect(4, top + 2, W - 8, cardH, 8, c);

  int iconCy = top + 22;
  tft.drawCircle(cx, iconCy, 13, c);
  menuIcon(menuIndex, cx - 7, iconCy - 6, c);

  String t = menuTitle(menuIndex);
  int maxW = W - 28;
  int y0 = top + 40;
  int sp = t.indexOf(' ');
  if ((int)t.length() * 12 <= maxW) {
    drawCentered(t, cx, y0 + 6, 2, ST77XX_WHITE);
  } else if (sp > 0) {
    String a = t.substring(0, sp);
    String b = t.substring(sp + 1);
    int widest = max((int)a.length(), (int)b.length()) * 12;
    if (widest <= maxW) {
      drawCentered(a, cx, y0 - 2, 2, ST77XX_WHITE);
      drawCentered(b, cx, y0 + 16, 2, ST77XX_WHITE);
    } else {
      drawCentered(t, cx, y0 + 6, 1, ST77XX_WHITE);
    }
  } else {
    drawCentered(t, cx, y0 + 6, 1, ST77XX_WHITE);
  }

  int midY = top + 2 + cardH / 2;
  tft.fillTriangle(8, midY, 14, midY - 5, 14, midY + 5, c);
  tft.fillTriangle(W - 9, midY, W - 15, midY - 5, W - 15, midY + 5, c);

  int gap = (W - 10) / MENU_COUNT;
  if (gap > 10) gap = 10;
  int startX = cx - (gap * (MENU_COUNT - 1)) / 2;
  for (int i = 0; i < MENU_COUNT; i++) {
    if (i == menuIndex) tft.fillCircle(startX + i * gap, bot - 5, 3, c);
    else tft.fillCircle(startX + i * gap, bot - 5, 1, JT_LIGHTGREY);
  }
}

// ---------- Style 3: WHEEL ----------
void drawMenuWheel() {
  int top = menuBodyTop();
  int bot = menuBodyBottom();
  int W = tft.width();
  int cy = top + (bot - top) / 2;

  fastFillRect(0, top, W, bot - top, ST77XX_BLACK);

  for (int k = -2; k <= 2; k++) {
    int ak = abs(k);
    int step = (ak == 1) ? 26 : 44;
    int y = (k == 0) ? cy : cy + ((k < 0) ? -step : step);
    if (y - 9 < top || y + 9 > bot) continue;

    int idx = (menuIndex + k + MENU_COUNT) % MENU_COUNT;

    if (k == 0) {
      uint16_t c = menuColor(idx);
      tft.fillRoundRect(2, cy - 12, W - 4, 24, 6, JT_PANEL);
      tft.drawRoundRect(2, cy - 12, W - 4, 24, 6, c);
      menuIcon(idx, 7, cy - 6, c);
      tft.setTextSize(1);
      tft.setTextColor(ST77XX_WHITE);
      String t = menuTitle(idx);
      tft.setCursor(27, cy - 4); tft.print(t);
      tft.setCursor(28, cy - 4); tft.print(t);
    } else {
      int indent = ak * 8;
      uint16_t c = (ak == 1) ? JT_LIGHTGREY : 0x8410;
      if (ak == 2) c = 0x4208;
      menuIcon(idx, 7 + indent, y - 6, c);
      String t = menuTitle(idx);
      int maxChars = (W - (27 + indent)) / 6;
      if ((int)t.length() > maxChars) t = t.substring(0, maxChars);
      tft.setTextSize(1);
      tft.setTextColor(c);
      tft.setCursor(27 + indent, y - 4);
      tft.print(t);
    }
  }
}

// ---------- Style 4: GRID ----------
int menuGridCols() { return (tft.width() >= tft.height()) ? 4 : 3; }

void drawMenuGridTile(int idx) {
  int cols = menuGridCols();
  int rows = (MENU_COUNT + cols - 1) / cols;
  int top = menuBodyTop();
  int bodyH = menuBodyBottom() - top;
  int tw = tft.width() / cols;
  int th = bodyH / rows;
  int x = (idx % cols) * tw;
  int y = top + (idx / cols) * th;
  uint16_t c = menuColor(idx);

  fastFillRect(x, y, tw, th, ST77XX_BLACK);
  if (idx == menuIndex) {
    tft.fillRoundRect(x + 1, y + 1, tw - 2, th - 2, 4, JT_PANEL);
    tft.drawRoundRect(x + 1, y + 1, tw - 2, th - 2, 4, c);
  }
  menuIcon(idx, x + tw / 2 - 7, y + 4, c);
  drawCentered(menuShort(idx), x + tw / 2, y + th - 11, 1,
               (idx == menuIndex) ? ST77XX_WHITE : JT_LIGHTGREY);
}

void drawMenuGrid(bool fullRedraw) {
  if (fullRedraw || menuDrawnIndex < 0) {
    for (int i = 0; i < MENU_COUNT; i++) drawMenuGridTile(i);
  } else if (menuDrawnIndex != menuIndex) {
    drawMenuGridTile(menuDrawnIndex);
    drawMenuGridTile(menuIndex);
  }
}

// ---------- Dispatcher ----------
void drawMainMenu(bool fullRedraw) {
  if (fullRedraw) {
    jtFillScreen(ST77XX_BLACK);
    menuTopBar();

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(3, tft.height() - 14);
    tft.print("SW Select");
    tft.setTextColor(JT_LIGHTGREY);
    tft.setCursor(66, tft.height() - 14);
    tft.print("BACK");

    menuFirst = -1;
    menuDrawnIndex = -1;
  }

  bool moved = (menuDrawnIndex != menuIndex);

  switch (menuStyle) {
    case MENU_STYLE_FULL:  if (fullRedraw || moved) drawMenuFull();  break;
    case MENU_STYLE_WHEEL: if (fullRedraw || moved) drawMenuWheel(); break;
    case MENU_STYLE_GRID:  drawMenuGrid(fullRedraw); break;
    default:               drawMenuList(fullRedraw); break;
  }

  menuDrawnIndex = menuIndex;
  drawMenuCounter();
}

bool menuNavigate(const JoyDir &d) {
  int step = 0;
  if (menuStyle == MENU_STYLE_GRID) {
    if (d.y != 0) step = d.y * menuGridCols();
    else if (d.x != 0) step = d.x;
  } else if (menuStyle == MENU_STYLE_LIST) {
    step = d.y;
  } else {
    step = (d.y != 0) ? d.y : d.x;
  }
  if (step == 0) return false;
  menuIndex = ((menuIndex + step) % MENU_COUNT + MENU_COUNT) % MENU_COUNT;
  return true;
}

void styledMenu(const uint8_t* ids, int count, const char* heading, void (*launch)(int id)) {
  menuIds = ids;
  MENU_COUNT = count;
  menuHeading = heading;
  menuIndex = 0;
  drawMainMenu(true);

  bool rotHeld = false;
  bool rotLongDone = false;
  unsigned long rotDownAt = 0;

  while (true) {
    bool rotRaw = (digitalRead(ROTATE_BUTTON) == LOW);
    if (rotRaw && !rotHeld) {
      rotHeld = true;
      rotLongDone = false;
      rotDownAt = millis();
    } else if (rotRaw && rotHeld && !rotLongDone && (millis() - rotDownAt) >= MENU_LONG_PRESS_MS) {
      rotLongDone = true;
      menuStyle = (menuStyle + 1) % MENU_STYLE_COUNT;
      prefs.putUChar("menustyle", menuStyle);
      drawMainMenu(true);
    } else if (!rotRaw && rotHeld) {
      rotHeld = false;
      if (!rotLongDone && (millis() - rotDownAt) >= 30) {
        changeRotation();
        drawMainMenu(true);
      }
    }

    if (backPressed()) return;

    JoyDir d = readJoyDirection();
    if (menuNavigate(d)) {
      drawMainMenu(false);
      delay(160);
    }

    if (joyPressed()) {
      int saved = menuIndex;
      launch(ids[menuIndex]);
      menuIds = ids;
      MENU_COUNT = count;
      menuHeading = heading;
      menuIndex = saved;
      rotHeld = false;
      rotLongDone = false;
      drawMainMenu(true);
      delay(120);
    }

    delay(15);
  }
}

void launchMainItem(int id) {
  switch (id) {
    case 0: wifiScanner(); break;
    case 1: wifiDetails(); break;
    case 3: nrfTest(); break;
    case 5: joystickScreen(); break;
    case 8: case 9: case 10: case 11:
    case 12: case 13: case 14: case 15:
      jtModules[id - 8].run(); break;
  }
}

void launchSettingsItem(int id) {
  switch (id) {
    case 4: nrfSettings(); break;
    case 6: hardwareScreen(); break;
    case 7: softwareInfo(); break;
  }
}

void launchGame(int id) {
  switch (id) {
    case 100: gameBlockFall(); break;
    case 101: gameNeonSnake(); break;
    case 102: gameStarRaid(); break;
    case 103: gameBreaker(); break;
    case 104: game2048(); break;
    case 105: gameRoadRacer(); break;
    case 106: gameSkyHopper(); break;
    case 107: gameInvaders(); break;
    case 108: gameTankBattle(); break;
    case 109: gameSkyHunter(); break;
    case 110: gameTurboRoad(); break;
    case 111: gamePong(); break;
    case 112: gameFrogHop(); break;
    case 113: gameMazeMuncher(); break;
    case 114: gameMemoryMatch(); break;
    case 115: gameCloudJump(); break;
  }
}

void mainMenuLoop() { styledMenu(mainMenuIds, MAIN_MENU_COUNT, nullptr, launchMainItem); }
void settingsMenu() { styledMenu(settingsIds, SETTINGS_COUNT, "Settings", launchSettingsItem); }
void gamesMenu()    { styledMenu(gamesMenuIds, GAMES_COUNT, "Games", launchGame); }

// -------------------- Welcome page --------------------
const uint16_t JT_RED = 0xF800;
const char* welcomeItems[3] = { "Games", "Settings", "Menu" };
int welcomeIndex = 2;

void drawJtLogo(int x, int y) {
  tft.startWrite();
  tft.setAddrWindow(x, y, JT_LOGO_W, JT_LOGO_H);
  tft.writePixels((uint16_t*)jtLogo, (uint32_t)JT_LOGO_W * JT_LOGO_H);
  tft.endWrite();
}

int welcomeLogoY()  { return (tft.height() >= 140) ? 8 : 3; }
int welcomeRowTop() { return welcomeLogoY() + JT_LOGO_H + ((tft.height() >= 140) ? 8 : 5); }
int welcomeRowGap() {
  int gap = (tft.height() - welcomeRowTop() - 4) / 3;
  return (gap > 26) ? 26 : gap;
}

void drawWelcomeIcon(int i, int x, int y, uint16_t c) {
  if (i == 0) drawMenuIcon(2, x, y, c);
  else if (i == 1) drawMenuIcon(4, x, y, c);
  else {
    tft.drawLine(x, y + 1, x + 13, y + 1, c);
    tft.drawLine(x, y + 5, x + 13, y + 5, c);
    tft.drawLine(x, y + 9, x + 13, y + 9, c);
  }
}

void drawWelcomeRow(int i) {
  int W = tft.width();
  int gap = welcomeRowGap();
  int rowH = gap - 3;
  int y = welcomeRowTop() + i * gap;
  bool sel = (i == welcomeIndex);

  fastFillRect(0, y - 1, W, rowH + 2, ST77XX_BLACK);
  if (sel) {
    tft.fillRoundRect(6, y, W - 12, rowH, 5, 0x2000);
    tft.drawRoundRect(6, y, W - 12, rowH, 5, JT_RED);
    tft.setTextColor(ST77XX_WHITE);
  } else {
    tft.drawRoundRect(6, y, W - 12, rowH, 5, 0x4208);
    tft.setTextColor(JT_LIGHTGREY);
  }
  drawWelcomeIcon(i, 14, y + (rowH - 12) / 2, sel ? JT_RED : JT_LIGHTGREY);
  tft.setTextSize(1);
  tft.setCursor(38, y + (rowH - 8) / 2);
  tft.print(welcomeItems[i]);
}

void drawWelcome() {
  jtFillScreen(ST77XX_BLACK);
  drawJtLogo((tft.width() - JT_LOGO_W) / 2, welcomeLogoY());
  for (int i = 0; i < 3; i++) drawWelcomeRow(i);
}

void welcomeLoop() {
  drawWelcome();

  while (true) {
    if (rotatePressed()) {
      changeRotation();
      drawWelcome();
    }

    JoyDir d = readJoyDirection();
    if (d.y != 0) {
      int old = welcomeIndex;
      welcomeIndex = (welcomeIndex + d.y + 3) % 3;
      drawWelcomeRow(old);
      drawWelcomeRow(welcomeIndex);
      delay(160);
    }

    if (joyPressed()) {
      switch (welcomeIndex) {
        case 0: gamesMenu(); break;
        case 1: settingsMenu(); break;
        case 2: mainMenuLoop(); break;
      }
      drawWelcome();
      delay(120);
    }
    delay(15);
  }
}

// -------------------- Setup / loop --------------------
void setup() {
  // Call module setup functions (if declared in JT1.h / JT2.h)
  JT1_setup();
  #ifdef JT2_SETUP_EXISTS
  JT2_setup();
  #endif

  Serial.begin(115200);

  pinMode(BACK_BUTTON, INPUT_PULLUP);
  pinMode(ROTATE_BUTTON, INPUT_PULLUP);
  pinMode(JOY_SW, INPUT_PULLUP);

  analogReadResolution(12);
  pinMode(JOY_X, INPUT);
  pinMode(JOY_Y, INPUT);

  prefs.begin("jt-os", false);
  menuStyle = prefs.getUChar("menustyle", MENU_STYLE_LIST) % MENU_STYLE_COUNT;
  screenRotation = DEFAULT_ROTATION;
  prefs.putUChar("rotation", screenRotation);

#if JT_TFT_HW_SPI
  vspi.begin(NRF2_SCK, NRF2_MISO, NRF2_MOSI, TFT_CS);
#endif
  tft.initR(INITR_BLACKTAB);
  applyRotation();
  jtFillScreen(ST77XX_BLACK);

  drawJtLogo((tft.width() - JT_LOGO_W) / 2, (tft.height() >= 140) ? 8 : 3);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(8, (tft.height() >= 140) ? 84 : 72);
  tft.print("Created by JwalaTech");
  tft.setCursor(8, (tft.height() >= 140) ? 99 : 87);
  tft.print("Center joystick...");
  calibrateJoystick();
  joyStateX = 0;
  joyStateY = 0;

  initRadios();
  WiFi.mode(WIFI_STA);
}

void loop() {
  welcomeLoop();
  JT1_loop();
  #ifdef JT2_LOOP_EXISTS
  JT2_loop();
  #endif
}
