#ifndef JT4_H
#define JT4_H

// ================================================================
// JT 4 - INDEPENDENT FEATURE MODULE
// Everything specific to this JT slot lives in this file:
//   1. Menu title
//   2. Menu accent color
//   3. Menu logo/icon
//   4. Feature screen
//
// To add/replace the feature later, edit this file without changing
// the main ESP32_JT_OS.ino menu code.
// ================================================================

#define JT4_TITLE "JT 4"
#define JT4_COLOR JT_GOLD

// JT4 menu logo/icon. Change only this function to redesign the JT4 logo.
static void jt4DrawIcon(int x, int y, uint16_t c) {
  // --- Geometric Skull Outer Frame ---
  tft.drawLine(x + 5, y,     x + 11, y,      c); // Top skull crown
  tft.drawLine(x + 5, y,     x + 1,  y + 4,  c); // Top-left slope
  tft.drawLine(x + 11, y,    x + 15, y + 4,  c); // Top-right slope
  tft.drawLine(x + 1, y + 4, x + 1,  y + 9,  c); // Left cheek
  tft.drawLine(x + 15, y + 4, x + 15, y + 9,  c); // Right cheek
  tft.drawLine(x + 1, y + 9, x + 4,  y + 15, c); // Left jaw slope
  tft.drawLine(x + 15, y + 9, x + 12, y + 15, c); // Right jaw slope
  tft.drawLine(x + 4, y + 15, x + 12, y + 15, c); // Bottom jaw

  // --- Eye Sockets & Nose Bridge ---
  tft.drawRect(x + 3, y + 6, 3, 3, c);           // Left eye socket
  tft.drawRect(x + 10, y + 6, 3, 3, c);          // Right eye socket
  tft.drawLine(x + 8, y + 7, x + 7, y + 9, c);   // Nose cavity left
  tft.drawLine(x + 8, y + 7, x + 9, y + 9, c);   // Nose cavity right

  // --- Teeth Detail ---
  tft.drawFastHLine(x + 5, y + 11, 7, c);        // Mouth dividing line
  tft.drawFastVLine(x + 6, y + 10, 3, c);        // Tooth divider 1
  tft.drawFastVLine(x + 8, y + 10, 3, c);        // Tooth divider 2
  tft.drawFastVLine(x + 10, y + 10, 3, c);       // Tooth divider 3

  // --- Wi-Fi Signal Symbol (Forehead Area) ---
  tft.fillCircle(x + 8, y + 4, 1, c);            // Wi-Fi dot center
  tft.drawLine(x + 6, y + 2, x + 10, y + 2, c);  // Wi-Fi inner arc
  tft.drawLine(x + 5, y + 1, x + 11, y + 1, c);  // Wi-Fi outer arc
}



// JT feature screen. Replace the body with the real JT feature later.
static void jt4Feature() {
  header(JT4_TITLE);

  tft.setTextColor(JT_GOLD);
  tft.setTextSize(2);
  tft.setCursor(20, 35);
  tft.print(JT4_TITLE);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(20, 62);
  tft.print("Add Custom Feature By,");

  tft.setTextColor(JT_LIGHTGREY);
  tft.setCursor(20, 82);
  tft.print("Adding The Code on JT4.h file");

  footer("BACK: return");
  waitBack();
}

#endif
