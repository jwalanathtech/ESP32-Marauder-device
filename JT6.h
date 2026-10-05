#ifndef JT6_H
#define JT6_H

/*
  JT 6 feature module (placeholder).
  Same interface as JT1-JT4: title, colour, menu icon and a feature screen.
  Replace jt6Feature() with your own code. BACK exits, ROTATE rotates.
*/

#define JT6_TITLE "JT 6"
#define JT6_COLOR JT_ORANGE

// 14x12 menu icon
static void jt6DrawIcon(int x, int y, uint16_t c) {
  tft.drawLine(x + 7, y, x + 13, y + 12, c); tft.drawLine(x + 13, y + 12, x + 1, y + 12, c);
  tft.drawLine(x + 1, y + 12, x + 7, y, c);
}

static void jt6Feature() {
  header(JT6_TITLE);

  tft.setTextColor(JT_ORANGE);
  tft.setTextSize(2);
  tft.setCursor(20, 35);
  tft.print(JT6_TITLE);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(20, 62);
  tft.print("Add Custom Feature By,");

  tft.setTextColor(JT_LIGHTGREY);
  tft.setCursor(20, 82);
  tft.print("Adding The Code on JT6.h file");

  footer("BACK: return");
  waitBack();
}

#endif
