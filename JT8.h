#ifndef JT8_H
#define JT8_H

/*
  JT 8 feature module (placeholder).
  Same interface as JT1-JT4: title, colour, menu icon and a feature screen.
  Replace jt8Feature() with your own code. BACK exits, ROTATE rotates.
*/

#define JT8_TITLE "JT 8"
#define JT8_COLOR JT_PINK

// 14x12 menu icon
static void jt8DrawIcon(int x, int y, uint16_t c) {
  tft.drawCircle(x + 7, y + 6, 6, c); tft.fillCircle(x + 7, y + 6, 2, c);
}

static void jt8Feature() {
  header(JT8_TITLE);

  tft.setTextColor(JT_PINK);
  tft.setTextSize(2);
  tft.setCursor(20, 35);
  tft.print(JT8_TITLE);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(20, 62);
  tft.print("Add Custom Feature By,");

  tft.setTextColor(JT_LIGHTGREY);
  tft.setCursor(20, 82);
  tft.print("Adding The Code on JT8.h file");

  footer("BACK: return");
  waitBack();
}

#endif
