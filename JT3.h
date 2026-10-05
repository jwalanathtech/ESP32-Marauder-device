#ifndef JT3_H
#define JT3_H

// ================================================================
// JT 3 - INDEPENDENT FEATURE MODULE
// Everything specific to this JT slot lives in this file:
//   1. Menu title
//   2. Menu accent color
//   3. Menu logo/icon
//   4. Feature screen
//
// To add/replace the feature later, edit this file without changing
// the main ESP32_JT_OS.ino menu code.
// ================================================================

#define JT3_TITLE "JT 3"
#define JT3_COLOR JT_SKY

// JT3 menu logo/icon. Change only this function to redesign the JT3 logo.
static void jt3DrawIcon(int x, int y, uint16_t c) {

  // --- Syringe barrel ---
  tft.drawRect(x + 5, y + 3, 6, 8, c);

  // --- Plunger ---
  tft.drawLine(x + 4, y + 4, x + 12, y + 4, c);
  tft.drawLine(x + 7, y + 1, x + 7, y + 4, c);
  tft.drawLine(x + 9, y + 1, x + 9, y + 4, c);

  // --- Syringe tip ---
  tft.drawLine(x + 8, y + 11, x + 8, y + 13, c);
  tft.drawLine(x + 8, y + 13, x + 12, y + 13, c);

  // --- Needle ---
  tft.drawLine(x + 12, y + 13, x + 14, y + 15, c);

  // --- Measurement lines ---
  tft.drawFastHLine(x + 6, y + 6, 3, c);
  tft.drawFastHLine(x + 6, y + 8, 3, c);
  tft.drawFastHLine(x + 6, y + 10, 3, c);
}


// JT feature screen. Replace the body with the real JT feature later.
static void jt3Feature() {
  header(JT3_TITLE);

  tft.setTextColor(JT_SKY);
  tft.setTextSize(2);
  tft.setCursor(20, 35);
  tft.print(JT3_TITLE);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(20, 62);
  tft.print("Add Custom Feature By,");

  tft.setTextColor(JT_LIGHTGREY);
  tft.setCursor(20, 82);
  tft.print("Adding The Code on JT3.h file");

  footer("BACK: return");
  waitBack();
}

#endif
