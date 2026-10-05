#ifndef JT5_H
#define JT5_H

/*
  JT 5 feature module (placeholder).
  Same interface as JT1-JT4: title, colour, menu icon and a feature screen.
  Replace jt5Feature() with your own code. BACK exits, ROTATE rotates.
*/

#define JT5_TITLE "JT 5"
#define JT5_COLOR ST77XX_GREEN

// 14x12 menu icon
static void jt5DrawIcon(int x, int y, uint16_t c) {
  tft.drawLine(x + 7, y, x + 13, y + 6, c); tft.drawLine(x + 13, y + 6, x + 7, y + 12, c);
  tft.drawLine(x + 7, y + 12, x + 1, y + 6, c); tft.drawLine(x + 1, y + 6, x + 7, y, c);
}

static void jt5Feature() {
  header(JT5_TITLE);

  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(2);
  tft.setCursor(20, 35);
  tft.print(JT5_TITLE);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(20, 62);
  tft.print("Add Custom Feature By,");

  tft.setTextColor(JT_LIGHTGREY);
  tft.setCursor(20, 82);
  tft.print("Adding The Code on JT5.h file");

  footer("BACK: return");
  waitBack();
}

#endif
