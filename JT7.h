#ifndef JT7_H
#define JT7_H

/*
  JT 7 feature module (placeholder).
  Same interface as JT1-JT4: title, colour, menu icon and a feature screen.
  Replace jt7Feature() with your own code. BACK exits, ROTATE rotates.
*/

#define JT7_TITLE "JT 7"
#define JT7_COLOR JT_SKY

// 14x12 menu icon
static void jt7DrawIcon(int x, int y, uint16_t c) {
  tft.drawRect(x + 2, y + 1, 10, 10, c); tft.fillRect(x + 5, y + 4, 4, 4, c);
}

static void jt7Feature() {
  header(JT7_TITLE);

  tft.setTextColor(JT_SKY);
  tft.setTextSize(2);
  tft.setCursor(20, 35);
  tft.print(JT7_TITLE);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(20, 62);
  tft.print("Add Custom Feature By,");

  tft.setTextColor(JT_LIGHTGREY);
  tft.setCursor(20, 82);
  tft.print("Adding The Code on JT7.h file");

  footer("BACK: return");
  waitBack();
}

#endif
