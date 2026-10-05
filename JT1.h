#ifndef JT1_H
#define JT1_H

#include <Arduino.h>
#include <SPI.h>
#include "RF24.h"
#include "esp_bt.h"
#include "esp_wifi.h"

// --- Module Metadata & Styling ---
#define JT1_TITLE "nRF Carrier Jam"
#define JT1_COLOR ST77XX_RED

// External references provided by the main sketch
extern RF24 radio3; // HSPI Radio
extern RF24 radio2; // VSPI Radio
extern bool radio1OK;
extern bool radio2OK;

// Module State
static bool jt1Active = false;
static int ch_hspi = 45;
static int ch_vspi = 45;
static unsigned int flag_hspi = 0;
static unsigned int flag_vspi = 0;

// Draw JT1 Icon for Main Menu
inline void jt1DrawIcon(int x, int y, uint16_t color) {
  tft.drawTriangle(x + 7, y + 1, x + 1, y + 12, x + 13, y + 12, color);
  tft.drawLine(x + 7, y + 4, x + 7, y + 8, color);
  tft.drawPixel(x + 7, y + 10, color);
}

// Separate setup function called inside main setup()
inline void JT1_setup() {
  // Reserved for custom boot setups if needed
}

// Carrier / Channel frequency sweep routine
inline void jt1_carrier_sweep() {
  // Channel sweep algorithm for VSPI radio
  if (flag_vspi == 0) {
    ch_vspi += 4;
  } else {
    ch_vspi -= 4;
  }

  // Channel sweep algorithm for HSPI radio
  if (flag_hspi == 0) {
    ch_hspi += 2;
  } else {
    ch_hspi -= 2;
  }

  // VSPI Boundaries
  if ((ch_vspi > 79) && (flag_vspi == 0)) {
    flag_vspi = 1;
  } else if ((ch_vspi < 2) && (flag_vspi == 1)) {
    flag_vspi = 0;
  }

  // HSPI Boundaries
  if ((ch_hspi > 79) && (flag_hspi == 0)) {
    flag_hspi = 1;
  } else if ((ch_hspi < 2) && (flag_hspi == 1)) {
    flag_hspi = 0;
  }

  // Update hardware channels
  if (radio1OK) radio3.setChannel(ch_hspi);
  if (radio2OK) radio2.setChannel(ch_vspi);
}

// Start RF Transmitters
inline void jt1_start_jamming() {
  // Disable Bluetooth and WiFi to clear RF spectrum & prevent brownout
  esp_bt_controller_deinit();
  esp_wifi_stop();
  esp_wifi_deinit();
  esp_wifi_disconnect();

  if (radio1OK) {
    radio3.setAutoAck(false);
    radio3.stopListening();
    radio3.setRetries(0, 0);
    radio3.setPALevel(RF24_PA_MAX, true);
    radio3.setDataRate(RF24_2MBPS);
    radio3.setCRCLength(RF24_CRC_DISABLED);
    radio3.startConstCarrier(RF24_PA_MAX, ch_hspi);
  }

  if (radio2OK) {
    radio2.setAutoAck(false);
    radio2.stopListening();
    radio2.setRetries(0, 0);
    radio2.setPALevel(RF24_PA_MAX, true);
    radio2.setDataRate(RF24_2MBPS);
    radio2.setCRCLength(RF24_CRC_DISABLED);
    radio2.startConstCarrier(RF24_PA_MAX, ch_vspi);
  }

  jt1Active = true;
}

// Stop RF Transmitters
inline void jt1_stop_jamming() {
  if (radio1OK) {
    radio3.stopConstCarrier();
    radio3.powerDown();
  }
  if (radio2OK) {
    radio2.stopConstCarrier();
    radio2.powerDown();
  }

  // Restore WiFi Station mode for the main OS
  WiFi.mode(WIFI_STA);

  jt1Active = false;
}

// UI Screen Renderer
inline void jt1_drawUI() {
  header(JT1_TITLE);

  // Center status box
  if (jt1Active) {
    tft.fillRect(10, 30, tft.width() - 20, 34, ST77XX_RED);
    tft.drawRect(10, 30, tft.width() - 20, 34, ST77XX_WHITE);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.setCursor(20, 39);
    tft.print("JAMMING");
  } else {
    tft.fillRect(10, 30, tft.width() - 20, 34, JT_PANEL);
    tft.drawRect(10, 30, tft.width() - 20, 34, JT_LIGHTGREY);
    tft.setTextColor(ST77XX_GREEN);
    tft.setTextSize(2);
    tft.setCursor(22, 39);
    tft.print("STOPPED");
  }

  // Details
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(8, 75);
  tft.print("HSPI (nRF1): ");
  tft.print(radio1OK ? (jt1Active ? "ACTIVE" : "READY") : "FAIL");

  tft.setCursor(8, 90);
  tft.print("VSPI (nRF2): ");
  tft.print(radio2OK ? (jt1Active ? "ACTIVE" : "READY") : "FAIL");

  tft.setCursor(8, 105);
  tft.print("PA Level: ");
  tft.setTextColor(ST77XX_YELLOW);
  tft.print("MAX");

  footer("SW: Toggle   BACK: Exit");
}

// Main Feature Function invoked from the Main Menu
inline void jt1Feature() {
  jt1_drawUI();

  while (true) {
    // Rotation button check
    if (rotatePressed()) {
      changeRotation();
      jt1_drawUI();
    }

    // Toggle button press
    if (joyPressed()) {
      if (!jt1Active) {
        jt1_start_jamming();
      } else {
        jt1_stop_jamming();
      }
      jt1_drawUI();
      delay(150);
    }

    // Perform carrier frequency sweeps while active
    if (jt1Active) {
      jt1_carrier_sweep();
      delayMicroseconds(100);
    }

    // Exit condition
    if (backPressed()) {
      if (jt1Active) {
        jt1_stop_jamming();
      }
      return;
    }

    yield(); // Prevent Task Watchdog Resets
  }
}

// Executed in continuous loop if referenced
inline void JT1_loop() {
  if (jt1Active) {
    jt1_carrier_sweep();
  }
}

#endif // JT1_H
