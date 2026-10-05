#ifndef JT_GAMES_H
#define JT_GAMES_H

/*
  JT OS Games module  (v2 - premium game pack)

  Games:
    1. Block Fall    - falling blocks, ghost piece, next preview, levels
    2. Neon Snake    - glowing gradient snake, bonus stars, rocks, wrap walls
    3. Star Raid     - scrolling star field, waves, bosses, power-ups, bombs
    4. Breaker       - coloured bricks, multi-ball, power-ups, 5 level layouts
    5. 2048          - slide & merge tiles, 3x3 / 4x4 / 5x5, undo
    6. Road Racer    - top-down highway driving, traffic, coins, nitro
    7. Sky Hopper    - flap through the pillars
    8. Invaders      - classic alien waves
    9. Tank Battle   - top-down tank battle, destructible walls, defend base
   10. Sky Hunter    - shooting gallery
   11. Turbo Road    - pseudo-3D arcade racer with curves, checkpoints, nitro
   12. Pong Duel     - paddle duel against the CPU, spin shots, rally speed-up   (new)
   13. Frog Hop      - cross traffic and ride the logs                           (new)
   14. Maze Muncher  - eat the dots, power pellets, 2-4 ghosts                   (new)
   15. Memory Match  - flip cards, combos, endless boards                        (new)
   16. Cloud Jump    - bounce up through moving / crumbly clouds and springs     (new)

  (Off-Road, Asteroids and Target Blast were removed; Sky Hopper replaces Off-Road.)

  Every game opens an OPTIONS screen first (difficulty, theme, ...).
  Options and high scores are saved in flash (Preferences).

  Controls (all games):
    Joystick      = move / steer (screen-relative)
    Joystick SW   = action (see the tip line on each options screen)
    ROTATE button = rotate the display
    BACK button   = leave game (back to options) / leave options (back to menu)

  Expects from the main file: tft, prefs, fastFillRect(), jtFillScreen(),
  readJoyDirection(), joyPressed(), backPressed(), rotatePressed(), changeRotation().
*/

// ============================================================================
//  Flicker-free rendering engine
//
//  Most games erase every sprite and redraw it a few milliseconds later, which shows
//  as flicker on a display that is fed one pixel at a time. Games started with
//  gRun() instead draw into an off-screen canvas, and only the parts of the picture
//  that REALLY changed since the last frame are sent to the display (8-row bands,
//  compared against what the display already shows). The display therefore never
//  shows a half-drawn frame. If memory is short the games silently fall back to
//  drawing directly.
//
//  The macros below (undone at the end of this file) route every tft.xxx() call in
//  this file to the canvas while a game is running, and push the canvas whenever a
//  game calls backPressed() or delay() (every game loop does).
// ============================================================================
#include <new>
#include <string.h>

static Adafruit_ST7735 &gRealTft = tft;
static GFXcanvas16 *gCv = nullptr;
static uint16_t    *gShadow = nullptr;
static int  gCvW = 0, gCvH = 0;
static bool gFrameOn = false, gShadowOk = false;
static Adafruit_GFX *gDraw = &tft;

static void gFrameFree() {
  if (gCv) { delete gCv; gCv = nullptr; }
  if (gShadow) { free(gShadow); gShadow = nullptr; }
  gFrameOn = false;
  gShadowOk = false;
  gDraw = &gRealTft;
}

static bool gFrameBegin() {
  gFrameFree();
  int w = gRealTft.width(), h = gRealTft.height();
  gCv = new (std::nothrow) GFXcanvas16(w, h);
  if (!gCv || !gCv->getBuffer()) { gFrameFree(); return false; }
  gShadow = (uint16_t*)malloc((size_t)w * h * 2);
  if (!gShadow) { gFrameFree(); return false; }
  gCvW = w; gCvH = h;
  gShadowOk = false;          // first push sends the whole picture
  gFrameOn = true;
  gDraw = gCv;
  return true;
}

// Send everything that changed since the last push.
static void gFramePush() {
  if (!gFrameOn || !gCv) return;
  const int W = gCvW, H = gCvH, BAND = 8;
  uint16_t *cv = gCv->getBuffer();
  bool any = false;
  for (int y0 = 0; y0 < H; y0 += BAND) {
    int y1 = (y0 + BAND < H) ? y0 + BAND : H;
    int xmin = W, xmax = -1;
    if (!gShadowOk) {
      xmin = 0; xmax = W - 1;
    } else {
      for (int y = y0; y < y1; y++) {
        const uint16_t *a = cv + (size_t)y * W;
        const uint16_t *b = gShadow + (size_t)y * W;
        if (memcmp(a, b, (size_t)W * 2) == 0) continue;
        int i = 0;     while (a[i] == b[i]) i++;
        int j = W - 1; while (a[j] == b[j]) j--;
        if (i < xmin) xmin = i;
        if (j > xmax) xmax = j;
      }
    }
    if (xmax < xmin) continue;
    int w = xmax - xmin + 1;
    if (!any) { gRealTft.startWrite(); any = true; }
    gRealTft.setAddrWindow(xmin, y0, w, y1 - y0);
    for (int y = y0; y < y1; y++) {
      uint16_t *row = cv + (size_t)y * W + xmin;
      gRealTft.writePixels(row, (uint32_t)w);
      memcpy(gShadow + (size_t)y * W + xmin, row, (size_t)w * 2);
    }
  }
  if (any) gRealTft.endWrite();
  gShadowOk = true;
}

static void gFrameEnd() {
  gFramePush();
  gFrameFree();
}

struct GameOpt;
typedef long (*GamePlayFn)(const GameOpt*);

// Runs one game round with buffered, flicker-free drawing.
static long gRun(GamePlayFn fn, const GameOpt* o) {
  gFrameBegin();
  long r = fn(o);
  gFrameEnd();
  return r;
}

// Wrappers around the sketch's own helpers (they must see the real display or the canvas).
static void gFastFill(int x, int y, int w, int h, uint16_t c) {
  if (gFrameOn) gCv->fillRect(x, y, w, h, c); else fastFillRect(x, y, w, h, c);
}
static void gFillScreen(uint16_t c) {
  if (gFrameOn) gCv->fillScreen(c); else jtFillScreen(c);
}
static void gChangeRotation() {
  changeRotation();
  if (gFrameOn) gFrameBegin();   // new size -> new canvas, full push next time
}
static bool gBackPressed() {
  if (gFrameOn) gFramePush();    // every game loop calls this once per iteration
  return backPressed();
}
static void gDelay(unsigned long ms) {
  if (gFrameOn) gFramePush();
  delay(ms);
}

#define tft            (*gDraw)
#define fastFillRect   gFastFill
#define jtFillScreen   gFillScreen
#define changeRotation gChangeRotation
#define backPressed    gBackPressed
#define delay          gDelay

// ============================================================================
//  Shared helpers: colours, themes, text, options screen, game-over screen
// ============================================================================
#define JT_RGB(r,g,b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

struct JTTheme { const char* name; uint16_t bg, c1, c2, c3, c4; };

static const JTTheme jtThemes[4] = {
  { "Neon",   JT_RGB(6, 6, 18),  JT_RGB(0, 230, 255), JT_RGB(255, 40, 200), JT_RGB(255, 225, 60), JT_RGB(60, 255, 120) },
  { "Retro",  JT_RGB(8, 16, 8),  JT_RGB(80, 255, 80), JT_RGB(255, 176, 0),  JT_RGB(255, 80, 40),  JT_RGB(150, 255, 210) },
  { "Sunset", JT_RGB(20, 8, 24), JT_RGB(255, 120, 40), JT_RGB(255, 50, 120), JT_RGB(255, 210, 80), JT_RGB(170, 90, 255) },
  { "Ocean",  JT_RGB(4, 12, 28), JT_RGB(60, 170, 255), JT_RGB(0, 230, 200), JT_RGB(230, 245, 255), JT_RGB(255, 150, 90) }
};
static const JTTheme* jtTh = &jtThemes[0];

// t = 0 -> a, t = 255 -> b
static uint16_t jtBlend(uint16_t a, uint16_t b, int t) {
  if (t < 0) t = 0;
  if (t > 255) t = 255;
  int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
  int br = (b >> 11) & 31, bgc = (b >> 5) & 63, bb = b & 31;
  int r = ar + ((br - ar) * t) / 255;
  int g = ag + ((bgc - ag) * t) / 255;
  int l = ab + ((bb - ab) * t) / 255;
  return (uint16_t)((r << 11) | (g << 5) | l);
}

static int jtLum(uint16_t c) {
  int r = ((c >> 11) & 31) * 8, g = ((c >> 5) & 63) * 4, b = (c & 31) * 8;
  return (r * 30 + g * 59 + b * 11) / 100;
}

static void gText(const char* s, int x, int y, uint8_t size, uint16_t fg) {
  tft.setTextSize(size); tft.setTextColor(fg); tft.setCursor(x, y); tft.print(s);
}
static void gTextC(const char* s, int cx, int y, uint8_t size, uint16_t fg) {
  gText(s, cx - ((int)strlen(s) * 6 * size) / 2, y, size, fg);
}
static void gTextBg(const char* s, int x, int y, uint16_t fg, uint16_t bg) {
  tft.setTextSize(1); tft.setTextColor(fg, bg); tft.setCursor(x, y); tft.print(s);
}

static void gHeart(int x, int y, uint16_t c) {
  tft.fillCircle(x + 2, y + 2, 2, c);
  tft.fillCircle(x + 6, y + 2, 2, c);
  tft.fillTriangle(x, y + 3, x + 8, y + 3, x + 4, y + 8, c);
}

static void gClear(int x, int y, int w, int h, int topLimit, uint16_t col) {
  int W = tft.width(), H = tft.height();
  if (x < 0) { w += x; x = 0; }
  if (y < topLimit) { h -= (topLimit - y); y = topLimit; }
  if (x + w > W) w = W - x;
  if (y + h > H) h = H - y;
  if (w > 0 && h > 0) tft.fillRect(x, y, w, h, col);
}

// ---------------- Options ----------------
struct GameOpt {
  const char* name;
  const char* labels[4];
  uint8_t n;     // number of labels in use
  uint8_t val;   // current value (default value when declared)
};

static uint32_t gameBest(const char* key) {
  char k[12]; snprintf(k, sizeof(k), "%sH", key);
  return prefs.getUInt(k, 0);
}
static void gameSaveBest(const char* key, uint32_t v) {
  char k[12]; snprintf(k, sizeof(k), "%sH", key);
  prefs.putUInt(k, v);
}
static void gameLoadOpts(const char* key, GameOpt* o, int n) {
  for (int i = 0; i < n; i++) {
    char k[12]; snprintf(k, sizeof(k), "%s%d", key, i);
    uint8_t v = prefs.getUChar(k, o[i].val);
    if (v < o[i].n) o[i].val = v;
  }
}
static void gameSaveOpts(const char* key, GameOpt* o, int n) {
  for (int i = 0; i < n; i++) {
    char k[12]; snprintf(k, sizeof(k), "%s%d", key, i);
    prefs.putUChar(k, o[i].val);
  }
}

static void gameFlushButtons() {
  backPressed(); joyPressed(); rotatePressed();
}

// Options screen. Returns true = PLAY, false = leave.
static bool gameSetupImpl(const char* title, const char* tip, uint16_t accent,
                           const char* key, GameOpt* o, int n) {
  gameLoadOpts(key, o, n);
  const uint16_t SB = JT_RGB(8, 10, 22);
  int W = 0, H = 0;
  const int top = 48;
  int sel = n;   // start on PLAY
  uint32_t best = gameBest(key);

  auto drawRow = [&](int i) {
    int y = top + i * 15;
    bool on = (i == sel);
    fastFillRect(0, y, W, 15, SB);
    if (i == n) {
      uint16_t f = on ? accent : jtBlend(SB, accent, 70);
      tft.fillRoundRect(6, y, W - 12, 14, 4, f);
      gTextC("PLAY", W / 2, y + 3, 1, on ? (uint16_t)ST77XX_BLACK : (uint16_t)ST77XX_WHITE);
    } else {
      if (on) {
        tft.fillRoundRect(4, y, W - 8, 14, 4, jtBlend(SB, accent, 60));
        tft.drawRoundRect(4, y, W - 8, 14, 4, accent);
      }
      gText(o[i].name, 9, y + 3, 1, on ? (uint16_t)ST77XX_WHITE : (uint16_t)JT_LIGHTGREY);
      char buf[20];
      snprintf(buf, sizeof(buf), on ? "< %s >" : "%s", o[i].labels[o[i].val]);
      gText(buf, W - 9 - (int)strlen(buf) * 6, y + 3, 1, on ? accent : (uint16_t)ST77XX_WHITE);
    }
  };

  auto drawAll = [&]() {
    W = tft.width(); H = tft.height();
    jtFillScreen(SB);
    for (int y = 0; y < 40; y += 4)
      fastFillRect(0, y, W, 4, jtBlend(jtBlend(SB, accent, 90), SB, y * 255 / 40));
    int sz = ((int)strlen(title) * 12 <= W - 4) ? 2 : 1;
    gTextC(title, W / 2, 7, sz, ST77XX_WHITE);
    fastFillRect(W / 2 - 30, 26, 60, 2, accent);
    gTextC(tip, W / 2, 30, 1, JT_LIGHTGREY);
    char b[24]; snprintf(b, sizeof(b), "BEST %lu", (unsigned long)best);
    gTextC(b, W / 2, 39, 1, JT_GOLD);
    for (int i = 0; i <= n; i++) drawRow(i);
    if (H >= 150) gTextC("SW Play   BACK Exit", W / 2, H - 10, 1, jtBlend(SB, JT_RGB(255,255,255), 120));
  };

  drawAll();
  gameFlushButtons();
  unsigned long lastMove = 0;

  while (true) {
    if (backPressed()) return false;
    if (rotatePressed()) { changeRotation(); drawAll(); }
    JoyDir d = readJoyDirection();
    if ((d.x != 0 || d.y != 0) && millis() - lastMove > 170) {
      lastMove = millis();
      int old = sel;
      if (d.y != 0) {
        sel = (sel + d.y + n + 1) % (n + 1);
        drawRow(old); drawRow(sel);
      } else if (sel < n) {
        o[sel].val = (o[sel].val + d.x + o[sel].n) % o[sel].n;
        drawRow(sel);
      }
    }
    if (joyPressed()) {
      gameSaveOpts(key, o, n);
      randomSeed(micros() ^ (uint32_t)analogRead(JOY_X));
      return true;
    }
    delay(10);
  }
}

// Options screen drawn through the off-screen canvas, so moving the highlight never flickers.
static bool gameSetup(const char* title, const char* tip, uint16_t accent,
                      const char* key, GameOpt* o, int n) {
  gFrameBegin();
  bool r = gameSetupImpl(title, tip, accent, key, o, n);
  gFrameEnd();
  return r;
}

// ---------------- Game over ----------------
static void gOverDraw(const char* title, uint16_t accent, uint32_t score, uint32_t best, bool nb) {
  int W = tft.width(), H = tft.height();
  int pw = W - 12, ph = 92, x = 6, y = (H - ph) / 2;
  tft.fillRoundRect(x, y, pw, ph, 6, JT_RGB(10, 12, 28));
  tft.drawRoundRect(x, y, pw, ph, 6, accent);
  tft.drawRoundRect(x + 1, y + 1, pw - 2, ph - 2, 5, jtBlend(accent, 0, 150));
  gTextC("GAME OVER", W / 2, y + 8, 2, JT_RGB(255, 70, 70));
  gTextC(title, W / 2, y + 28, 1, accent);
  char b[28];
  snprintf(b, sizeof(b), "Score %lu", (unsigned long)score);
  gTextC(b, W / 2, y + 40, 1, ST77XX_WHITE);
  snprintf(b, sizeof(b), "Best  %lu", (unsigned long)best);
  gTextC(b, W / 2, y + 52, 1, JT_LIGHTGREY);
  if (nb) gTextC("NEW BEST!", W / 2, y + 64, 1, JT_GOLD);
  gTextC("SW again  BACK menu", W / 2, y + 77, 1, accent);
}

// Returns true = play again, false = back to options
static bool gameOverScreen(const char* title, uint16_t accent, uint32_t score, const char* key) {
  uint32_t best = gameBest(key);
  bool nb = false;
  if (score > best) { best = score; nb = true; gameSaveBest(key, best); }
  gOverDraw(title, accent, score, best, nb);
  delay(450);
  gameFlushButtons();
  while (true) {
    if (backPressed()) return false;
    if (joyPressed()) return true;
    if (rotatePressed()) {
      changeRotation();
      jtFillScreen(JT_RGB(4, 4, 10));
      gOverDraw(title, accent, score, best, nb);
    }
    delay(10);
  }
}

// ============================================================================
//  1. BLOCK FALL
// ============================================================================
static const uint16_t BF_SHAPES[7][4] = {
  { 0x0F00, 0x2222, 0x00F0, 0x4444 },   // I
  { 0x6600, 0x6600, 0x6600, 0x6600 },   // O
  { 0x4E00, 0x4640, 0x0E40, 0x4C40 },   // T
  { 0x6C00, 0x4620, 0x06C0, 0x8C40 },   // S
  { 0xC600, 0x2640, 0x0C60, 0x4C80 },   // Z
  { 0x8E00, 0x6440, 0x0E20, 0x44C0 },   // J
  { 0x2E00, 0x4460, 0x0E80, 0xC440 }    // L
};

static void bfBlock(int x, int y, int sz, uint16_t c) {
  tft.fillRect(x, y, sz, sz, c);
  uint16_t hi = jtBlend(c, 0xFFFF, 130), lo = jtBlend(c, 0x0000, 150);
  tft.drawFastHLine(x, y, sz, hi);
  tft.drawFastVLine(x, y, sz, hi);
  tft.drawFastHLine(x, y + sz - 1, sz, lo);
  tft.drawFastVLine(x + sz - 1, y, sz, lo);
  if (sz >= 7) tft.drawPixel(x + 2, y + 2, 0xFFFF);
}

static long playBlockFall(const GameOpt* o) {
  const int COLS = 10, ROWS = 20;
  jtTh = &jtThemes[o[2].val];
  const bool ghostOn = (o[1].val == 0);
  const int startLevel = o[0].val * 3;
  const uint16_t bg = jtTh->bg;
  uint16_t col[8];
  col[0] = bg;
  col[1] = jtTh->c1; col[2] = jtTh->c3; col[3] = jtTh->c2; col[4] = jtTh->c4;
  col[5] = jtBlend(jtTh->c1, jtTh->c2, 128);
  col[6] = jtBlend(jtTh->c3, jtTh->c2, 128);
  col[7] = jtBlend(jtTh->c1, jtTh->c4, 128);

  uint8_t board[ROWS][COLS];
  uint8_t drawn[ROWS][COLS];
  memset(board, 0, sizeof(board));
  int W = 0, H = 0, s = 8, bx = 1, by = 0, px0 = 0;
  uint32_t score = 0;
  int lines = 0, level = startLevel;
  uint32_t best = gameBest("bf");
  int bag[7], bagPos = 7;

  auto nextFromBag = [&]() -> int {
    if (bagPos >= 7) {
      for (int i = 0; i < 7; i++) bag[i] = i;
      for (int i = 6; i > 0; i--) { int j = random(i + 1); int t = bag[i]; bag[i] = bag[j]; bag[j] = t; }
      bagPos = 0;
    }
    return bag[bagPos++];
  };

  int cur = nextFromBag(), nxt = nextFromBag(), rot = 0, cx = 3, cy = -1;

  auto fits = [&](int p, int r, int px, int py) -> bool {
    uint16_t m = BF_SHAPES[p][r];
    for (int i = 0; i < 16; i++) {
      if (m & (0x8000 >> i)) {
        int x = px + (i & 3), y = py + (i >> 2);
        if (x < 0 || x >= COLS || y >= ROWS) return false;
        if (y >= 0 && board[y][x]) return false;
      }
    }
    return true;
  };

  uint32_t lastScore = 0xFFFFFFFF; int lastLines = -1, lastLevel = -1; int lastNext = -1;

  auto drawStat = [&](int idx, const char* label, uint32_t v, uint16_t c) {
    int y = by + 12 + (4 * (s - 2 < 3 ? 3 : s - 2) + 4) + 8 + idx * 20;
    gText(label, px0, y, 1, jtBlend(bg, jtTh->c1, 170));
    char b[12]; snprintf(b, sizeof(b), "%-6lu", (unsigned long)v);
    gTextBg(b, px0, y + 9, c, bg);
  };

  auto drawPreview = [&]() {
    int ps = (s - 2 < 3) ? 3 : s - 2;
    int bw = 4 * ps + 4;
    int ty = by + 12;
    tft.fillRoundRect(px0 - 1, ty - 1, bw + 2, bw + 2, 3, jtBlend(bg, jtTh->c1, 30));
    tft.drawRoundRect(px0 - 1, ty - 1, bw + 2, bw + 2, 3, jtBlend(bg, jtTh->c1, 120));
    uint16_t m = BF_SHAPES[nxt][0];
    int minC = 4, maxC = -1, minR = 4, maxR = -1;
    for (int i = 0; i < 16; i++) if (m & (0x8000 >> i)) {
      int c = i & 3, r = i >> 2;
      if (c < minC) minC = c; if (c > maxC) maxC = c;
      if (r < minR) minR = r; if (r > maxR) maxR = r;
    }
    int ow = (maxC - minC + 1) * ps, oh = (maxR - minR + 1) * ps;
    int ox = px0 + (bw - ow) / 2, oy = ty + (bw - oh) / 2;
    for (int i = 0; i < 16; i++) if (m & (0x8000 >> i)) {
      bfBlock(ox + ((i & 3) - minC) * ps, oy + ((i >> 2) - minR) * ps, ps, col[nxt + 1]);
    }
  };

  auto drawStats = [&](bool force) {
    if (force || score != lastScore) { drawStat(0, "SCORE", score, ST77XX_WHITE); lastScore = score; }
    if (force || lines != lastLines) { drawStat(1, "LINES", lines, jtTh->c1); lastLines = lines; }
    if (force || level != lastLevel) { drawStat(2, "LEVEL", level + 1, jtTh->c3); lastLevel = level; }
    if (force) drawStat(3, "BEST", best > score ? best : score, JT_GOLD);
    else if (score > best) { best = score; drawStat(3, "BEST", best, JT_GOLD); }
    if (force || nxt != lastNext) { drawPreview(); lastNext = nxt; }
  };

  auto drawAll = [&]() {
    W = tft.width(); H = tft.height();
    s = H / ROWS; if (s < 4) s = 4;
    bx = 1; by = (H - ROWS * s) / 2; px0 = bx + COLS * s + 6;
    jtFillScreen(bg);
    tft.drawRect(bx - 1, by - 1, COLS * s + 2, ROWS * s + 2, jtBlend(bg, jtTh->c1, 150));
    tft.drawFastVLine(bx + COLS * s + 2, by - 1, ROWS * s + 2, jtBlend(bg, jtTh->c2, 120));
    gText("NEXT", px0, by + 2, 1, jtBlend(bg, jtTh->c1, 170));
    memset(drawn, 0xFF, sizeof(drawn));
    drawStats(true);
  };

  auto drawCell = [&](int c, int r, uint8_t v) {
    int x = bx + c * s, y = by + r * s;
    if (v == 0) {
      tft.fillRect(x, y, s, s, bg);
      tft.drawPixel(x + s / 2, y + s / 2, jtBlend(bg, jtTh->c1, 55));
    } else if (v & 0x40) {
      tft.fillRect(x, y, s, s, bg);
      tft.drawRect(x, y, s, s, jtBlend(bg, col[v & 7], 130));
    } else {
      bfBlock(x, y, s, col[v]);
    }
  };

  uint8_t view[ROWS][COLS];
  auto render = [&]() {
    memcpy(view, board, sizeof(view));
    if (ghostOn) {
      int gy = cy;
      while (fits(cur, rot, cx, gy + 1)) gy++;
      if (gy != cy) {
        uint16_t m = BF_SHAPES[cur][rot];
        for (int i = 0; i < 16; i++) if (m & (0x8000 >> i)) {
          int x = cx + (i & 3), y = gy + (i >> 2);
          if (y >= 0 && y < ROWS && x >= 0 && x < COLS && !view[y][x]) view[y][x] = 0x40 | (cur + 1);
        }
      }
    }
    uint16_t m = BF_SHAPES[cur][rot];
    for (int i = 0; i < 16; i++) if (m & (0x8000 >> i)) {
      int x = cx + (i & 3), y = cy + (i >> 2);
      if (y >= 0 && y < ROWS && x >= 0 && x < COLS) view[y][x] = cur + 1;
    }
    for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) {
      if (view[r][c] != drawn[r][c]) { drawCell(c, r, view[r][c]); drawn[r][c] = view[r][c]; }
    }
  };

  // Lock piece, clear lines, spawn next. Returns false when the stack topped out.
  auto lockPiece = [&]() -> bool {
    uint16_t m = BF_SHAPES[cur][rot];
    for (int i = 0; i < 16; i++) if (m & (0x8000 >> i)) {
      int x = cx + (i & 3), y = cy + (i >> 2);
      if (y >= 0 && y < ROWS && x >= 0 && x < COLS) board[y][x] = cur + 1;
    }
    int full[4], nf = 0;
    for (int r = 0; r < ROWS && nf < 4; r++) {
      bool all = true;
      for (int c = 0; c < COLS; c++) if (!board[r][c]) { all = false; break; }
      if (all) full[nf++] = r;
    }
    if (nf) {
      render();   // show the locked piece first
      for (int k = 0; k < 3; k++) {
        uint16_t fc = (k & 1) ? jtBlend(jtTh->c1, 0xFFFF, 100) : (uint16_t)0xFFFF;
        for (int j = 0; j < nf; j++) tft.fillRect(bx, by + full[j] * s, COLS * s, s, fc);
        delay(70);
      }
      for (int k = 0; k < nf; k++) {
        int r = full[k];
        for (int rr = r; rr > 0; rr--) memcpy(board[rr], board[rr - 1], COLS);
        memset(board[0], 0, COLS);
      }
      static const int pts[5] = { 0, 100, 300, 500, 800 };
      score += pts[nf] * (level + 1);
      lines += nf;
      level = startLevel + lines / 10;
      memset(drawn, 0xFF, sizeof(drawn));
    }
    cur = nxt; nxt = nextFromBag(); rot = 0; cx = 3; cy = -1;
    return fits(cur, rot, cx, cy);
  };

  drawAll();
  gameFlushButtons();
  unsigned long lastFall = millis(), lastX = 0, moveStart = 0;
  int moveDir = 0;
  bool prevUp = false;

  while (true) {
    if (backPressed()) return -1;
    if (rotatePressed()) { changeRotation(); drawAll(); }

    JoyDir d = readJoyDirection();
    unsigned long now = millis();

    if (d.x != 0) {
      if (d.x != moveDir) {
        moveDir = d.x; moveStart = now; lastX = now;
        if (fits(cur, rot, cx + moveDir, cy)) cx += moveDir;
      } else if (now - moveStart > 200 && now - lastX > 70) {
        lastX = now;
        if (fits(cur, rot, cx + moveDir, cy)) cx += moveDir;
      }
    } else moveDir = 0;

    if (d.y < 0) {
      if (!prevUp) {
        prevUp = true;
        int nr = (rot + 1) & 3;
        static const int kicks[5] = { 0, -1, 1, -2, 2 };
        for (int k = 0; k < 5; k++) {
          if (fits(cur, nr, cx + kicks[k], cy)) { rot = nr; cx += kicks[k]; break; }
        }
      }
    } else prevUp = false;

    bool dead = false;
    if (joyPressed()) {                       // hard drop
      while (fits(cur, rot, cx, cy + 1)) { cy++; score += 2; }
      if (!lockPiece()) dead = true;
      lastFall = millis();
    }

    int gravity = 760 - level * 62; if (gravity < 80) gravity = 80;
    int iv = (d.y > 0) ? (gravity < 40 ? gravity : 40) : gravity;
    if (!dead && now - lastFall >= (unsigned long)iv) {
      lastFall = now;
      if (fits(cur, rot, cx, cy + 1)) { cy++; if (d.y > 0) score += 1; }
      else if (!lockPiece()) dead = true;
    }

    render();
    drawStats(false);

    if (dead) {
      for (int r = ROWS - 1; r >= 0; r--) {
        tft.fillRect(bx, by + r * s, COLS * s, s, jtBlend(bg, 0xFFFF, 70));
        delay(18);
      }
      return (long)score;
    }
    delay(8);
  }
}

static void gameBlockFall() {
  GameOpt o[3] = {
    { "Level", { "1", "4", "7", "10" }, 4, 0 },
    { "Ghost", { "On", "Off" }, 2, 0 },
    { "Theme", { "Neon", "Retro", "Sunset", "Ocean" }, 4, 0 }
  };
  while (true) {
    if (!gameSetup("BLOCK FALL", "Up rotate  SW drop", ST77XX_CYAN, "bf", o, 3)) return;
    while (true) {
      long sc = gRun(playBlockFall, o);
      if (sc < 0) break;
      if (!gameOverScreen("Block Fall", jtTh->c1, (uint32_t)sc, "bf")) break;
    }
  }
}

// ============================================================================
//  2. NEON SNAKE
// ============================================================================
static long playNeonSnake(const GameOpt* o) {
  jtTh = &jtThemes[o[3].val];
  static const int stepMs[4] = { 170, 125, 90, 62 };
  const int baseStep = stepMs[o[0].val];
  const bool wrap = (o[1].val == 1);
  const int rockLevel = o[2].val;
  const uint16_t bg = jtTh->bg;
  const uint16_t bgB = jtBlend(bg, jtTh->c1, 16);
  const uint16_t headCol = jtBlend(jtTh->c1, 0xFFFF, 90);
  const int C = 8, HUD = 16;

  int W = 0, H = 0, cols = 0, rows = 0, ox = 0, oy = 0;
  uint8_t sx[400], sy[400];
  bool rock[20][20];
  int len = 0, dx = 1, dy = 0, ndx = 1, ndy = 0;
  int fx = 0, fy = 0;
  bool bonusOn = false; int bnx = 0, bny = 0; unsigned long bonusEnd = 0;
  int eaten = 0;
  uint32_t score = 0;
  uint32_t best = gameBest("ns");
  bool paused = false;

  auto segColor = [&](int i) -> uint16_t {
    if (i == 0) return headCol;
    return jtBlend(jtTh->c1, jtTh->c2, len > 1 ? i * 255 / (len - 1) : 0);
  };
  auto bgCell = [&](int x, int y) {
    tft.fillRect(ox + x * C, oy + y * C, C, C, ((x + y) & 1) ? bgB : bg);
  };
  auto drawSeg = [&](int i) {
    int x = ox + sx[i] * C, y = oy + sy[i] * C;
    tft.fillRoundRect(x + 1, y + 1, C - 2, C - 2, 2, segColor(i));
    if (i == 0) {
      int e1x, e1y, e2x, e2y;
      if (dx != 0) { e1x = e2x = x + (dx > 0 ? 5 : 1); e1y = y + 2; e2y = y + 5; }
      else { e1y = e2y = y + (dy > 0 ? 5 : 1); e1x = x + 2; e2x = x + 5; }
      tft.fillRect(e1x, e1y, 2, 2, bg);
      tft.fillRect(e2x, e2y, 2, 2, bg);
    }
  };
  auto drawRock = [&](int x, int y) {
    int px = ox + x * C, py = oy + y * C;
    tft.fillRoundRect(px + 1, py + 1, C - 2, C - 2, 2, JT_RGB(90, 96, 120));
    tft.drawFastHLine(px + 2, py + 2, C - 4, JT_RGB(150, 156, 180));
    tft.drawFastHLine(px + 2, py + C - 3, C - 4, JT_RGB(50, 54, 70));
  };
  auto drawFood = [&](int phase) {
    bgCell(fx, fy);
    int cx = ox + fx * C + C / 2, cy = oy + fy * C + C / 2;
    if (phase) { tft.fillCircle(cx, cy, 2, jtTh->c3); tft.drawCircle(cx, cy, 3, jtBlend(bg, jtTh->c3, 110)); }
    else tft.fillCircle(cx, cy, 3, jtTh->c3);
    tft.drawPixel(cx - 1, cy - 1, 0xFFFF);
  };
  auto drawBonus = [&](bool on) {
    bgCell(bnx, bny);
    if (!on) return;
    int x = ox + bnx * C, y = oy + bny * C;
    tft.fillRect(x + 3, y + 1, 2, 6, jtTh->c4);
    tft.fillRect(x + 1, y + 3, 6, 2, jtTh->c4);
    tft.fillRect(x + 3, y + 3, 2, 2, 0xFFFF);
  };
  auto drawHud = [&]() {
    tft.fillRect(0, 0, W, HUD - 1, jtBlend(bg, jtTh->c1, 40));
    tft.drawFastHLine(0, HUD - 1, W, jtTh->c1);
    char b[20];
    snprintf(b, sizeof(b), "SC %lu", (unsigned long)score);
    gText(b, 3, 4, 1, ST77XX_WHITE);
    snprintf(b, sizeof(b), "BEST %lu", (unsigned long)(best > score ? best : score));
    gText(b, W - 3 - (int)strlen(b) * 6, 4, 1, JT_GOLD);
    if (bonusOn) {   // bonus timer bar
      long left = (long)(bonusEnd - millis());
      if (left < 0) left = 0;
      tft.fillRect(W / 2 - 8, 12, (int)(16 * left / 6000), 2, jtTh->c4);
    }
  };
  auto drawField = [&]() {
    jtFillScreen(bg);
    fastFillRect(0, HUD, W, H - HUD, bg);
    for (int y = 0; y < rows; y++) for (int x = 0; x < cols; x++) if ((x + y) & 1) bgCell(x, y);
    tft.drawRect(ox - 1, oy - 1, cols * C + 2, rows * C + 2,
                 wrap ? jtBlend(bg, jtTh->c4, 130) : jtBlend(bg, jtTh->c1, 170));
    for (int y = 0; y < rows; y++) for (int x = 0; x < cols; x++) if (rock[y][x]) drawRock(x, y);
    drawFood(0);
    if (bonusOn) drawBonus(true);
    for (int i = len - 1; i >= 0; i--) drawSeg(i);
    drawHud();
  };
  auto occupied = [&](int x, int y) -> bool {
    if (rock[y][x]) return true;
    for (int i = 0; i < len; i++) if (sx[i] == x && sy[i] == y) return true;
    if (x == fx && y == fy) return true;
    if (bonusOn && x == bnx && y == bny) return true;
    return false;
  };
  auto placeFood = [&]() {
    for (int tries = 0; tries < 200; tries++) {
      int x = random(cols), y = random(rows);
      if (!occupied(x, y)) { fx = x; fy = y; return; }
    }
  };

  auto newRound = [&]() {
    W = tft.width(); H = tft.height();
    cols = (W - 4) / C; rows = (H - HUD - 4) / C;
    if (cols > 19) cols = 19;
    if (rows > 17) rows = 17;
    ox = (W - cols * C) / 2;
    oy = HUD + ((H - HUD) - rows * C) / 2;
    memset(rock, 0, sizeof(rock));
    len = 4; dx = 1; dy = 0; ndx = 1; ndy = 0;
    int mid = rows / 2;
    for (int i = 0; i < len; i++) { sx[i] = cols / 2 - i; sy[i] = mid; }
    int nr = (rockLevel == 1) ? cols * rows / 28 : (rockLevel == 2) ? cols * rows / 13 : 0;
    for (int k = 0; k < nr; k++) {
      for (int tries = 0; tries < 50; tries++) {
        int x = random(cols), y = random(rows);
        if (abs(y - mid) <= 1) continue;
        if (x == 0 || y == 0 || x == cols - 1 || y == rows - 1) { if (random(3)) continue; }
        if (!rock[y][x]) { rock[y][x] = true; break; }
      }
    }
    fx = -1; fy = -1;
    bonusOn = false; eaten = 0; score = 0; paused = false;
    placeFood();
    drawField();
  };

  newRound();
  gameFlushButtons();
  unsigned long lastStep = millis(), lastPulse = 0, lastBlink = 0;
  int phase = 0; bool bblink = false;

  while (true) {
    if (backPressed()) return -1;
    if (rotatePressed()) { changeRotation(); newRound(); lastStep = millis(); }

    JoyDir d = readJoyDirection();
    if (!paused) {
      if (d.x != 0 && dx == 0) { ndx = d.x; ndy = 0; }
      else if (d.y != 0 && dy == 0) { ndx = 0; ndy = d.y; }
    }
    if (joyPressed()) {
      paused = !paused;
      if (paused) {
        int bw = 70;
        tft.fillRoundRect(W / 2 - bw / 2, H / 2 - 12, bw, 24, 5, JT_RGB(10, 12, 28));
        tft.drawRoundRect(W / 2 - bw / 2, H / 2 - 12, bw, 24, 5, jtTh->c1);
        gTextC("PAUSED", W / 2, H / 2 - 4, 1, jtTh->c3);
      } else { drawField(); lastStep = millis(); }
    }

    unsigned long now = millis();
    if (!paused) {
      // food pulse + bonus blink
      if (now - lastPulse > 260) { lastPulse = now; phase ^= 1; drawFood(phase); }
      if (bonusOn) {
        if ((long)(now - bonusEnd) > 0) { bonusOn = false; drawBonus(false); drawHud(); }
        else if (now - lastBlink > 140) { lastBlink = now; bblink = !bblink; drawBonus(bblink || true); if (bblink) tft.drawRect(ox + bnx * C, oy + bny * C, C, C, jtTh->c4); }
      }

      int interval = baseStep - (eaten * 2 > baseStep / 3 ? baseStep / 3 : eaten * 2);
      if (now - lastStep >= (unsigned long)interval) {
        lastStep = now;
        dx = ndx; dy = ndy;
        int nx = sx[0] + dx, ny = sy[0] + dy;
        bool dead = false;
        if (wrap) { nx = (nx + cols) % cols; ny = (ny + rows) % rows; }
        else if (nx < 0 || ny < 0 || nx >= cols || ny >= rows) dead = true;
        if (!dead && rock[ny][nx]) dead = true;
        bool eat = (!dead && nx == fx && ny == fy);
        bool eatBonus = (!dead && bonusOn && nx == bnx && ny == bny);
        if (!dead) {
          int chk = eat ? len : len - 1;
          for (int i = 0; i < chk; i++) if (sx[i] == nx && sy[i] == ny) { dead = true; break; }
        }
        if (dead) {
          for (int k = 0; k < 4; k++) {
            for (int i = 0; i < len; i++) {
              int x = ox + sx[i] * C, y = oy + sy[i] * C;
              tft.fillRoundRect(x + 1, y + 1, C - 2, C - 2, 2, (k & 1) ? jtTh->c2 : (uint16_t)0xFFFF);
            }
            delay(110);
          }
          return (long)score;
        }
        int tailX = sx[len - 1], tailY = sy[len - 1];
        if (!eat && !eatBonus) bgCell(tailX, tailY);
        if (eat || eatBonus) { if (len < 399) len++; }
        for (int i = len - 1; i > 0; i--) { sx[i] = sx[i - 1]; sy[i] = sy[i - 1]; }
        sx[0] = nx; sy[0] = ny;
        for (int i = len - 1; i >= 0; i--) drawSeg(i);
        if (eat) {
          eaten++; score += 10;
          placeFood(); drawFood(0);
          if (!bonusOn && eaten % 4 == 0) {
            for (int tries = 0; tries < 100; tries++) {
              int x = random(cols), y = random(rows);
              if (!occupied(x, y)) { bnx = x; bny = y; bonusOn = true; bonusEnd = millis() + 6000; drawBonus(true); break; }
            }
          }
          drawHud();
        } else if (eatBonus) {
          score += 50; bonusOn = false;
          for (int i = len - 1; i >= 0; i--) drawSeg(i);
          drawHud();
        }
      }
    }
    delay(4);
  }
}

static void gameNeonSnake() {
  GameOpt o[4] = {
    { "Speed", { "Slow", "Normal", "Fast", "Insane" }, 4, 1 },
    { "Walls", { "Solid", "Wrap" }, 2, 0 },
    { "Rocks", { "Off", "Few", "Many" }, 3, 0 },
    { "Theme", { "Neon", "Retro", "Sunset", "Ocean" }, 4, 0 }
  };
  while (true) {
    if (!gameSetup("NEON SNAKE", "SW pause  Rotate=restart", ST77XX_GREEN, "ns", o, 4)) return;
    while (true) {
      long sc = gRun(playNeonSnake, o);
      if (sc < 0) break;
      if (!gameOverScreen("Neon Snake", jtTh->c1, (uint32_t)sc, "ns")) break;
    }
  }
}

// ============================================================================
//  3. STAR RAID
// ============================================================================
struct SrStar   { int16_t x, y; uint8_t spd; };
struct SrBullet { float x, y, vx, vy; bool on; uint8_t p; };
struct SrEnemy  { float x, y, bx, vx, vy; int16_t hp; uint8_t type; bool on; uint16_t t; };
struct SrPow    { float x, y; uint8_t type; bool on; };
struct SrExp    { int16_t x, y; uint8_t age; bool on; };

static long playStarRaid(const GameOpt* o) {
  jtTh = &jtThemes[o[2].val];
  const uint16_t bg = jtTh->bg;
  const int mode = o[0].val, shipT = o[1].val;
  static const int   livesStart[3] = { 5, 3, 2 };
  static const int   spawnGap[3]   = { 26, 20, 14 };
  static const float shootScale[3] = { 0.6f, 1.0f, 1.5f };
  static const int   hpBonus[3]    = { 0, 0, 1 };
  static const float shipSpd[3]    = { 3.0f, 4.2f, 2.2f };
  static const int   shipCd[3]     = { 8, 10, 7 };
  static const int   shipPow[3]    = { 1, 1, 2 };
  const uint16_t shipCol = (shipT == 0) ? jtTh->c1 : (shipT == 1) ? jtTh->c3 : jtTh->c2;
  const int HUD = 12;
  const uint16_t RED = JT_RGB(255, 70, 50);

  int W = tft.width(), H = tft.height();
  SrStar stars[28];
  SrBullet pb[18], eb[14];
  SrEnemy en[9];
  SrPow pw[3];
  SrExp ex[8];
  memset(pb, 0, sizeof(pb)); memset(eb, 0, sizeof(eb)); memset(en, 0, sizeof(en));
  memset(pw, 0, sizeof(pw)); memset(ex, 0, sizeof(ex));

  float px = W / 2.0f, py = H - 24.0f;
  int lives = livesStart[mode], bombs = 1, wl = (shipT == 1) ? 2 : 1;
  int shield = 0, invuln = 0, fireCd = 0;
  uint32_t score = 0;
  int wave = 0, toSpawn = 0, spawnTimer = 0, banner = 0;
  bool bossWave = false;
  int bossMax = 1;
  uint32_t frame = 0;

  auto initStars = [&]() {
    for (int i = 0; i < 28; i++) {
      stars[i].x = random(W); stars[i].y = HUD + 2 + random(H - HUD - 2);
      stars[i].spd = 1 + (i % 3);
    }
  };
  auto relayout = [&]() {
    W = tft.width(); H = tft.height();
    if (px < 10) px = 10; if (px > W - 10) px = W - 10;
    if (py < H / 2) py = H / 2; if (py > H - 20) py = H - 20;
    for (int i = 0; i < 14; i++) eb[i].on = false;
    for (int i = 0; i < 18; i++) pb[i].on = false;
    for (int i = 0; i < 9; i++) en[i].on = false;
    for (int i = 0; i < 3; i++) pw[i].on = false;
    for (int i = 0; i < 8; i++) ex[i].on = false;
    toSpawn = 0; spawnTimer = 0;
    jtFillScreen(bg);
    initStars();
  };

  auto ehw = [&](int t) -> int { return t == 3 ? 16 : (t == 2 ? 7 : 6); };
  auto ehh = [&](int t) -> int { return t == 3 ? 20 : (t == 2 ? 12 : 9); };

  auto addExp = [&](int x, int y) {
    for (int i = 0; i < 8; i++) if (!ex[i].on) { ex[i].on = true; ex[i].x = x; ex[i].y = y; ex[i].age = 0; return; }
  };
  auto addPB = [&](float x, float y, float vx, int p) {
    for (int i = 0; i < 18; i++) if (!pb[i].on) { pb[i] = { x, y, vx, -6.0f, true, (uint8_t)p }; return; }
  };
  auto addEB = [&](float x, float y, float vx, float vy) {
    for (int i = 0; i < 14; i++) if (!eb[i].on) { eb[i] = { x, y, vx, vy, true, 0 }; return; }
  };
  auto addPow = [&](float x, float y) {
    for (int i = 0; i < 3; i++) if (!pw[i].on) {
      int r = random(100);
      pw[i].on = true; pw[i].x = x; pw[i].y = y;
      pw[i].type = (r < 40) ? 0 : (r < 65) ? 1 : (r < 85) ? 2 : 3;   // W S B +
      return;
    }
  };

  auto spawnEnemy = [&](int t, float x) {
    for (int i = 0; i < 9; i++) if (!en[i].on) {
      SrEnemy &e = en[i];
      e.on = true; e.type = t; e.x = x; e.bx = x; e.y = HUD + 3; e.t = random(60);
      float sp = 0.9f + wave * 0.06f; if (sp > 2.0f) sp = 2.0f;
      e.vy = sp; e.vx = (random(2) ? 1.0f : -1.0f);
      if (t == 0) e.hp = 1;
      else if (t == 1) e.hp = 1;
      else if (t == 2) { e.hp = 3 + wave / 4 + hpBonus[mode]; e.vy = 0.6f; }
      else { e.hp = bossMax; e.vy = 0.8f; e.vx = 1.0f; }
      return;
    }
  };

  auto startWave = [&]() {
    wave++;
    bossWave = (wave % 5 == 0);
    toSpawn = bossWave ? 1 : (5 + wave * 2 > 22 ? 22 : 5 + wave * 2);
    bossMax = 24 + wave * 3 + hpBonus[mode] * 8;
    spawnTimer = 40;
    banner = 45;
  };

  auto killEnemy = [&](SrEnemy &e) {
    e.on = false;
    addExp((int)e.x, (int)e.y + ehh(e.type) / 2);
    if (e.type == 3) { addExp((int)e.x - 10, (int)e.y + 6); addExp((int)e.x + 10, (int)e.y + 10); }
    static const int pts[4] = { 10, 20, 40, 300 };
    score += pts[e.type] + (e.type == 3 ? wave * 20 : 0);
    int chance = (e.type == 2) ? 35 : (e.type == 3 ? 100 : 9);
    if (random(100) < chance) addPow(e.x, e.y + 4);
  };

  auto damageEnemy = [&](SrEnemy &e, int dmg) {
    e.hp -= dmg;
    if (e.hp <= 0) killEnemy(e);
  };

  auto playerHit = [&]() {
    if (invuln > 0) return;
    if (shield > 0) { shield = 0; invuln = 25; return; }
    lives--;
    addExp((int)px, (int)py + 6);
    invuln = 70;
    if (wl > 1) wl--;
  };

  auto drawShip = [&](int cx, int y, bool flame) {
    int wg = (shipT == 2) ? 9 : (shipT == 1) ? 6 : 8;
    uint16_t c = shipCol, dk = jtBlend(c, 0, 120), lt = jtBlend(c, 0xFFFF, 120);
    tft.fillTriangle(cx - wg, y + 12, cx - 2, y + 5, cx - 2, y + 12, dk);
    tft.fillTriangle(cx + wg, y + 12, cx + 2, y + 5, cx + 2, y + 12, dk);
    tft.fillTriangle(cx, y, cx - 4, y + 12, cx + 4, y + 12, c);
    tft.fillRect(cx - 1, y + 4, 2, 4, lt);
    if (shipT == 2) { tft.fillRect(cx - wg, y + 6, 2, 6, lt); tft.fillRect(cx + wg - 1, y + 6, 2, 6, lt); }
    if (shipT == 1) { tft.drawLine(cx - wg, y + 12, cx - wg, y + 9, lt); tft.drawLine(cx + wg, y + 12, cx + wg, y + 9, lt); }
    if (flame) {
      tft.fillTriangle(cx - 2, y + 12, cx + 2, y + 12, cx, y + 17, JT_ORANGE);
      tft.fillTriangle(cx - 1, y + 12, cx + 1, y + 12, cx, y + 14, ST77XX_YELLOW);
    }
  };

  auto drawEnemy = [&](const SrEnemy &e) {
    int x = (int)e.x, y = (int)e.y;
    switch (e.type) {
      case 0: {
        uint16_t c = jtTh->c2;
        tft.fillRoundRect(x - 4, y, 9, 6, 2, c);
        tft.drawFastVLine(x - 3, y + 6, 2, c); tft.drawFastVLine(x + 3, y + 6, 2, c);
        tft.drawFastVLine(x - 5, y + 2, 3, c); tft.drawFastVLine(x + 5, y + 2, 3, c);
        tft.fillRect(x - 2, y + 2, 2, 2, bg); tft.fillRect(x + 1, y + 2, 2, 2, bg);
      } break;
      case 1: {
        uint16_t c = jtTh->c3;
        tft.fillTriangle(x, y + 8, x - 5, y, x + 5, y, c);
        tft.fillRect(x - 1, y + 1, 2, 3, bg);
        tft.drawFastHLine(x - 5, y, 11, jtBlend(c, 0xFFFF, 120));
      } break;
      case 2: {
        uint16_t c = jtTh->c4;
        tft.fillRoundRect(x - 6, y, 13, 8, 2, c);
        tft.fillRect(x - 1, y + 8, 3, 3, c);
        tft.fillRect(x - 4, y + 2, 9, 3, bg);
        for (int k = 0; k < e.hp && k < 5; k++) tft.fillRect(x - 4 + k * 2, y + 3, 1, 1, c);
      } break;
      default: {
        uint16_t c = jtTh->c2, c2 = jtTh->c1;
        tft.fillRoundRect(x - 14, y, 29, 10, 4, c);
        tft.fillTriangle(x - 15, y + 2, x - 15, y + 12, x - 8, y + 10, jtBlend(c, 0, 90));
        tft.fillTriangle(x + 15, y + 2, x + 15, y + 12, x + 8, y + 10, jtBlend(c, 0, 90));
        tft.fillRect(x - 12, y + 10, 3, 5, c2); tft.fillRect(x + 10, y + 10, 3, 5, c2);
        tft.fillCircle(x, y + 5, 3, (frame & 4) ? (uint16_t)0xFFFF : RED);
        tft.fillRect(x - 14, y + 17, 29, 2, jtBlend(bg, 0xFFFF, 60));
        int w = (int)(29L * e.hp / bossMax); if (w < 0) w = 0;
        tft.fillRect(x - 14, y + 17, w, 2, jtTh->c3);
      }
    }
  };

  auto drawHud = [&]() {
    tft.fillRect(0, 0, W, HUD, jtBlend(bg, jtTh->c1, 35));
    tft.drawFastHLine(0, HUD, W, jtBlend(bg, jtTh->c1, 140));
    char b[14];
    snprintf(b, sizeof(b), "%lu", (unsigned long)score);
    gText(b, 2, 2, 1, ST77XX_WHITE);
    snprintf(b, sizeof(b), "W%d", wave);
    gText(b, 44, 2, 1, jtTh->c3);
    snprintf(b, sizeof(b), "B%d", bombs);
    gText(b, 66, 2, 1, jtTh->c4);
    for (int i = 0; i < lives && i < 5; i++) gHeart(W - 3 - (i + 1) * 9, 2, RED);
  };

  relayout();
  startWave();
  gameFlushButtons();
  unsigned long lastFrame = 0;
  bool bombReq = false;
  const float spd = shipSpd[shipT];

  while (true) {
    if (backPressed()) return -1;
    if (rotatePressed()) { changeRotation(); relayout(); }
    if (joyPressed()) bombReq = true;
    if (millis() - lastFrame < 33) { delay(1); continue; }
    lastFrame = millis();
    frame++;
    JoyDir d = readJoyDirection();

    // -------- erase everything from its last position --------
    for (int i = 0; i < 28; i++) gClear(stars[i].x, stars[i].y, 1, stars[i].spd + 1, HUD + 1, bg);
    gClear((int)px - 12, (int)py - 5, 25, 25, HUD + 1, bg);
    for (int i = 0; i < 18; i++) if (pb[i].on) gClear((int)pb[i].x - 3, (int)pb[i].y - 1, 7, 8, HUD + 1, bg);
    for (int i = 0; i < 14; i++) if (eb[i].on) gClear((int)eb[i].x - 3, (int)eb[i].y - 3, 7, 7, HUD + 1, bg);
    for (int i = 0; i < 9; i++) if (en[i].on) gClear((int)en[i].x - ehw(en[i].type) - 1, (int)en[i].y - 1, 2 * ehw(en[i].type) + 3, ehh(en[i].type) + 2, HUD + 1, bg);
    for (int i = 0; i < 3; i++) if (pw[i].on) gClear((int)pw[i].x - 5, (int)pw[i].y - 5, 11, 11, HUD + 1, bg);
    for (int i = 0; i < 8; i++) if (ex[i].on) { int r = 2 + ex[i].age * 2 + 1; gClear(ex[i].x - r, ex[i].y - r, 2 * r + 1, 2 * r + 1, HUD + 1, bg); }
    if (banner > 0) gClear(0, H / 2 - 24, W, 36, HUD + 1, bg);

    // -------- update --------
    px += d.x * spd; py += d.y * spd;
    if (px < 10) px = 10; if (px > W - 10) px = W - 10;
    if (py < H / 2) py = H / 2; if (py > H - 20) py = H - 20;

    if (fireCd > 0) fireCd--;
    else {
      fireCd = shipCd[shipT];
      int p = shipPow[shipT];
      if (wl == 1) addPB(px, py, 0, p);
      else if (wl == 2) { addPB(px - 3, py + 2, 0, p); addPB(px + 3, py + 2, 0, p); }
      else { addPB(px, py, 0, p); addPB(px - 4, py + 3, -0.9f, p); addPB(px + 4, py + 3, 0.9f, p); }
    }
    if (invuln > 0) invuln--;
    if (shield > 0) shield--;

    if (bombReq) {
      bombReq = false;
      if (bombs > 0) {
        bombs--;
        for (int i = 0; i < 14; i++) eb[i].on = false;
        jtFillScreen(0xFFFF); delay(35);
        jtFillScreen(bg);
        for (int i = 0; i < 9; i++) if (en[i].on) damageEnemy(en[i], en[i].type == 3 ? 20 : 6);
      }
    }

    // stars
    for (int i = 0; i < 28; i++) {
      stars[i].y += stars[i].spd;
      if (stars[i].y >= H) { stars[i].y = HUD + 1; stars[i].x = random(W); }
    }

    // wave logic
    int alive = 0;
    for (int i = 0; i < 9; i++) if (en[i].on) alive++;
    if (toSpawn > 0) {
      if (--spawnTimer <= 0) {
        spawnTimer = spawnGap[mode] + random(8);
        int t;
        if (bossWave) t = 3;
        else {
          int r = random(100);
          t = (wave < 2) ? 0 : (wave < 3) ? (r < 60 ? 0 : 1) : (r < 45 ? 0 : (r < 80 ? 1 : 2));
        }
        spawnEnemy(t, 14 + random(W - 28));
        toSpawn--;
      }
    } else if (alive == 0) {
      startWave();
    }

    // enemies
    for (int i = 0; i < 9; i++) if (en[i].on) {
      SrEnemy &e = en[i];
      e.t++;
      if (e.type == 0) { e.y += e.vy; }
      else if (e.type == 1) { e.y += e.vy; e.x = e.bx + sinf(e.t * 0.11f) * 22.0f; if (e.x < 8) e.x = 8; if (e.x > W - 8) e.x = W - 8; }
      else if (e.type == 2) {
        if (e.y < H * 0.30f) e.y += e.vy;
        else { e.x += e.vx * 0.7f; if (e.x < 10 || e.x > W - 10) e.vx = -e.vx; }
        int iv = (int)(85 / shootScale[mode]);
        if (e.t % iv == 0) {
          float ddx = px - e.x, ddy = py - e.y; float dist = sqrtf(ddx * ddx + ddy * ddy); if (dist < 1) dist = 1;
          addEB(e.x, e.y + 11, ddx / dist * 2.2f, ddy / dist * 2.2f);
        }
      } else {
        if (e.y < HUD + 6) e.y += 0.8f;
        e.x += e.vx * 0.9f; if (e.x < 20 || e.x > W - 20) e.vx = -e.vx;
        int iv = (int)(38 / shootScale[mode]);
        if (e.t % iv == 0) { addEB(e.x, e.y + 14, -1.0f, 2.3f); addEB(e.x, e.y + 14, 0, 2.5f); addEB(e.x, e.y + 14, 1.0f, 2.3f); }
      }
      if (mode == 2 && e.type == 0 && e.t % 110 == 55) addEB(e.x, e.y + 8, 0, 2.2f);
      if (e.y > H) { e.on = false; continue; }
      // body collision with player
      if (e.type != 3 || true) {
        if (fabsf(e.x - px) < ehw(e.type) + 5 && e.y + ehh(e.type) > py && e.y < py + 12) {
          playerHit();
          if (e.type != 3) { killEnemy(e); }
        }
      }
    }

    // player bullets
    for (int i = 0; i < 18; i++) if (pb[i].on) {
      pb[i].x += pb[i].vx; pb[i].y += pb[i].vy;
      if (pb[i].y < HUD + 2 || pb[i].x < 0 || pb[i].x > W) { pb[i].on = false; continue; }
      for (int j = 0; j < 9; j++) if (en[j].on) {
        SrEnemy &e = en[j];
        if (fabsf(pb[i].x - e.x) <= ehw(e.type) && pb[i].y >= e.y && pb[i].y <= e.y + ehh(e.type)) {
          pb[i].on = false;
          damageEnemy(e, pb[i].p);
          break;
        }
      }
    }

    // enemy bullets
    for (int i = 0; i < 14; i++) if (eb[i].on) {
      eb[i].x += eb[i].vx; eb[i].y += eb[i].vy;
      if (eb[i].y > H || eb[i].y < HUD || eb[i].x < 0 || eb[i].x > W) { eb[i].on = false; continue; }
      if (fabsf(eb[i].x - px) <= 4 && eb[i].y >= py && eb[i].y <= py + 12) { eb[i].on = false; playerHit(); }
    }

    // power-ups
    for (int i = 0; i < 3; i++) if (pw[i].on) {
      pw[i].y += 1.1f;
      if (pw[i].y > H) { pw[i].on = false; continue; }
      if (fabsf(pw[i].x - px) < 10 && pw[i].y > py - 4 && pw[i].y < py + 14) {
        pw[i].on = false;
        switch (pw[i].type) {
          case 0: if (wl < 3) wl++; else score += 50; break;
          case 1: shield = 240; break;
          case 2: if (bombs < 5) bombs++; else score += 50; break;
          default: if (lives < 5) lives++; else score += 100; break;
        }
        score += 25;
      }
    }

    // explosions
    for (int i = 0; i < 8; i++) if (ex[i].on) { if (++ex[i].age > 6) ex[i].on = false; }

    // -------- game over --------
    if (lives <= 0) {
      for (int k = 0; k < 6; k++) { addExp((int)px + random(-8, 9), (int)py + random(0, 12)); }
      for (int a = 0; a < 7; a++) {
        for (int i = 0; i < 8; i++) if (ex[i].on) {
          int r = 2 + ex[i].age * 2;
          uint16_t c = (ex[i].age < 3) ? (uint16_t)ST77XX_YELLOW : (ex[i].age < 5) ? (uint16_t)JT_ORANGE : RED;
          tft.fillCircle(ex[i].x, ex[i].y, r, c);
          ex[i].age++; if (ex[i].age > 6) ex[i].on = false;
        }
        delay(70);
      }
      return (long)score;
    }

    // -------- draw --------
    for (int i = 0; i < 28; i++) {
      uint16_t c = (stars[i].spd == 3) ? (uint16_t)0xFFFF : (stars[i].spd == 2) ? jtBlend(bg, 0xFFFF, 170) : jtBlend(bg, 0xFFFF, 90);
      if (stars[i].y > HUD) { tft.drawPixel(stars[i].x, stars[i].y, c); if (stars[i].spd == 3) tft.drawPixel(stars[i].x, stars[i].y + 1, jtBlend(bg, 0xFFFF, 100)); }
    }
    for (int i = 0; i < 3; i++) if (pw[i].on) {
      static const char L[4] = { 'W', 'S', 'B', '+' };
      uint16_t c = (pw[i].type == 0) ? jtTh->c3 : (pw[i].type == 1) ? jtTh->c1 : (pw[i].type == 2) ? jtTh->c4 : RED;
      int x = (int)pw[i].x, y = (int)pw[i].y;
      tft.fillRoundRect(x - 4, y - 4, 9, 9, 2, c);
      tft.setTextSize(1); tft.setTextColor(bg); tft.setCursor(x - 2, y - 3); tft.print(L[pw[i].type]);
    }
    for (int i = 0; i < 9; i++) if (en[i].on) drawEnemy(en[i]);
    for (int i = 0; i < 14; i++) if (eb[i].on) {
      tft.fillCircle((int)eb[i].x, (int)eb[i].y, 2, RED);
      tft.drawPixel((int)eb[i].x - 1, (int)eb[i].y - 1, 0xFFFF);
    }
    for (int i = 0; i < 18; i++) if (pb[i].on) {
      int w = (pb[i].p > 1) ? 3 : 2;
      tft.fillRect((int)pb[i].x - w / 2, (int)pb[i].y, w, 5, jtTh->c4);
      tft.drawPixel((int)pb[i].x, (int)pb[i].y, 0xFFFF);
    }
    if (!(invuln > 0 && (frame & 2))) {
      drawShip((int)px, (int)py, (frame & 1));
      if (shield > 0 && (shield > 60 || (frame & 2)))
        tft.drawCircle((int)px, (int)py + 7, 11, jtTh->c1);
    }
    for (int i = 0; i < 8; i++) if (ex[i].on) {
      int r = 2 + ex[i].age * 2;
      uint16_t c = (ex[i].age < 2) ? (uint16_t)0xFFFF : (ex[i].age < 4) ? (uint16_t)ST77XX_YELLOW : (ex[i].age < 6) ? (uint16_t)JT_ORANGE : RED;
      if (ex[i].age < 4) tft.fillCircle(ex[i].x, ex[i].y, r, c);
      else tft.drawCircle(ex[i].x, ex[i].y, r, c);
    }
    if (banner > 0) {
      char b[20];
      if (bossWave) { gTextC("WARNING!", W / 2, H / 2 - 18, 2, RED); gTextC("BOSS INCOMING", W / 2, H / 2 + 2, 1, jtTh->c3); }
      else { snprintf(b, sizeof(b), "WAVE %d", wave); gTextC(b, W / 2, H / 2 - 18, 2, jtTh->c1); }
      banner--;
      if (banner == 0) gClear(0, H / 2 - 24, W, 36, HUD + 1, bg);
    }
    if ((frame & 3) == 0) drawHud();
  }
}

static void gameStarRaid() {
  GameOpt o[3] = {
    { "Mode", { "Easy", "Normal", "Hard" }, 3, 1 },
    { "Ship", { "Falcon", "Viper", "Titan" }, 3, 0 },
    { "Theme", { "Neon", "Retro", "Sunset", "Ocean" }, 4, 0 }
  };
  while (true) {
    if (!gameSetup("STAR RAID", "Auto-fire  SW bomb", ST77XX_YELLOW, "sr", o, 3)) return;
    while (true) {
      long sc = gRun(playStarRaid, o);
      if (sc < 0) break;
      if (!gameOverScreen("Star Raid", jtTh->c1, (uint32_t)sc, "sr")) break;
    }
  }
}

// ============================================================================
//  4. BREAKER
// ============================================================================
struct BkBall { float x, y, vx, vy; bool on; int16_t hx[3], hy[3]; };
struct BkPow  { float x, y; uint8_t type; bool on; int16_t ox, oy; };

static long playBreaker(const GameOpt* o) {
  jtTh = &jtThemes[o[2].val];
  const uint16_t bg = jtTh->bg;
  static const int   livesStart[3] = { 5, 3, 2 };
  static const float spdBase[3]    = { 3.0f, 3.6f, 4.3f };
  static const float padFrac[3]    = { 0.30f, 0.22f, 0.16f };
  const int HUD = 12, ROWS = 6, COLS = 8, BH = 7, TOP = 20;
  const uint16_t RED = JT_RGB(255, 70, 50);

  int W = tft.width(), H = tft.height();
  int bw = W / COLS, padY = H - 12, padWBase = (int)(W * padFrac[o[0].val]), padW = padWBase, prevPadW = padW;
  float padX = W / 2.0f, padV = 0; int prevPadX = (int)padX;
  uint8_t br[ROWS][COLS];
  int bricksLeft = 0;
  BkBall balls[3];
  BkPow pows[3];
  memset(balls, 0, sizeof(balls)); memset(pows, 0, sizeof(pows));
  for (int i = 0; i < 3; i++) for (int k = 0; k < 3; k++) { balls[i].hx[k] = -1; balls[i].hy[k] = -1; }
  int lives = livesStart[o[0].val], level = 1;
  float spd = spdBase[o[0].val];
  uint32_t score = 0;
  bool attached = true;
  int wideT = 0, slowT = 0;
  uint32_t frame = 0;
  bool paused = false;

  uint16_t rowCol[ROWS];
  for (int r = 0; r < ROWS; r++) {
    int t = r * 255 / (ROWS - 1);
    rowCol[r] = (t < 128) ? jtBlend(jtTh->c1, jtTh->c2, t * 2) : jtBlend(jtTh->c2, jtTh->c3, (t - 128) * 2);
  }

  auto drawBrick = [&](int r, int c) {
    int x = c * bw, y = TOP + r * BH;
    uint8_t v = br[r][c];
    if (!v) { tft.fillRect(x, y, bw, BH, bg); return; }
    uint16_t base = (v == 2) ? JT_RGB(200, 205, 225) : rowCol[r];
    tft.fillRect(x, y, bw, BH, bg);
    tft.fillRect(x + 1, y + 1, bw - 2, BH - 2, base);
    tft.drawFastHLine(x + 1, y + 1, bw - 2, jtBlend(base, 0xFFFF, 130));
    tft.drawFastHLine(x + 1, y + BH - 2, bw - 2, jtBlend(base, 0x0000, 130));
  };

  auto redrawRegion = [&](int x, int y, int w, int h) {
    gClear(x, y, w, h, HUD + 1, bg);
    int x0 = x < 0 ? 0 : x, x1 = x + w - 1; if (x1 >= W) x1 = W - 1;
    int y0 = y, y1 = y + h - 1;
    if (y1 < TOP || y0 >= TOP + ROWS * BH) return;
    int r0 = (y0 - TOP) / BH, r1 = (y1 - TOP) / BH;
    if (y0 < TOP) r0 = 0;
    if (r1 >= ROWS) r1 = ROWS - 1;
    int c0 = x0 / bw, c1 = x1 / bw; if (c1 >= COLS) c1 = COLS - 1;
    for (int r = r0; r <= r1; r++) for (int c = c0; c <= c1; c++) if (br[r][c]) drawBrick(r, c);
  };

  auto buildLevel = [&]() {
    int pat = (level - 1) % 5;
    bricksLeft = 0;
    for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) {
      uint8_t v = 0;
      switch (pat) {
        case 0: v = 1; if (r == 0 && level > 5) v = 2; break;
        case 1: v = ((r + c) & 1) ? 0 : 1; if (r == 0) v = 2; break;
        case 2: v = (c >= r && c < COLS - r) ? 1 : 0; if (r < 3 && c == COLS / 2) v = 2; break;
        case 3: v = (r & 1) ? 0 : 1; if (r == 2) v = 2; break;
        default: v = (r == 0 || r == ROWS - 1 || c == 0 || c == COLS - 1) ? 2 : 1; break;
      }
      br[r][c] = v;
      if (v) bricksLeft++;
    }
  };

  auto drawHud = [&]() {
    tft.fillRect(0, 0, W, HUD, jtBlend(bg, jtTh->c1, 35));
    tft.drawFastHLine(0, HUD, W, jtBlend(bg, jtTh->c1, 140));
    char b[14];
    snprintf(b, sizeof(b), "%lu", (unsigned long)score);
    gText(b, 2, 2, 1, ST77XX_WHITE);
    snprintf(b, sizeof(b), "LV%d", level);
    gText(b, 50, 2, 1, jtTh->c3);
    if (wideT > 0) gText("W", 76, 2, 1, jtTh->c1);
    if (slowT > 0) gText("S", 84, 2, 1, jtTh->c4);
    for (int i = 0; i < lives && i < 5; i++) gHeart(W - 3 - (i + 1) * 9, 2, RED);
  };

  auto drawPaddle = [&]() {
    int x = (int)padX - padW / 2;
    tft.fillRoundRect(x, padY, padW, 5, 2, jtTh->c1);
    tft.drawFastHLine(x + 2, padY + 1, padW - 4, jtBlend(jtTh->c1, 0xFFFF, 150));
    tft.fillRect(x, padY, 3, 5, jtTh->c2);
    tft.fillRect(x + padW - 3, padY, 3, 5, jtTh->c2);
  };

  auto drawPlayfield = [&]() {
    jtFillScreen(bg);
    for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) if (br[r][c]) drawBrick(r, c);
    drawHud();
  };

  auto attachBall = [&]() {
    for (int i = 0; i < 3; i++) balls[i].on = false;
    balls[0].on = true; balls[0].x = padX; balls[0].y = padY - 3; balls[0].vx = balls[0].vy = 0;
    attached = true;
  };

  auto spawnPow = [&](float x, float y) {
    for (int i = 0; i < 3; i++) if (!pows[i].on) {
      int r = random(100);
      pows[i].on = true; pows[i].x = x; pows[i].y = y; pows[i].ox = -1; pows[i].oy = -1;
      pows[i].type = (r < 35) ? 0 : (r < 65) ? 1 : (r < 90) ? 2 : 3;   // W S M +
      return;
    }
  };

  // brick collision at a ball position; returns true if a brick was hit
  auto hitBrick = [&](float x, float y) -> bool {
    const float rr = 2.0f;
    float cxs[4] = { x - rr, x + rr, x - rr, x + rr };
    float cys[4] = { y - rr, y - rr, y + rr, y + rr };
    for (int k = 0; k < 4; k++) {
      if (cys[k] < TOP) continue;
      int r = (int)((cys[k] - TOP) / BH), c = (int)(cxs[k] / bw);
      if (r < 0 || r >= ROWS || c < 0 || c >= COLS || !br[r][c]) continue;
      if (br[r][c] == 2) { br[r][c] = 1; drawBrick(r, c); score += 5; }
      else {
        br[r][c] = 0; bricksLeft--; drawBrick(r, c);
        score += 10 + (ROWS - r) * 3;
        if (random(100) < 14) spawnPow(c * bw + bw / 2, TOP + r * BH + BH);
      }
      return true;
    }
    return false;
  };

  buildLevel();
  drawPlayfield();
  attachBall();
  gameFlushButtons();
  unsigned long lastFrame = 0;
  bool launchReq = false;
  int hudTick = 0;

  while (true) {
    if (backPressed()) return -1;
    if (rotatePressed()) {
      changeRotation();
      W = tft.width(); H = tft.height();
      bw = W / COLS; padY = H - 12;
      padWBase = (int)(W * padFrac[o[0].val]); padW = wideT > 0 ? padWBase * 3 / 2 : padWBase; prevPadW = padW;
      if (padX > W - padW / 2) padX = W - padW / 2; if (padX < padW / 2) padX = padW / 2;
      prevPadX = (int)padX;
      for (int i = 0; i < 3; i++) { pows[i].on = false; for (int k = 0; k < 3; k++) balls[i].hx[k] = balls[i].hy[k] = -1; }
      drawPlayfield(); attachBall();
    }
    if (joyPressed()) launchReq = true;
    if (millis() - lastFrame < 30) { delay(1); continue; }
    lastFrame = millis();
    frame++;
    JoyDir d = readJoyDirection();

    if (launchReq) {
      launchReq = false;
      if (attached) {
        balls[0].vx = spd * 0.35f * (random(2) ? 1 : -1);
        balls[0].vy = -spd * 0.94f;
        attached = false;
      } else {
        paused = !paused;
        if (paused) { tft.fillRoundRect(W / 2 - 30, H / 2 - 10, 60, 20, 5, JT_RGB(10, 12, 28)); tft.drawRoundRect(W / 2 - 30, H / 2 - 10, 60, 20, 5, jtTh->c1); gTextC("PAUSED", W / 2, H / 2 - 4, 1, jtTh->c3); }
        else { drawPlayfield(); }
      }
    }
    if (paused) continue;

    // ---- erase ----
    for (int i = 0; i < 3; i++) {
      for (int k = 0; k < 3; k++) if (balls[i].hx[k] >= 0) { redrawRegion(balls[i].hx[k] - 3, balls[i].hy[k] - 3, 7, 7); }
      if (!balls[i].on) for (int k = 0; k < 3; k++) balls[i].hx[k] = balls[i].hy[k] = -1;
    }
    for (int i = 0; i < 3; i++) if (pows[i].on && pows[i].ox >= 0) redrawRegion(pows[i].ox - 5, pows[i].oy - 5, 11, 11);
    gClear(prevPadX - prevPadW / 2 - 1, padY - 1, prevPadW + 3, 8, HUD + 1, bg);

    // ---- update paddle ----
    if (d.x != 0) { padV += d.x * 1.3f; float mx = 6.5f; if (padV > mx) padV = mx; if (padV < -mx) padV = -mx; }
    else padV *= 0.4f;
    if (padV > -0.2f && padV < 0.2f && d.x == 0) padV = 0;
    padX += padV;
    if (padX < padW / 2) { padX = padW / 2; padV = 0; }
    if (padX > W - padW / 2) { padX = W - padW / 2; padV = 0; }

    if (wideT > 0 && --wideT == 0) { padW = padWBase; hudTick = 0; }
    if (slowT > 0) slowT--;

    // ---- balls ----
    if (attached) { balls[0].x = padX; balls[0].y = padY - 3; }
    else {
      float step = (slowT > 0) ? 0.35f : 0.5f;
      for (int i = 0; i < 3; i++) if (balls[i].on) {
        BkBall &b = balls[i];
        for (int sub = 0; sub < 2 && b.on; sub++) {
          // x
          float nx = b.x + b.vx * step;
          if (nx < 2) { nx = 2; b.vx = fabsf(b.vx); }
          if (nx > W - 3) { nx = W - 3; b.vx = -fabsf(b.vx); }
          if (hitBrick(nx, b.y)) b.vx = -b.vx; else b.x = nx;
          // y
          float ny = b.y + b.vy * step;
          if (ny < HUD + 3) { ny = HUD + 3; b.vy = fabsf(b.vy); }
          if (hitBrick(b.x, ny)) b.vy = -b.vy; else b.y = ny;
          // paddle
          if (b.vy > 0 && b.y + 2 >= padY && b.y + 2 <= padY + 7 &&
              b.x >= padX - padW / 2 - 2 && b.x <= padX + padW / 2 + 2) {
            float rel = (b.x - padX) / (padW / 2.0f + 2);
            if (rel > 1) rel = 1; if (rel < -1) rel = -1;
            float ang = rel * 1.05f;
            b.vx = spd * sinf(ang); b.vy = -spd * cosf(ang);
            if (b.vy > -spd * 0.35f) b.vy = -spd * 0.35f;
            b.y = padY - 3;
          }
          if (b.y > H + 3) b.on = false;
        }
      }
      int alive = 0; for (int i = 0; i < 3; i++) if (balls[i].on) alive++;
      if (alive == 0) {
        lives--;
        if (lives <= 0) { delay(250); return (long)score; }
        wideT = 0; slowT = 0; padW = padWBase;
        drawHud();
        attachBall();
      }
    }

    // ---- power-ups ----
    for (int i = 0; i < 3; i++) if (pows[i].on) {
      pows[i].y += 1.4f;
      if (pows[i].y > H) { pows[i].on = false; continue; }
      if (pows[i].y >= padY - 4 && pows[i].y <= padY + 8 && fabsf(pows[i].x - padX) <= padW / 2 + 5) {
        pows[i].on = false;
        switch (pows[i].type) {
          case 0: padW = padWBase * 3 / 2; wideT = 330; if (padX < padW / 2) padX = padW / 2; if (padX > W - padW / 2) padX = W - padW / 2; break;
          case 1: slowT = 300; break;
          case 2: {
            int src = -1; for (int k = 0; k < 3; k++) if (balls[k].on) { src = k; break; }
            if (src >= 0 && !attached) {
              int made = 0;
              for (int k = 0; k < 3 && made < 2; k++) if (!balls[k].on) {
                balls[k] = balls[src];
                for (int q = 0; q < 3; q++) { balls[k].hx[q] = -1; balls[k].hy[q] = -1; }
                float a = (made == 0) ? 0.5f : -0.5f;
                float ca = cosf(a), sa = sinf(a);
                balls[k].vx = balls[src].vx * ca - balls[src].vy * sa;
                balls[k].vy = balls[src].vx * sa + balls[src].vy * ca;
                balls[k].on = true; made++;
              }
            }
          } break;
          default: if (lives < 5) lives++; else score += 100; break;
        }
        score += 20; hudTick = 0;
      }
    }

    // ---- level cleared ----
    if (bricksLeft <= 0) {
      for (int i = 0; i < 3; i++) { pows[i].on = false; balls[i].on = false; }
      gTextC("LEVEL CLEAR!", W / 2, H / 2 - 8, 1, jtTh->c3);
      delay(900);
      level++; spd *= 1.06f; if (spd > 6.2f) spd = 6.2f;
      score += 100;
      wideT = 0; slowT = 0; padW = padWBase;
      buildLevel(); drawPlayfield(); attachBall();
      for (int i = 0; i < 3; i++) for (int k = 0; k < 3; k++) balls[i].hx[k] = balls[i].hy[k] = -1;
      prevPadX = (int)padX; prevPadW = padW;
      continue;
    }

    // ---- draw ----
    for (int i = 0; i < 3; i++) if (pows[i].on) {
      static const char L[4] = { 'W', 'S', 'M', '+' };
      uint16_t c = (pows[i].type == 0) ? jtTh->c1 : (pows[i].type == 1) ? jtTh->c4 : (pows[i].type == 2) ? jtTh->c3 : RED;
      int x = (int)pows[i].x, y = (int)pows[i].y;
      tft.fillRoundRect(x - 4, y - 4, 9, 9, 2, c);
      tft.setTextSize(1); tft.setTextColor(bg); tft.setCursor(x - 2, y - 3); tft.print(L[pows[i].type]);
      pows[i].ox = x; pows[i].oy = y;
    }
    for (int i = 0; i < 3; i++) if (balls[i].on) {
      BkBall &b = balls[i];
      int ix = (int)b.x, iy = (int)b.y;
      if (b.hx[1] >= 0) tft.drawPixel(b.hx[1], b.hy[1], jtBlend(bg, jtTh->c1, 80));
      if (b.hx[0] >= 0) tft.fillCircle(b.hx[0], b.hy[0], 1, jtBlend(bg, jtTh->c1, 150));
      tft.fillCircle(ix, iy, 2, 0xFFFF);
      b.hx[2] = b.hx[1]; b.hy[2] = b.hy[1];
      b.hx[1] = b.hx[0]; b.hy[1] = b.hy[0];
      b.hx[0] = ix; b.hy[0] = iy;
    }
    drawPaddle();
    prevPadX = (int)padX; prevPadW = padW;

    if (++hudTick >= 6) { hudTick = 0; drawHud(); }
  }
}

static void gameBreaker() {
  GameOpt o[3] = {
    { "Mode",   { "Easy", "Normal", "Hard" }, 3, 1 },
    { "Paddle", { "Wide", "Normal", "Small" }, 3, 1 },
    { "Theme",  { "Neon", "Retro", "Sunset", "Ocean" }, 4, 0 }
  };
  while (true) {
    if (!gameSetup("BREAKER", "SW launch / pause", ST77XX_MAGENTA, "bk", o, 3)) return;
    while (true) {
      long sc = gRun(playBreaker, o);
      if (sc < 0) break;
      if (!gameOverScreen("Breaker", jtTh->c1, (uint32_t)sc, "bk")) break;
    }
  }
}

// ============================================================================
//  5. 2048
// ============================================================================
static long play2048(const GameOpt* o) {
  const int N = 3 + o[0].val;
  static const int goals[3] = { 2048, 1024, 512 };
  const int goal = goals[o[1].val];
  jtTh = &jtThemes[o[2].val];
  const uint16_t bg = jtTh->bg;
  const uint16_t boardCol = jtBlend(bg, jtTh->c1, 38);
  const uint16_t emptyCol = jtBlend(bg, jtTh->c1, 16);

  uint16_t g[5][5], prev[5][5], shown[5][5];
  memset(g, 0, sizeof(g)); memset(prev, 0, sizeof(prev));
  uint32_t score = 0, prevScore = 0;
  uint32_t best = gameBest("24");
  bool canUndo = false, won = false;
  int W = 0, H = 0, gap = 3, cell = 20, bx = 0, by = 0, bsize = 0;

  auto tileColor = [&](uint16_t v) -> uint16_t {
    int e = 0; while ((1 << (e + 1)) <= v) e++;     // v = 2^e
    if (e <= 4) return jtBlend(emptyCol, jtTh->c1, e * 52);
    if (e <= 8) return jtBlend(jtTh->c1, jtTh->c2, (e - 4) * 62);
    if (e <= 10) return jtBlend(jtTh->c2, jtTh->c3, (e - 8) * 120);
    return jtTh->c4;
  };

  auto drawTile = [&](int r, int c, int inset) {
    int x = bx + gap + c * (cell + gap), y = by + gap + r * (cell + gap);
    uint16_t v = g[r][c];
    if (inset > 0) tft.fillRect(x, y, cell, cell, boardCol);
    if (!v) { tft.fillRoundRect(x, y, cell, cell, 3, emptyCol); return; }
    uint16_t tc = tileColor(v);
    tft.fillRoundRect(x + inset, y + inset, cell - 2 * inset, cell - 2 * inset, 3, tc);
    if (inset > 0) return;
    tft.drawFastHLine(x + 3, y + 1, cell - 6, jtBlend(tc, 0xFFFF, 110));
    char b[8];
    if ((int)(snprintf(b, sizeof(b), "%u", v)) * 6 > cell - 2) snprintf(b, sizeof(b), "%uk", v / 1024);
    int len = strlen(b);
    uint8_t sz = (len * 12 <= cell - 2) ? 2 : 1;
    uint16_t fg = (jtLum(tc) > 150) ? (uint16_t)0x0000 : (uint16_t)0xFFFF;
    gText(b, x + (cell - len * 6 * sz) / 2 + (sz == 2 ? 1 : 0), y + (cell - 7 * sz) / 2 + 1, sz, fg);
  };

  auto drawHud = [&]() {
    char b[12];
    snprintf(b, sizeof(b), "%d", goal);
    int sz = (W >= 128 && (int)strlen(b) * 12 + 3 <= W - 76 + 0) ? 2 : 1;
    fastFillRect(0, 0, W - 76, by - 2, bg);
    gText(b, 3, 5, sz, jtTh->c3);
    gText("join tiles", 3, 5 + 8 * sz + 3, 1, jtBlend(bg, jtTh->c1, 150));
    const char* lab[2] = { "SCORE", "BEST" };
    uint32_t val[2] = { score, best > score ? best : score };
    for (int k = 0; k < 2; k++) {
      int x = W - 76 + k * 38;
      tft.fillRoundRect(x, 2, 36, 24, 3, boardCol);
      gTextC(lab[k], x + 18, 5, 1, jtBlend(jtTh->c1, 0xFFFF, 90));
      char v[12]; snprintf(v, sizeof(v), "%lu", (unsigned long)val[k]);
      gTextC(v, x + 18, 15, 1, k ? (uint16_t)JT_GOLD : (uint16_t)0xFFFF);
    }
  };

  auto drawAll = [&]() {
    W = tft.width(); H = tft.height();
    gap = (N == 5) ? 2 : 3;
    int side = (W - 8 < H - 32) ? W - 8 : H - 32;
    cell = (side - gap * (N + 1)) / N;
    bsize = cell * N + gap * (N + 1);
    bx = (W - bsize) / 2; by = H - bsize - 4;
    jtFillScreen(bg);
    tft.fillRoundRect(bx, by, bsize, bsize, 5, boardCol);
    for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) drawTile(r, c, 0);
    drawHud();
    memcpy(shown, g, sizeof(g));
  };

  auto spawn = [&]() -> int {
    int cells[25], n = 0;
    for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) if (!g[r][c]) cells[n++] = r * 5 + c;
    if (!n) return -1;
    int pick = cells[random(n)];
    g[pick / 5][pick % 5] = (random(10) == 0) ? 4 : 2;
    return pick;
  };

  auto at = [&](int dir, int k, int i) -> uint16_t& {
    switch (dir) {
      case 0: return g[k][i];
      case 1: return g[k][N - 1 - i];
      case 2: return g[i][k];
      default: return g[N - 1 - i][k];
    }
  };

  auto canMove = [&]() -> bool {
    for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) {
      if (!g[r][c]) return true;
      if (c + 1 < N && g[r][c] == g[r][c + 1]) return true;
      if (r + 1 < N && g[r][c] == g[r + 1][c]) return true;
    }
    return false;
  };

  auto doMove = [&](int dir) -> bool {
    uint16_t before[5][5]; memcpy(before, g, sizeof(g));
    uint32_t sBefore = score;
    for (int k = 0; k < N; k++) {
      uint16_t tmp[5]; int n = 0;
      for (int i = 0; i < N; i++) { uint16_t v = at(dir, k, i); if (v) tmp[n++] = v; }
      for (int i = 0; i + 1 < n; i++) {
        if (tmp[i] == tmp[i + 1]) {
          tmp[i] *= 2; score += tmp[i];
          for (int j = i + 1; j < n - 1; j++) tmp[j] = tmp[j + 1];
          n--;
        }
      }
      for (int i = 0; i < N; i++) at(dir, k, i) = (i < n) ? tmp[i] : 0;
    }
    if (memcmp(before, g, sizeof(g)) == 0) return false;
    memcpy(prev, before, sizeof(prev)); prevScore = sBefore; canUndo = true;
    return true;
  };

  auto refresh = [&](int newCell) {
    for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) {
      if (g[r][c] != shown[r][c] && (r * 5 + c) != newCell) drawTile(r, c, 0);
    }
    if (newCell >= 0) {
      drawTile(newCell / 5, newCell % 5, 6); delay(35);
      drawTile(newCell / 5, newCell % 5, 3); delay(35);
      drawTile(newCell / 5, newCell % 5, 0);
    }
    memcpy(shown, g, sizeof(g));
    drawHud();
  };

  spawn(); spawn();
  drawAll();
  gameFlushButtons();
  bool held = false; unsigned long lastMove = 0;

  while (true) {
    if (backPressed()) return -1;
    if (rotatePressed()) { changeRotation(); drawAll(); }
    JoyDir d = readJoyDirection();
    int dir = -1;
    if (d.x < 0) dir = 0; else if (d.x > 0) dir = 1; else if (d.y < 0) dir = 2; else if (d.y > 0) dir = 3;
    unsigned long now = millis();
    if (dir >= 0 && (!held || now - lastMove > 380)) {
      held = true; lastMove = now;
      if (doMove(dir)) {
        int nc = spawn();
        refresh(nc);
        if (!won) {
          for (int r = 0; r < N && !won; r++) for (int c = 0; c < N; c++) if (g[r][c] >= goal) { won = true; break; }
          if (won) {
            tft.fillRoundRect(W / 2 - 50, H / 2 - 16, 100, 32, 6, JT_RGB(10, 12, 28));
            tft.drawRoundRect(W / 2 - 50, H / 2 - 16, 100, 32, 6, jtTh->c3);
            gTextC("YOU WIN!", W / 2, H / 2 - 10, 1, jtTh->c3);
            gTextC("keep going...", W / 2, H / 2 + 2, 1, ST77XX_WHITE);
            delay(1400);
            drawAll();
          }
        }
        if (!canMove()) { delay(500); return (long)score; }
      }
    }
    if (dir < 0) held = false;
    if (joyPressed() && canUndo) {
      memcpy(g, prev, sizeof(g)); score = prevScore; canUndo = false;
      for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) shown[r][c] = 0xFFFF;
      refresh(-1);
    }
    delay(10);
  }
}

static void game2048() {
  GameOpt o[3] = {
    { "Size",  { "3x3", "4x4", "5x5" }, 3, 1 },
    { "Goal",  { "2048", "1024", "512" }, 3, 0 },
    { "Theme", { "Neon", "Retro", "Sunset", "Ocean" }, 4, 0 }
  };
  while (true) {
    if (!gameSetup("2048", "Slide  SW undo", ST77XX_ORANGE, "24", o, 3)) return;
    while (true) {
      long sc = gRun(play2048, o);
      if (sc < 0) break;
      if (!gameOverScreen("2048", jtTh->c1, (uint32_t)sc, "24")) break;
    }
  }
}


// ============================================================================
//  6.  ROAD RACER   (driving engine; the old Off-Road style is no longer in the menu)
//    Joystick: left/right steer, up accelerate, down brake, hold SW = NITRO
// ============================================================================
struct RcThing { float x, y, vy; uint8_t type, col; bool on; };
struct RcScen  { float x, y; uint8_t type; };
struct RcExp   { int16_t x, y; uint8_t age; bool on; };
// thing types: 0 car, 1 truck, 2 rock, 3 oil, 4 barrel, 5 coin, 6 can, 7 boost pad
static const uint8_t RC_W[8] = { 12, 14, 10, 16, 8, 7, 8, 14 };
static const uint8_t RC_H[8] = { 20, 32, 8, 9, 10, 7, 9, 12 };

static long playRacer(const GameOpt* o, int variant) {
  jtTh = &jtThemes[o[2].val];
  const uint16_t bg = jtTh->bg;
  static const uint16_t carCols[4] = { JT_RGB(235, 40, 40), JT_RGB(50, 110, 255), JT_RGB(255, 205, 30), JT_RGB(235, 235, 235) };
  static const int livesStart[3] = { 5, 3, 2 };
  const uint16_t myCol = carCols[o[1].val];
  const int mode = o[0].val;
  const int HUD = 12;
  const uint16_t groundCol = jtBlend(variant == 0 ? JT_RGB(26, 100, 46) : JT_RGB(160, 120, 66), bg, 70);
  const uint16_t roadCol   = jtBlend(variant == 0 ? JT_RGB(56, 58, 68) : JT_RGB(112, 82, 54), bg, 40);
  const uint16_t lineCol   = variant == 0 ? JT_RGB(235, 235, 235) : JT_RGB(240, 200, 120);
  const uint16_t dashCol   = jtBlend(roadCol, lineCol, 200);
  const uint16_t GLASS = JT_RGB(40, 60, 100), RED = JT_RGB(255, 60, 50), GOLD = JT_RGB(255, 210, 40);
  const char* fname = variant == 0 ? "Road Racer" : "Off-Road";

  int W = 0, H = 0, laneW = 20, roadW = 80, roadX = 24, py = 0;
  RcThing th[9]; RcScen sc[10]; RcExp ex[5];
  memset(th, 0, sizeof(th)); memset(sc, 0, sizeof(sc)); memset(ex, 0, sizeof(ex));
  float px = 0, v = 3.5f, nitro = 100, fuel = 100, scroll = 0, dist = 0;
  int lives = livesStart[mode], invuln = 0, oilT = 0, boostT = 0, level = 1, spawnT = 30;
  uint32_t bonus = 0, frame = 0;
  int prevBase = 0, prevPx = 0, prevPy = 0;
  bool prevFlame = false;

  auto R = [&](int x, int y, int w, int h, uint16_t c) { gClear(x, y, w, h, HUD + 1, c); };

  auto drawCar = [&](int cx, int y, uint16_t c, bool player) {
    int w = 12, h = 20, x = cx - 6;
    R(x + 1, y, w - 2, h, c);
    R(x, y + 2, w, h - 4, c);
    if (player) R(cx - 1, y + 2, 2, h - 4, jtBlend(c, 0xFFFF, 150));
    R(x + 2, y + 5, w - 4, 4, GLASS);
    R(x + 2, y + 13, w - 4, 3, GLASS);
    R(x - 1, y + 2, 2, 5, 0x0000); R(x + w - 1, y + 2, 2, 5, 0x0000);
    R(x - 1, y + h - 7, 2, 5, 0x0000); R(x + w - 1, y + h - 7, 2, 5, 0x0000);
    R(x + 1, y, 3, 1, 0xFFE0); R(x + w - 4, y, 3, 1, 0xFFE0);
    R(x + 1, y + h - 1, 3, 1, RED); R(x + w - 4, y + h - 1, 3, 1, RED);
  };
  auto drawTruck = [&](int cx, int y, uint16_t c) {
    int w = 14, x = cx - 7;
    R(x, y + 11, w, 21, JT_RGB(205, 205, 215));
    R(x + 1, y + 12, w - 2, 1, 0xFFFF);
    R(x + 1, y, w - 2, 11, c);
    R(x + 2, y + 2, w - 4, 4, GLASS);
    R(x - 1, y + 3, 2, 5, 0x0000); R(x + w - 1, y + 3, 2, 5, 0x0000);
    R(x - 1, y + 22, 2, 6, 0x0000); R(x + w - 1, y + 22, 2, 6, 0x0000);
    R(x + 1, y + 31, w - 2, 1, RED);
  };
  auto drawThing = [&](const RcThing &t) {
    int x = (int)t.x, y = (int)t.y;
    switch (t.type) {
      case 0: drawCar(x, y, carCols[t.col & 3], false); break;
      case 1: drawTruck(x, y, carCols[t.col & 3]); break;
      case 2: R(x - 3, y, 6, 8, JT_RGB(130, 130, 140)); R(x - 5, y + 2, 10, 5, JT_RGB(130, 130, 140)); R(x - 3, y + 1, 3, 2, JT_RGB(190, 190, 200)); break;
      case 3: R(x - 6, y, 12, 9, JT_RGB(18, 18, 24)); R(x - 8, y + 2, 16, 5, JT_RGB(18, 18, 24)); R(x - 4, y + 3, 4, 1, JT_RGB(70, 90, 160)); break;
      case 4: R(x - 4, y, 8, 10, JT_ORANGE); R(x - 4, y + 3, 8, 1, JT_RGB(90, 40, 0)); R(x - 4, y + 6, 8, 1, JT_RGB(90, 40, 0)); break;
      case 5: R(x - 2, y, 5, 7, GOLD); R(x - 3, y + 1, 7, 5, GOLD); R(x - 1, y + 2, 2, 3, JT_RGB(255, 245, 160)); break;
      case 6: {
        uint16_t cc = (variant == 0) ? jtTh->c1 : RED;
        R(x - 4, y, 8, 9, cc); R(x - 1, y + 2, 2, 5, 0xFFFF); R(x - 2, y + 4, 4, 1, 0xFFFF);
      } break;
      default:
        R(x - 7, y, 14, 12, jtBlend(roadCol, 0, 120));
        for (int k = 0; k < 2; k++) {
          R(x - 1, y + k * 6, 2, 1, 0xFFE0); R(x - 3, y + 1 + k * 6, 6, 1, 0xFFE0); R(x - 5, y + 2 + k * 6, 10, 1, 0xFFE0);
        }
    }
  };
  auto eraseThing = [&](const RcThing &t) {
    int w = RC_W[t.type], h = RC_H[t.type];
    R((int)t.x - w / 2 - 3, (int)t.y - 1, w + 6, h + 2, roadCol);
  };
  auto drawScen = [&](const RcScen &s) {
    int x = (int)s.x, y = (int)s.y;
    if (variant == 0) {
      if (s.type == 0) { R(x + 4, y + 9, 3, 5, JT_RGB(90, 60, 30)); R(x + 1, y, 9, 10, JT_RGB(20, 120, 40)); R(x, y + 2, 11, 6, JT_RGB(20, 120, 40)); R(x + 3, y + 1, 3, 2, JT_RGB(60, 170, 80)); }
      else if (s.type == 1) { R(x + 2, y + 2, 2, 12, JT_RGB(150, 150, 160)); R(x, y, 7, 3, jtTh->c3); }
      else { R(x + 1, y, 6, 6, JT_RGB(30, 130, 50)); R(x, y + 2, 8, 3, JT_RGB(30, 130, 50)); }
    } else {
      if (s.type == 0) { R(x + 3, y, 3, 14, JT_RGB(40, 120, 50)); R(x, y + 4, 3, 2, JT_RGB(40, 120, 50)); R(x, y + 2, 2, 4, JT_RGB(40, 120, 50)); R(x + 6, y + 6, 3, 2, JT_RGB(40, 120, 50)); R(x + 7, y + 3, 2, 5, JT_RGB(40, 120, 50)); }
      else if (s.type == 1) { R(x + 1, y + 2, 8, 5, JT_RGB(120, 100, 80)); R(x + 3, y, 5, 3, JT_RGB(140, 120, 100)); }
      else { R(x + 1, y, 6, 6, JT_RGB(110, 120, 50)); R(x, y + 2, 8, 3, JT_RGB(110, 120, 50)); }
    }
  };
  auto eraseScen = [&](const RcScen &s) { R((int)s.x - 1, (int)s.y - 1, 13, 16, groundCol); };
  auto placeScen = [&](int i, float y) {
    int rightX = roadX + roadW + 6;
    int leftMax = roadX - 16; if (leftMax < 3) leftMax = 3;
    int rightMax = W - rightX - 12; if (rightMax < 1) rightMax = 1;
    sc[i].type = random(3); sc[i].y = y;
    sc[i].x = (i & 1) ? (float)(rightX + random(rightMax)) : (float)(2 + random(leftMax));
  };
  auto dashes = [&](int base, uint16_t colr) {
    int n = H / 20 + 2;
    for (int b = 0; b < 3; b++) {
      int x = roadX + laneW * (b + 1) - 1;
      for (int k = 0; k < n; k++) R(x, base + k * 20 - 20, 2, 9, colr);
    }
  };
  auto drawStatic = [&]() {
    jtFillScreen(groundCol);
    fastFillRect(roadX, HUD + 1, roadW, H - HUD - 1, roadCol);
    fastFillRect(roadX - 2, HUD + 1, 2, H - HUD - 1, lineCol);
    fastFillRect(roadX + roadW, HUD + 1, 2, H - HUD - 1, lineCol);
  };
  auto drawHud = [&]() {
    tft.fillRect(0, 0, W, HUD, jtBlend(bg, jtTh->c1, 40));
    tft.drawFastHLine(0, HUD, W, jtBlend(bg, jtTh->c1, 150));
    char b[14]; snprintf(b, sizeof(b), "%lu", (unsigned long)((uint32_t)dist + bonus));
    gText(b, 2, 2, 1, ST77XX_WHITE);
    int bx = W / 2 - 6;
    tft.fillRect(bx, 2, 30, 3, jtBlend(bg, 0xFFFF, 50));
    tft.fillRect(bx, 2, (int)(30 * nitro / 100), 3, jtTh->c1);
    if (variant == 1) {
      tft.fillRect(bx, 7, 30, 3, jtBlend(bg, 0xFFFF, 50));
      tft.fillRect(bx, 7, (int)(30 * fuel / 100), 3, fuel < 25 ? RED : jtTh->c3);
    }
    for (int i = 0; i < lives && i < 5; i++) gHeart(W - 3 - (i + 1) * 9, 2, RED);
  };

  auto relayout = [&]() {
    W = tft.width(); H = tft.height();
    laneW = (W * 5 / 8) / 4; roadW = laneW * 4; roadX = (W - roadW) / 2; py = H - 36;
    px = W / 2.0f;
    for (int i = 0; i < 9; i++) th[i].on = false;
    for (int i = 0; i < 5; i++) ex[i].on = false;
    for (int i = 0; i < 10; i++) placeScen(i, HUD + 1 + i * (H - HUD) / 10.0f);
    drawStatic();
    prevPx = (int)px; prevPy = py; prevBase = (int)scroll % 20;
  };

  auto addExp = [&](int x, int y) {
    for (int i = 0; i < 5; i++) if (!ex[i].on) { ex[i].on = true; ex[i].x = x; ex[i].y = y; ex[i].age = 0; return; }
  };

  auto trySpawn = [&]() {
    int r = random(100), t;
    if (variant == 0) t = r < 50 ? 0 : r < 66 ? 1 : r < 84 ? 5 : r < 92 ? 6 : 7;
    else t = r < 24 ? 2 : r < 38 ? 3 : r < 54 ? 4 : r < 80 ? 5 : r < 90 ? 6 : 7;
    int lane = random(4);
    int obst = 0;
    for (int i = 0; i < 9; i++) if (th[i].on) {
      int l = (int)((th[i].x - roadX) / laneW);
      if (l == lane && th[i].y < HUD + 60) return;
      if (th[i].type <= 4 && th[i].y < HUD + 45) obst++;
    }
    if (t <= 4 && obst >= 2) return;
    for (int i = 0; i < 9; i++) if (!th[i].on) {
      th[i].on = true; th[i].type = t; th[i].col = random(4);
      th[i].x = roadX + lane * laneW + laneW / 2.0f;
      th[i].y = HUD - RC_H[t] + 1;
      th[i].vy = (t == 0) ? 1.7f + random(0, 8) / 10.0f : (t == 1) ? 1.5f + random(0, 6) / 10.0f : 0;
      return;
    }
  };

  relayout();
  gameFlushButtons();
  unsigned long lastFrame = 0;

  while (true) {
    if (backPressed()) return -1;
    if (rotatePressed()) { changeRotation(); relayout(); }
    if (millis() - lastFrame < 33) { delay(1); continue; }
    lastFrame = millis();
    frame++;
    JoyDir d = readJoyDirection();
    bool nitroKey = (digitalRead(JOY_SW) == LOW);

    // ---------- erase ----------
    dashes(prevBase, roadCol);
    for (int i = 0; i < 9; i++) if (th[i].on) eraseThing(th[i]);
    for (int i = 0; i < 10; i++) eraseScen(sc[i]);
    R(prevPx - 9, py - 1, 19, 20 + 8, roadCol);
    for (int i = 0; i < 5; i++) if (ex[i].on) { int r = 3 + ex[i].age * 2; R(ex[i].x - r - 1, ex[i].y - r - 1, 2 * r + 3, 2 * r + 3, roadCol); }

    // ---------- update ----------
    level = 1 + (int)(dist / 90);
    float cruise = 4.0f + (level - 1) * 0.4f; if (cruise > 7.0f) cruise = 7.0f;
    if (variant == 1) cruise -= 0.3f;
    bool boosting = nitroKey && nitro > 1;
    if (boostT > 0) boostT--;
    float target = cruise;
    if (d.y < 0) target = cruise + 2.5f;
    if (d.y > 0) target = 2.6f;
    if (boosting) { target += 3.5f; nitro -= 1.3f; }
    else if (nitro < 100) nitro += 0.12f;
    if (boostT > 0) target = 9.5f;
    if (v < target) v += (boosting || boostT > 0) ? 0.35f : 0.12f; else v -= (d.y > 0) ? 0.3f : 0.08f;
    if (v > 10.0f) v = 10.0f; if (v < 2.5f) v = 2.5f;

    float steer = 2.2f + v * 0.18f;
    px += d.x * steer;
    if (oilT > 0) { oilT--; px += ((frame >> 2) & 1) ? 1.6f : -1.6f; }
    float minX = roadX + 7, maxX = roadX + roadW - 7;
    if (px < minX) { px = minX; v *= 0.96f; }
    if (px > maxX) { px = maxX; v *= 0.96f; }

    scroll += v;
    dist += v * 0.04f;
    if (variant == 1) { fuel -= 0.045f + (boosting ? 0.05f : 0); }
    if (invuln > 0) invuln--;

    // spawn
    if (--spawnT <= 0) {
      trySpawn();
      float S = 58.0f - level * 3.0f - mode * 6.0f; if (S < 26) S = 26;
      spawnT = (int)(S / v) + random(0, 4); if (spawnT < 4) spawnT = 4;
    }

    // things
    for (int i = 0; i < 9; i++) if (th[i].on) {
      RcThing &t = th[i];
      t.y += v - t.vy;
      if (t.y > H + 6 || t.y < HUD - 70) { t.on = false; continue; }
      float tw = RC_W[t.type], tH = RC_H[t.type];
      bool overlap = (fabsf(t.x - px) < (tw / 2 - 1 + 5)) && (t.y + tH - 1 > py + 1) && (t.y + 1 < py + 19);
      if (!overlap) continue;
      switch (t.type) {
        case 3: oilT = 40; break;
        case 5: bonus += 50; t.on = false; break;
        case 6: if (variant == 0) nitro = 100; else { fuel += 40; if (fuel > 100) fuel = 100; } bonus += 20; t.on = false; break;
        case 7: boostT = 40; bonus += 10; t.on = false; break;
        default:
          if (invuln == 0) {
            lives--; invuln = 75; v = 2.6f; boostT = 0; nitro = (nitro + 20 > 100) ? 100 : nitro + 20;
            addExp((int)((t.x + px) / 2), (int)(py + 4));
            t.on = false;
          }
      }
    }

    // scenery
    for (int i = 0; i < 10; i++) {
      sc[i].y += v;
      if (sc[i].y > H) placeScen(i, HUD - 14.0f - random(0, 20));
    }
    for (int i = 0; i < 5; i++) if (ex[i].on) { if (++ex[i].age > 6) ex[i].on = false; }

    // ---------- game over ----------
    if (lives <= 0 || (variant == 1 && fuel <= 0)) {
      if (lives > 0) { gTextC("OUT OF FUEL", W / 2, H / 2 - 4, 1, RED); }
      for (int k = 0; k < 6; k++) addExp((int)px + random(-8, 9), py + random(0, 18));
      for (int a = 0; a < 7; a++) {
        for (int i = 0; i < 5; i++) if (ex[i].on) {
          int r = 3 + ex[i].age * 2;
          tft.fillCircle(ex[i].x, ex[i].y, r, ex[i].age < 3 ? (uint16_t)ST77XX_YELLOW : (ex[i].age < 5 ? (uint16_t)JT_ORANGE : RED));
          ex[i].age++; if (ex[i].age > 6) ex[i].on = false;
        }
        delay(80);
      }
      (void)fname;
      return (long)((uint32_t)dist + bonus);
    }

    // ---------- draw ----------
    int base = (int)scroll % 20;
    dashes(base, dashCol);
    prevBase = base;
    for (int i = 0; i < 10; i++) drawScen(sc[i]);
    for (int i = 0; i < 9; i++) if (th[i].on) drawThing(th[i]);
    if (!(invuln > 0 && (frame & 2))) {
      drawCar((int)px, py, myCol, true);
      if (boosting || boostT > 0) {
        R((int)px - 4, py + 20, 3, 5, JT_ORANGE); R((int)px + 1, py + 20, 3, 5, JT_ORANGE);
        R((int)px - 3, py + 20, 1, 3, ST77XX_YELLOW); R((int)px + 2, py + 20, 1, 3, ST77XX_YELLOW);
      }
    }
    prevPx = (int)px; prevFlame = boosting;
    for (int i = 0; i < 5; i++) if (ex[i].on) {
      int r = 3 + ex[i].age * 2;
      uint16_t c = ex[i].age < 2 ? (uint16_t)0xFFFF : (ex[i].age < 4 ? (uint16_t)ST77XX_YELLOW : (uint16_t)JT_ORANGE);
      if (ex[i].age < 4) tft.fillCircle(ex[i].x, ex[i].y, r, c); else tft.drawCircle(ex[i].x, ex[i].y, r, c);
    }
    if ((frame & 3) == 0) drawHud();
  }
}

static void gameRoadRacer() {
  GameOpt o[3] = {
    { "Mode",  { "Easy", "Normal", "Hard" }, 3, 1 },
    { "Car",   { "Red", "Blue", "Yellow", "White" }, 4, 0 },
    { "Theme", { "Neon", "Retro", "Sunset", "Ocean" }, 4, 0 }
  };
  while (true) {
    if (!gameSetup("ROAD RACER", "Up/Dn speed  SW nitro", ST77XX_RED, "rr", o, 3)) return;
    while (true) {
      long sc = gRun([](const GameOpt* oo) -> long { return playRacer(oo, 0); }, o);
      if (sc < 0) break;
      if (!gameOverScreen("Road Racer", jtTh->c1, (uint32_t)sc, "rr")) break;
    }
  }
}

// ============================================================================
//  7.  SKY HOPPER  (replaces Off-Road)  - flap through the pillars
// ============================================================================
struct HpPipe { float x; int16_t gy; bool on, passed; };

static long playSkyHopper(const GameOpt* o) {
  jtTh = &jtThemes[o[2].val];
  const uint16_t bg = jtTh->bg;
  static const uint16_t jetCols[4] = { JT_RGB(255, 210, 40), JT_RGB(255, 70, 60), JT_RGB(70, 170, 255), JT_RGB(120, 255, 120) };
  static const int   gapBase[3] = { 56, 48, 40 };
  static const float spdBase[3] = { 1.5f, 1.9f, 2.3f };
  const uint16_t jetCol = jetCols[o[1].val];
  const int mode = o[0].val;
  const int HUD = 12, PW = 18, CAP = 3, CH = 7;
  const uint16_t RED = JT_RGB(255, 70, 50), WHITE = 0xFFFF;
  const uint16_t pipeBody = jtBlend(bg, jtTh->c4, 190);
  const uint16_t pipeHi = jtBlend(pipeBody, WHITE, 90), pipeLo = jtBlend(pipeBody, 0x0000, 120);
  const uint16_t groundCol = jtBlend(bg, jtTh->c3, 110), groundHi = jtBlend(groundCol, WHITE, 70);
  uint16_t sky[4];
  for (int i = 0; i < 4; i++) sky[i] = jtBlend(jtBlend(bg, jtTh->c1, 45), jtBlend(bg, jtTh->c2, 110), i * 255 / 3);

  int W = 0, H = 0, groundY = 0, birdX = 0, gap = 48, spacing = 70;
  HpPipe pipes[4];
  memset(pipes, 0, sizeof(pipes));
  float by = 0, vy = 0;
  uint32_t score = 0, frame = 0;
  const uint32_t best = gameBest("hp");
  bool started = false, flapReq = false, prevUp = false;
  int flapAnim = 0;
  float scrollX = 0;

  auto relayout = [&]() {
    W = tft.width(); H = tft.height();
    groundY = H - 14;
    birdX = W / 4 + 4;
    gap = gapBase[mode] - (H < 140 ? 6 : 0);
    spacing = W * 55 / 100;
    by = (HUD + groundY) / 2.0f; vy = 0;
    for (int i = 0; i < 4; i++) pipes[i].on = false;
    started = false;
  };

  auto spawn = [&](float x) {
    for (int i = 0; i < 4; i++) if (!pipes[i].on) {
      int lo = HUD + 14, hi = groundY - gap - 14;
      pipes[i].x = x;
      pipes[i].gy = (int16_t)(lo + random(hi - lo + 1));
      pipes[i].on = true;
      pipes[i].passed = false;
      return;
    }
  };

  auto drawPipe = [&](const HpPipe &p) {
    int x = (int)p.x, gy = p.gy, gb = gy + gap;
    int topH = gy - (HUD + 1) - CH;
    if (topH > 0) {
      tft.fillRect(x, HUD + 1, PW, topH, pipeBody);
      tft.fillRect(x + 2, HUD + 1, 2, topH, pipeHi);
      tft.fillRect(x + PW - 3, HUD + 1, 3, topH, pipeLo);
    }
    tft.fillRect(x - CAP, gy - CH, PW + 2 * CAP, CH, pipeBody);
    tft.fillRect(x - CAP, gy - CH, PW + 2 * CAP, 1, pipeHi);
    tft.fillRect(x - CAP, gy - 1, PW + 2 * CAP, 1, pipeLo);
    int botH = groundY - gb - CH;
    tft.fillRect(x - CAP, gb, PW + 2 * CAP, CH, pipeBody);
    tft.fillRect(x - CAP, gb, PW + 2 * CAP, 1, pipeHi);
    tft.fillRect(x - CAP, gb + CH - 1, PW + 2 * CAP, 1, pipeLo);
    if (botH > 0) {
      tft.fillRect(x, gb + CH, PW, botH, pipeBody);
      tft.fillRect(x + 2, gb + CH, 2, botH, pipeHi);
      tft.fillRect(x + PW - 3, gb + CH, 3, botH, pipeLo);
    }
  };

  auto drawBird = [&](int x, int y, bool wingUp) {
    uint16_t c = jetCol, dk = jtBlend(c, 0x0000, 110), lt = jtBlend(c, WHITE, 130);
    tft.fillTriangle(x - 6, y - 2, x - 6, y + 2, x - 10, y - 4, dk);
    tft.fillRoundRect(x - 6, y - 4, 13, 8, 3, c);
    tft.drawFastHLine(x - 4, y - 3, 8, lt);
    if (wingUp) tft.fillTriangle(x - 3, y - 1, x + 2, y - 1, x - 4, y - 8, dk);
    else        tft.fillTriangle(x - 3, y + 1, x + 2, y + 1, x - 4, y + 7, dk);
    tft.fillTriangle(x + 7, y - 2, x + 7, y + 2, x + 11, y, JT_ORANGE);
    tft.fillRect(x + 2, y - 3, 3, 3, WHITE);
    tft.fillRect(x + 4, y - 2, 1, 1, 0x0000);
  };

  auto render = [&]() {
    int span = groundY - (HUD + 1);
    for (int i = 0; i < 4; i++) {
      int y0 = HUD + 1 + span * i / 4, y1 = HUD + 1 + span * (i + 1) / 4;
      tft.fillRect(0, y0, W, y1 - y0, sky[i]);
    }
    for (int i = 0; i < 4; i++) if (pipes[i].on) drawPipe(pipes[i]);
    tft.fillRect(0, groundY, W, H - groundY, groundCol);
    tft.drawFastHLine(0, groundY, W, groundHi);
    int off = (int)scrollX & 7;
    for (int x = -off; x < W; x += 8) tft.fillRect(x, groundY + 4, 4, 2, groundHi);
    drawBird(birdX, (int)by, flapAnim > 0 || (!started && ((frame >> 2) & 1)));
    if (!started) {
      gTextC("GET READY", W / 2, HUD + 28, 1, WHITE);
      gTextC("SW / Up = flap", W / 2, HUD + 40, 1, jtTh->c3);
    }
    // HUD
    tft.fillRect(0, 0, W, HUD, jtBlend(bg, jtTh->c1, 35));
    tft.drawFastHLine(0, HUD, W, jtBlend(bg, jtTh->c1, 140));
    char b[20];
    snprintf(b, sizeof(b), "SC %lu", (unsigned long)score);
    gText(b, 3, 2, 1, WHITE);
    snprintf(b, sizeof(b), "BEST %lu", (unsigned long)(best > score ? best : score));
    gText(b, W - 3 - (int)strlen(b) * 6, 2, 1, JT_GOLD);
  };

  relayout();
  gameFlushButtons();
  unsigned long lastFrame = 0;

  while (true) {
    if (backPressed()) return -1;
    if (rotatePressed()) { changeRotation(); relayout(); }
    if (joyPressed()) flapReq = true;
    if (millis() - lastFrame < 33) { delay(1); continue; }
    lastFrame = millis();
    frame++;

    JoyDir d = readJoyDirection();
    bool up = (d.y < 0);
    if (up && !prevUp) flapReq = true;
    prevUp = up;

    if (flapReq) {
      flapReq = false;
      if (!started) { started = true; spawn((float)(W + CAP + 10)); }
      vy = -3.5f;
      flapAnim = 6;
    }
    if (flapAnim > 0) flapAnim--;

    float spd = spdBase[mode] + ((score / 5 > 8) ? 8 : (int)(score / 5)) * 0.08f;

    if (!started) {
      by = (HUD + groundY) / 2.0f + sinf(frame * 0.25f) * 4.0f;
    } else {
      vy += 0.32f;
      if (vy > 5.5f) vy = 5.5f;
      by += vy;
      if (by < HUD + 6) { by = HUD + 6; if (vy < 0) vy = 0; }
      scrollX += spd;

      float lastX = -1000.0f;
      for (int i = 0; i < 4; i++) if (pipes[i].on) {
        pipes[i].x -= spd;
        if (pipes[i].x + PW + CAP < 0) pipes[i].on = false;
        else if (pipes[i].x > lastX) lastX = pipes[i].x;
      }
      if (lastX < W - spacing) spawn((float)(W + CAP + 2));

      bool dead = ((int)by + 4 >= groundY);
      int bx0 = birdX - 5, bx1 = birdX + 6, by0 = (int)by - 3, by1 = (int)by + 3;
      for (int i = 0; i < 4; i++) if (pipes[i].on) {
        HpPipe &p = pipes[i];
        if (!p.passed && p.x + PW < birdX - 6) { p.passed = true; score++; }
        if (bx1 > (int)p.x - CAP && bx0 < (int)p.x + PW + CAP && (by0 < p.gy || by1 > p.gy + gap)) dead = true;
      }

      if (dead) {
        render();
        for (int k = 0; k < 2; k++) {
          tft.drawRect(0, HUD + 1, W, groundY - HUD - 1, RED);
          tft.drawRect(1, HUD + 2, W - 2, groundY - HUD - 3, RED);
          delay(110);
          render();
          delay(80);
        }
        delay(250);
        return (long)score;
      }
    }
    render();
  }
}

static void gameSkyHopper() {
  GameOpt o[3] = {
    { "Mode",  { "Easy", "Normal", "Hard" }, 3, 1 },
    { "Jet",   { "Gold", "Red", "Blue", "Green" }, 4, 0 },
    { "Theme", { "Neon", "Retro", "Sunset", "Ocean" }, 4, 3 }
  };
  while (true) {
    if (!gameSetup("SKY HOPPER", "SW / Up flap  dodge", JT_SKY, "hp", o, 3)) return;
    while (true) {
      long sc = gRun(playSkyHopper, o);
      if (sc < 0) break;
      if (!gameOverScreen("Sky Hopper", jtTh->c1, (uint32_t)sc, "hp")) break;
    }
  }
}

// ============================================================================
//  8. INVADERS
// ============================================================================
static const uint8_t INV_BMP[3][2][8] = {
  { { 0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x24, 0x5A, 0xA5 }, { 0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x5A, 0x81, 0x42 } },
  { { 0x24, 0x24, 0x7E, 0xDB, 0xFF, 0xBD, 0xA5, 0x24 }, { 0x24, 0xA5, 0xFF, 0xDB, 0x7E, 0x3C, 0x24, 0x42 } },
  { { 0x3C, 0x7E, 0xFF, 0x99, 0xFF, 0x3C, 0x66, 0xC3 }, { 0x3C, 0x7E, 0xFF, 0x99, 0xFF, 0x66, 0xC3, 0x66 } }
};

static long playInvaders(const GameOpt* o) {
  jtTh = &jtThemes[o[2].val];
  const uint16_t bg = jtTh->bg;
  const int mode = o[0].val;
  const bool useShields = (o[1].val == 0);
  static const int livesStart[3] = { 5, 3, 2 };
  static const int fireGap[3] = { 1100, 800, 550 };
  static const int maxEB[3] = { 2, 3, 4 };
  const int COLS = 8, ROWS = 5, HUD = 12, RH = 11;
  const uint16_t RED = JT_RGB(255, 70, 50);
  uint16_t rowCol[ROWS] = { jtTh->c2, jtTh->c3, jtTh->c3, jtTh->c1, jtTh->c1 };
  int rowType[ROWS] = { 0, 1, 1, 2, 2 };

  int W = tft.width(), H = tft.height();
  int cw = 14, fx = 0, fy = 0, dirX = 1, anim = 0;
  bool alive[ROWS][COLS];
  int aliveN = 0, wave = 0;
  float cxp = W / 2.0f;
  int cy = H - 14;
  struct { float x, y; bool on; } pbul = { 0, 0, false };
  struct { float x, y, vy; bool on; } eb[4];
  memset(eb, 0, sizeof(eb));
  bool shield[4][3][4];
  int lives = livesStart[mode], invuln = 0;
  uint32_t score = 0, frame = 0;
  float ufoX = 0; bool ufoOn = false; int ufoDir = 1; unsigned long nextUfo = millis() + 12000;
  int popX = 0, popY = 0, popT = 0; char popTxt[8] = "";
  unsigned long lastStep = 0, lastShot = 0;
  int bunkerY = 0;

  auto bunkerX = [&](int k) -> int { return (int)(W * (k * 2 + 1) / 8) - 6; };
  auto drawBunker = [&](int k) {
    for (int r = 0; r < 3; r++) for (int c = 0; c < 4; c++) {
      int x = bunkerX(k) + c * 3, y = bunkerY + r * 3;
      tft.fillRect(x, y, 3, 3, shield[k][r][c] ? jtTh->c4 : bg);
    }
  };
  auto resetShields = [&]() {
    for (int k = 0; k < 4; k++) for (int r = 0; r < 3; r++) for (int c = 0; c < 4; c++) shield[k][r][c] = useShields;
    if (useShields) for (int k = 0; k < 4; k++) { shield[k][2][1] = false; shield[k][2][2] = false; }
  };
  auto drawCannon = [&](int x, uint16_t c) {
    tft.fillRect(x - 6, cy + 3, 13, 4, c);
    tft.fillRect(x - 4, cy + 1, 9, 3, c);
    tft.fillRect(x - 1, cy - 2, 3, 4, c);
  };
  auto drawInv = [&](int r, int c) {
    int x = fx + c * cw + (cw - 8) / 2, y = fy + r * RH;
    tft.drawBitmap(x, y, INV_BMP[rowType[r]][anim], 8, 8, rowCol[r]);
  };
  auto eraseFormation = [&]() { fastFillRect(fx, fy, COLS * cw, ROWS * RH, bg); };
  auto drawFormation = [&]() {
    for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) if (alive[r][c]) drawInv(r, c);
  };
  auto drawHud = [&]() {
    tft.fillRect(0, 0, W, HUD, jtBlend(bg, jtTh->c1, 35));
    tft.drawFastHLine(0, HUD, W, jtBlend(bg, jtTh->c1, 140));
    char b[14]; snprintf(b, sizeof(b), "%lu", (unsigned long)score);
    gText(b, 2, 2, 1, ST77XX_WHITE);
    snprintf(b, sizeof(b), "W%d", wave);
    gText(b, 52, 2, 1, jtTh->c3);
    for (int i = 0; i < lives && i < 5; i++) gHeart(W - 3 - (i + 1) * 9, 2, RED);
  };
  auto startWave = [&]() {
    wave++;
    for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) alive[r][c] = true;
    aliveN = ROWS * COLS;
    cw = (W - 16) / COLS; if (cw > 16) cw = 16;
    fx = (W - COLS * cw) / 2; fy = HUD + 10 + ((wave - 1 > 5 ? 5 : wave - 1) * 4); dirX = 1; anim = 0;
    pbul.on = false; for (int i = 0; i < 4; i++) eb[i].on = false;
    jtFillScreen(bg);
    bunkerY = cy - 26;
    resetShields();
    if (useShields) for (int k = 0; k < 4; k++) drawBunker(k);
    drawFormation(); drawCannon((int)cxp, jtTh->c1); drawHud();
  };

  startWave();
  gameFlushButtons();
  unsigned long lastFrame = 0;
  bool fireReq = false;

  while (true) {
    if (backPressed()) return -1;
    if (rotatePressed()) {
      changeRotation(); W = tft.width(); H = tft.height(); cy = H - 14;
      if (cxp > W - 8) cxp = W - 8;
      wave--; startWave();
    }
    if (joyPressed()) fireReq = true;
    if (millis() - lastFrame < 33) { delay(1); continue; }
    lastFrame = millis(); frame++;
    JoyDir d = readJoyDirection();
    unsigned long now = millis();

    // ---- erase moving things ----
    gClear((int)cxp - 8, cy - 3, 17, 10, HUD + 1, bg);
    if (pbul.on) gClear((int)pbul.x - 1, (int)pbul.y - 1, 3, 7, HUD + 1, bg);
    for (int i = 0; i < 4; i++) if (eb[i].on) gClear((int)eb[i].x - 2, (int)eb[i].y - 1, 5, 8, HUD + 1, bg);
    if (ufoOn) gClear((int)ufoX - 9, HUD + 1, 19, 9, HUD + 1, bg);
    if (popT > 0) gClear(popX - 2, popY - 1, 30, 10, HUD + 1, bg);

    // ---- player ----
    cxp += d.x * 3.0f;
    if (cxp < 8) cxp = 8; if (cxp > W - 8) cxp = W - 8;
    if (fireReq) { fireReq = false; if (!pbul.on) { pbul.on = true; pbul.x = cxp; pbul.y = cy - 3; } }
    if (invuln > 0) invuln--;

    // ---- player bullet ----
    if (pbul.on) {
      pbul.y -= 6;
      if (pbul.y < HUD + 2) pbul.on = false;
      // shields
      if (pbul.on && useShields && pbul.y < bunkerY + 9 && pbul.y > bunkerY - 2) {
        for (int k = 0; k < 4 && pbul.on; k++) {
          int rx = (int)pbul.x - bunkerX(k), ry = (int)pbul.y - bunkerY;
          if (rx >= 0 && rx < 12 && ry >= 0 && ry < 9 && shield[k][ry / 3][rx / 3]) {
            shield[k][ry / 3][rx / 3] = false; drawBunker(k); pbul.on = false;
          }
        }
      }
      // invaders
      if (pbul.on) {
        int c = ((int)pbul.x - fx) / cw, r = ((int)pbul.y - fy) / RH;
        if ((int)pbul.x >= fx && (int)pbul.y >= fy && c >= 0 && c < COLS && r >= 0 && r < ROWS && alive[r][c]) {
          int sx = fx + c * cw + (cw - 8) / 2;
          if ((int)pbul.x >= sx - 1 && (int)pbul.x <= sx + 8 && ((int)pbul.y - fy) % RH < 8) {
            alive[r][c] = false; aliveN--; pbul.on = false;
            tft.fillRect(sx, fy + r * RH, 8, 8, bg);
            score += (r == 0) ? 30 : (r < 3 ? 20 : 10);
            tft.fillCircle(sx + 4, fy + r * RH + 4, 4, 0xFFFF); delay(18);
            tft.fillRect(sx - 1, fy + r * RH - 1, 10, 10, bg);
          }
        }
      }
      // UFO
      if (pbul.on && ufoOn && pbul.y < HUD + 12 && fabsf(pbul.x - ufoX) < 9) {
        int pts = 50 * (1 + random(3)); score += pts; ufoOn = false; pbul.on = false;
        snprintf(popTxt, sizeof(popTxt), "+%d", pts); popX = (int)ufoX - 8; popY = HUD + 3; popT = 25;
      }
    }

    // ---- invader march ----
    int stepMs = 26 + aliveN * 13;
    stepMs = stepMs * (100 - (wave - 1 > 6 ? 6 : wave - 1) * 6) / 100;
    stepMs = stepMs * (mode == 0 ? 115 : mode == 2 ? 85 : 100) / 100;
    if (stepMs < 18) stepMs = 18;
    if (now - lastStep >= (unsigned long)stepMs && aliveN > 0) {
      lastStep = now;
      int minC = COLS, maxC = -1, maxR = -1;
      for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) if (alive[r][c]) {
        if (c < minC) minC = c; if (c > maxC) maxC = c; if (r > maxR) maxR = r;
      }
      eraseFormation();
      int left = fx + minC * cw, right = fx + (maxC + 1) * cw;
      if ((dirX > 0 && right + 3 > W - 2) || (dirX < 0 && left - 3 < 2)) { fy += 6; dirX = -dirX; }
      else fx += dirX * 3;
      anim ^= 1;
      drawFormation();
      if (useShields && fy + (maxR + 1) * RH > bunkerY - 4) for (int k = 0; k < 4; k++) drawBunker(k);
      if (fy + (maxR + 1) * RH >= cy - 2) {
        for (int k = 0; k < 8; k++) { tft.fillCircle((int)cxp + random(-6, 7), cy + random(-3, 4), 4, k & 1 ? (uint16_t)JT_ORANGE : RED); delay(50); }
        return (long)score;
      }
    }

    // ---- enemy fire ----
    int ebCount = 0; for (int i = 0; i < 4; i++) if (eb[i].on) ebCount++;
    if (aliveN > 0 && ebCount < maxEB[mode] && now - lastShot > (unsigned long)(fireGap[mode] - (wave > 8 ? 8 : wave) * 40) + random(0, 400)) {
      lastShot = now;
      int c = random(COLS), tries = 0;
      while (tries++ < COLS) {
        int rr = -1; for (int r = ROWS - 1; r >= 0; r--) if (alive[r][c]) { rr = r; break; }
        if (rr >= 0) {
          for (int i = 0; i < 4; i++) if (!eb[i].on) {
            eb[i].on = true; eb[i].x = fx + c * cw + cw / 2.0f; eb[i].y = fy + rr * RH + 8; eb[i].vy = 2.0f + wave * 0.1f + mode * 0.2f; break;
          }
          break;
        }
        c = (c + 1) % COLS;
      }
    }
    for (int i = 0; i < 4; i++) if (eb[i].on) {
      eb[i].y += eb[i].vy;
      if (eb[i].y > H) { eb[i].on = false; continue; }
      if (useShields && eb[i].y > bunkerY - 1 && eb[i].y < bunkerY + 10) {
        for (int k = 0; k < 4 && eb[i].on; k++) {
          int rx = (int)eb[i].x - bunkerX(k), ry = (int)eb[i].y + 3 - bunkerY;
          if (rx >= 0 && rx < 12 && ry >= 0 && ry < 9 && shield[k][ry / 3][rx / 3]) {
            shield[k][ry / 3][rx / 3] = false; drawBunker(k); eb[i].on = false;
          }
        }
        if (!eb[i].on) continue;
      }
      if (invuln == 0 && eb[i].y + 4 >= cy && eb[i].y <= cy + 6 && fabsf(eb[i].x - cxp) < 7) {
        eb[i].on = false; lives--; invuln = 70;
        for (int k = 0; k < 4; k++) { tft.fillCircle((int)cxp, cy + 3, 3 + k * 2, k & 1 ? (uint16_t)JT_ORANGE : (uint16_t)ST77XX_YELLOW); delay(45); }
        gClear((int)cxp - 12, cy - 6, 25, 18, HUD + 1, bg);
        if (lives <= 0) return (long)score;
        drawHud();
      }
    }

    // ---- UFO ----
    if (!ufoOn && now > nextUfo) { ufoOn = true; ufoDir = random(2) ? 1 : -1; ufoX = ufoDir > 0 ? -8.0f : (float)(W + 8); nextUfo = now + 14000 + random(0, 8000); }
    if (ufoOn) { ufoX += ufoDir * 1.6f; if (ufoX < -10 || ufoX > W + 10) ufoOn = false; }
    if (popT > 0) popT--;

    // ---- wave cleared ----
    if (aliveN == 0) {
      gTextC("WAVE CLEAR!", W / 2, H / 2 - 4, 1, jtTh->c3);
      delay(900);
      score += 100 * wave;
      startWave();
      continue;
    }

    // ---- draw ----
    if (!(invuln > 0 && (frame & 2))) drawCannon((int)cxp, jtTh->c1);
    if (pbul.on) tft.fillRect((int)pbul.x, (int)pbul.y - 1, 1, 5, 0xFFFF);
    for (int i = 0; i < 4; i++) if (eb[i].on) {
      int x = (int)eb[i].x, y = (int)eb[i].y;
      tft.drawFastVLine(x, y, 6, RED); tft.drawFastVLine(x + ((frame & 2) ? 1 : -1), y + 1, 4, RED);
    }
    if (ufoOn) {
      int x = (int)ufoX;
      tft.fillRoundRect(x - 7, HUD + 4, 15, 5, 2, RED);
      tft.fillRect(x - 3, HUD + 2, 7, 3, jtBlend(RED, 0xFFFF, 120));
      tft.drawPixel(x - 4 + (int)((frame >> 1) % 9), HUD + 6, 0xFFFF);
    }
    if (popT > 0) gText(popTxt, popX, popY, 1, JT_GOLD);
    if ((frame & 7) == 0) drawHud();
  }
}

static void gameInvaders() {
  GameOpt o[3] = {
    { "Mode",    { "Easy", "Normal", "Hard" }, 3, 1 },
    { "Shields", { "On", "Off" }, 2, 0 },
    { "Theme",   { "Neon", "Retro", "Sunset", "Ocean" }, 4, 1 }
  };
  while (true) {
    if (!gameSetup("INVADERS", "SW fire  Move L/R", ST77XX_GREEN, "iv", o, 3)) return;
    while (true) {
      long sc = gRun(playInvaders, o);
      if (sc < 0) break;
      if (!gameOverScreen("Invaders", jtTh->c1, (uint32_t)sc, "iv")) break;
    }
  }
}

// ============================================================================
//  9. TANK BATTLE
// ============================================================================
struct TkTank   { int16_t x, y; int8_t dir, hp; uint8_t type, cd; bool on; int16_t ai; float acc; };
struct TkBullet { int16_t x, y; int8_t dx, dy; int8_t owner; bool on; };
struct TkExp    { int16_t x, y; uint8_t age; bool on; };

static long playTankBattle(const GameOpt* o) {
  jtTh = &jtThemes[o[2].val];
  const uint16_t bg = jtTh->bg;
  const int mode = o[0].val, style = o[1].val;
  static const int livesStart[3] = { 5, 3, 2 };
  static const int enemyCd[3] = { 70, 50, 32 };
  static const float enemySpd[3] = { 0.8f, 1.0f, 1.2f };
  const int HUD = 12, CS = 8;
  const uint16_t RED = JT_RGB(255, 70, 50);
  const uint16_t brickCol = jtBlend(JT_RGB(190, 80, 40), bg, 40);
  const uint16_t brickDk  = jtBlend(brickCol, 0, 130);
  const uint16_t steelCol = JT_RGB(150, 156, 175);

  int W = 0, H = 0, cols = 0, rows = 0, ax = 0, ay = 0;
  uint8_t map[18][20];
  TkTank tk[5];          // 0 = player
  TkBullet bl[7];
  TkExp ex[6];
  memset(tk, 0, sizeof(tk)); memset(bl, 0, sizeof(bl)); memset(ex, 0, sizeof(ex));
  int lives = livesStart[mode], stage = 0, invuln = 0, toSpawn = 0, spawnT = 0, enemiesLeft = 0, pcd = 0;
  uint32_t score = 0, frame = 0;

  auto drawCell = [&](int cx, int cy) {
    int x = ax + cx * CS, y = ay + cy * CS;
    uint8_t m = map[cy][cx];
    if (m == 1) {
      tft.fillRect(x, y, CS, CS, brickCol);
      tft.drawFastHLine(x, y + 3, CS, brickDk); tft.drawFastHLine(x, y + 7, CS, brickDk);
      tft.drawFastVLine(x + 3, y, 3, brickDk); tft.drawFastVLine(x + 6, y + 4, 3, brickDk);
    } else if (m == 2) {
      tft.fillRect(x, y, CS, CS, steelCol);
      tft.drawRect(x, y, CS, CS, jtBlend(steelCol, 0, 120));
      tft.fillRect(x + 2, y + 2, 4, 4, jtBlend(steelCol, 0xFFFF, 110));
    } else tft.fillRect(x, y, CS, CS, bg);
  };
  auto eraseArea = [&](int lx, int ly, int w, int h) {   // arena-local rect
    int sx = ax + lx, sy = ay + ly;
    gClear(sx, sy, w, h, HUD + 1, bg);
    int c0 = lx / CS, c1 = (lx + w - 1) / CS, r0 = ly / CS, r1 = (ly + h - 1) / CS;
    if (c0 < 0) c0 = 0; if (r0 < 0) r0 = 0; if (c1 >= cols) c1 = cols - 1; if (r1 >= rows) r1 = rows - 1;
    for (int r = r0; r <= r1; r++) for (int c = c0; c <= c1; c++) if (map[r][c]) drawCell(c, r);
  };
  auto blockedRect = [&](int lx, int ly, int w, int h) -> bool {
    if (lx < 0 || ly < 0 || lx + w > cols * CS || ly + h > rows * CS) return true;
    int c0 = lx / CS, c1 = (lx + w - 1) / CS, r0 = ly / CS, r1 = (ly + h - 1) / CS;
    for (int r = r0; r <= r1; r++) for (int c = c0; c <= c1; c++) if (map[r][c]) return true;
    return false;
  };
  auto tankOverlap = [&](int self, int lx, int ly) -> bool {
    for (int i = 0; i < 5; i++) if (i != self && tk[i].on) {
      if (abs(tk[i].x - lx) < 7 && abs(tk[i].y - ly) < 7) return true;
    }
    return false;
  };
  auto drawTank = [&](const TkTank &t, uint16_t body, uint16_t gun) {
    int x = ax + t.x, y = ay + t.y;
    uint16_t trk = jtBlend(body, 0, 140);
    tft.fillRect(x, y, 7, 7, body);
    if (t.dir == 0 || t.dir == 2) { tft.fillRect(x, y, 2, 7, trk); tft.fillRect(x + 5, y, 2, 7, trk); }
    else { tft.fillRect(x, y, 7, 2, trk); tft.fillRect(x, y + 5, 7, 2, trk); }
    tft.fillRect(x + 2, y + 2, 3, 3, gun);
    switch (t.dir) {
      case 0: tft.fillRect(x + 3, y, 1, 3, gun); break;
      case 1: tft.fillRect(x + 4, y + 3, 3, 1, gun); break;
      case 2: tft.fillRect(x + 3, y + 4, 1, 3, gun); break;
      default: tft.fillRect(x, y + 3, 3, 1, gun);
    }
  };
  auto enemyColor = [&](const TkTank &t) -> uint16_t {
    if (t.type == 2) return t.hp >= 3 ? jtTh->c3 : (t.hp == 2 ? jtTh->c2 : RED);
    if (t.type == 1) return jtTh->c3;
    return jtTh->c2;
  };
  auto drawHud = [&]() {
    tft.fillRect(0, 0, W, HUD, jtBlend(bg, jtTh->c1, 35));
    tft.drawFastHLine(0, HUD, W, jtBlend(bg, jtTh->c1, 140));
    char b[16]; snprintf(b, sizeof(b), "%lu", (unsigned long)score);
    gText(b, 2, 2, 1, ST77XX_WHITE);
    snprintf(b, sizeof(b), "S%d E%d", stage, enemiesLeft);
    gText(b, 44, 2, 1, jtTh->c3);
    for (int i = 0; i < lives && i < 5; i++) gHeart(W - 3 - (i + 1) * 9, 2, RED);
  };
  auto addExp = [&](int lx, int ly) {
    for (int i = 0; i < 6; i++) if (!ex[i].on) { ex[i].on = true; ex[i].x = lx; ex[i].y = ly; ex[i].age = 0; return; }
  };
  auto fire = [&](int ti) {
    for (int i = 0; i < 7; i++) if (!bl[i].on) {
      static const int8_t DX[4] = { 0, 1, 0, -1 }, DY[4] = { -1, 0, 1, 0 };
      TkTank &t = tk[ti];
      bl[i].on = true; bl[i].owner = ti; bl[i].dx = DX[t.dir]; bl[i].dy = DY[t.dir];
      bl[i].x = t.x + 3 + DX[t.dir] * 4 - 0; bl[i].y = t.y + 3 + DY[t.dir] * 4 - 0;
      return;
    }
  };
  auto ownerBullets = [&](int ti) -> int { int n = 0; for (int i = 0; i < 7; i++) if (bl[i].on && bl[i].owner == ti) n++; return n; };

  auto buildMap = [&]() {
    memset(map, 0, sizeof(map));
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++) {
      int rv = random(100);
      if (style == 0) map[r][c] = (rv < 24) ? 1 : (rv < 28) ? 2 : 0;
      else if (style == 1) map[r][c] = ((c & 1) && (r & 1)) ? 2 : ((rv < 38) ? 1 : 0);
      else map[r][c] = (rv < 8) ? 1 : (rv < 14) ? 2 : 0;
    }
    // keep spawn areas clear
    for (int c = 0; c < cols; c++) { map[0][c] = 0; map[rows - 1][c] = 0; }
    for (int c = 0; c < cols; c++) { if (c < 3 || (c >= cols / 2 - 1 && c <= cols / 2 + 1) || c >= cols - 3) map[1][c] = 0; }
    for (int c = cols / 2 - 2; c <= cols / 2 + 2; c++) { map[rows - 2][c] = 0; }
  };
  auto drawMap = [&]() {
    jtFillScreen(bg);
    tft.drawRect(ax - 1, ay - 1, cols * CS + 2, rows * CS + 2, jtBlend(bg, jtTh->c1, 130));
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++) if (map[r][c]) drawCell(c, r);
    drawHud();
  };
  auto spawnPlayer = [&]() {
    tk[0].on = true; tk[0].x = (cols / 2) * CS; tk[0].y = (rows - 1) * CS; tk[0].dir = 0; tk[0].hp = 1; tk[0].acc = 0; tk[0].cd = 0;
    invuln = 90;
  };
  auto layout = [&]() {
    W = tft.width(); H = tft.height();
    cols = (W - 2) / CS; if (cols > 20) cols = 20;
    rows = (H - HUD - 3) / CS; if (rows > 18) rows = 18;
    ax = (W - cols * CS) / 2; ay = HUD + 2 + ((H - HUD - 3) - rows * CS) / 2;
  };
  auto startStage = [&]() {
    stage++;
    layout();
    buildMap();
    toSpawn = 6 + stage * 2; if (toSpawn > 24) toSpawn = 24;
    enemiesLeft = toSpawn; spawnT = 20;
    for (int i = 1; i < 5; i++) tk[i].on = false;
    for (int i = 0; i < 7; i++) bl[i].on = false;
    for (int i = 0; i < 6; i++) ex[i].on = false;
    spawnPlayer();
    drawMap();
  };

  startStage();
  gameFlushButtons();
  unsigned long lastFrame = 0;
  bool fireReq = false;
  static const int8_t DXs[4] = { 0, 1, 0, -1 }, DYs[4] = { -1, 0, 1, 0 };

  // move a tank up to `steps` px in direction dir, handling corner snapping
  auto moveTank = [&](int ti, int dir, int steps) -> bool {
    TkTank &t = tk[ti];
    if (((t.dir & 1) != (dir & 1))) {   // turning to the other axis: snap to grid
      int nx = t.x, ny = t.y;
      if (dir & 1) ny = ((t.y + 4) / CS) * CS; else nx = ((t.x + 4) / CS) * CS;
      if (!blockedRect(nx, ny, 7, 7) && !tankOverlap(ti, nx, ny)) { t.x = nx; t.y = ny; }
    }
    t.dir = dir;
    bool moved = false;
    for (int s = 0; s < steps; s++) {
      int nx = t.x + DXs[dir], ny = t.y + DYs[dir];
      if (blockedRect(nx, ny, 7, 7) || tankOverlap(ti, nx, ny)) return moved;
      t.x = nx; t.y = ny; moved = true;
    }
    return moved;
  };

  while (true) {
    if (backPressed()) return -1;
    if (rotatePressed()) { changeRotation(); stage--; startStage(); }
    if (joyPressed()) fireReq = true;
    if (millis() - lastFrame < 33) { delay(1); continue; }
    lastFrame = millis(); frame++;
    JoyDir d = readJoyDirection();

    // ---- erase ----
    for (int i = 0; i < 5; i++) if (tk[i].on) eraseArea(tk[i].x, tk[i].y, 7, 7);
    for (int i = 0; i < 7; i++) if (bl[i].on) eraseArea(bl[i].x - 1, bl[i].y - 1, 3, 3);
    for (int i = 0; i < 6; i++) if (ex[i].on) { int r = 2 + ex[i].age; eraseArea(ex[i].x - r - 1, ex[i].y - r - 1, 2 * r + 3, 2 * r + 3); }

    // ---- player ----
    if (tk[0].on) {
      int dir = -1;
      if (d.x != 0) dir = d.x > 0 ? 1 : 3; else if (d.y != 0) dir = d.y > 0 ? 2 : 0;
      if (dir >= 0) {
        tk[0].acc += 1.5f; int st = (int)tk[0].acc; tk[0].acc -= st;
        if (st < 1 && ((tk[0].dir & 1) != (dir & 1))) st = 0;
        moveTank(0, dir, st);
      } else tk[0].acc = 0;
      if (pcd > 0) pcd--;
      if (fireReq && pcd == 0 && ownerBullets(0) < 2) { fire(0); pcd = 12; }
    }
    fireReq = false;
    if (invuln > 0) invuln--;

    // ---- spawn enemies ----
    int aliveE = 0; for (int i = 1; i < 5; i++) if (tk[i].on) aliveE++;
    int maxE = 3 + (stage > 3 ? 1 : 0);
    if (toSpawn > 0 && aliveE < maxE && --spawnT <= 0) {
      spawnT = 45;
      int sp = random(3), sx = (sp == 0) ? 0 : (sp == 1) ? (cols / 2) * CS : (cols - 1) * CS;
      for (int i = 1; i < 5; i++) if (!tk[i].on) {
        if (!blockedRect(sx, 0, 7, 7) && !tankOverlap(i, sx, 0)) {
          tk[i].on = true; tk[i].x = sx; tk[i].y = 0; tk[i].dir = 2; tk[i].ai = 0; tk[i].cd = 30; tk[i].acc = 0;
          int r = random(100);
          tk[i].type = (stage >= 3 && r < 25) ? 2 : (stage >= 2 && r < 55) ? 1 : 0;
          tk[i].hp = (tk[i].type == 2) ? 3 : 1;
          toSpawn--;
        }
        break;
      }
    }

    // ---- enemy AI ----
    for (int i = 1; i < 5; i++) if (tk[i].on) {
      TkTank &t = tk[i];
      if (--t.ai <= 0) {
        int nd;
        if (random(100) < 40 && tk[0].on) {
          int ddx = tk[0].x - t.x, ddy = tk[0].y - t.y;
          nd = (abs(ddx) > abs(ddy)) ? (ddx > 0 ? 1 : 3) : (ddy > 0 ? 2 : 0);
        } else nd = (random(100) < 45) ? 2 : random(4);
        t.dir = nd; t.ai = 25 + random(0, 55);
      }
      float sp = enemySpd[mode] * (t.type == 1 ? 1.6f : (t.type == 2 ? 0.9f : 1.0f));
      t.acc += sp; int st = (int)t.acc; t.acc -= st;
      if (st > 0 && !moveTank(i, t.dir, st)) t.ai = 0;
      if (t.cd > 0) t.cd--;
      if (t.cd == 0 && ownerBullets(i) == 0) {
        bool aligned = false;
        if (tk[0].on) {
          if ((t.dir & 1) == 0 && abs(tk[0].x - t.x) < 6 && ((t.dir == 2 && tk[0].y > t.y) || (t.dir == 0 && tk[0].y < t.y))) aligned = true;
          if ((t.dir & 1) == 1 && abs(tk[0].y - t.y) < 6 && ((t.dir == 1 && tk[0].x > t.x) || (t.dir == 3 && tk[0].x < t.x))) aligned = true;
        }
        if (aligned || random(90) == 0) { fire(i); t.cd = enemyCd[mode] + random(0, 20); }
      }
    }

    // ---- bullets (3 px per frame in 1 px steps) ----
    for (int i = 0; i < 7; i++) if (bl[i].on) {
      TkBullet &b = bl[i];
      for (int s = 0; s < 3 && b.on; s++) {
        b.x += b.dx; b.y += b.dy;
        int lx = b.x, ly = b.y;
        if (lx < 0 || ly < 0 || lx >= cols * CS || ly >= rows * CS) { b.on = false; break; }
        int cx = lx / CS, cy = ly / CS;
        if (map[cy][cx] == 1) { map[cy][cx] = 0; drawCell(cx, cy); b.on = false; if (b.owner == 0) score += 2; break; }
        if (map[cy][cx] == 2) { b.on = false; addExp(lx, ly); break; }
        for (int k = 0; k < 5 && b.on; k++) if (tk[k].on && ((b.owner == 0) != (k == 0))) {
          if (lx >= tk[k].x && lx < tk[k].x + 7 && ly >= tk[k].y && ly < tk[k].y + 7) {
            b.on = false;
            if (k == 0) {
              if (invuln == 0) {
                lives--; addExp(tk[0].x + 3, tk[0].y + 3); tk[0].on = false;
                if (lives > 0) { eraseArea(tk[0].x, tk[0].y, 7, 7); spawnPlayer(); }
                drawHud();
              }
            } else {
              tk[k].hp--;
              if (tk[k].hp <= 0) {
                tk[k].on = false; enemiesLeft--;
                addExp(tk[k].x + 3, tk[k].y + 3);
                score += (tk[k].type == 2) ? 40 : (tk[k].type == 1 ? 20 : 10);
                drawHud();
              } else score += 5;
            }
          }
        }
        // bullet vs bullet
        for (int k = 0; k < 7 && b.on; k++) if (k != i && bl[k].on && ((bl[k].owner == 0) != (b.owner == 0)) && abs(bl[k].x - b.x) < 3 && abs(bl[k].y - b.y) < 3) {
          bl[k].on = false; b.on = false;
        }
      }
    }
    for (int i = 0; i < 6; i++) if (ex[i].on) { if (++ex[i].age > 5) ex[i].on = false; }

    // ---- game over / stage clear ----
    if (lives <= 0) {
      for (int k = 0; k < 6; k++) { tft.fillCircle(ax + tk[0].x + random(-6, 7), ay + tk[0].y + random(-6, 7), 4, k & 1 ? (uint16_t)JT_ORANGE : RED); delay(60); }
      return (long)score;
    }
    if (enemiesLeft <= 0) {
      gTextC("STAGE CLEAR", W / 2, H / 2 - 4, 1, jtTh->c3);
      delay(1000); score += 50 * stage;
      startStage(); continue;
    }

    // ---- draw ----
    for (int i = 1; i < 5; i++) if (tk[i].on) drawTank(tk[i], enemyColor(tk[i]), 0xFFFF);
    if (tk[0].on && !(invuln > 0 && (frame & 2))) drawTank(tk[0], jtTh->c4, jtBlend(jtTh->c4, 0xFFFF, 160));
    for (int i = 0; i < 7; i++) if (bl[i].on) tft.fillRect(ax + bl[i].x - 1 + 0, ay + bl[i].y - 1, 2, 2, bl[i].owner == 0 ? (uint16_t)0xFFFF : (uint16_t)RED);
    for (int i = 0; i < 6; i++) if (ex[i].on) {
      int r = 1 + ex[i].age;
      tft.fillCircle(ax + ex[i].x, ay + ex[i].y, r, ex[i].age < 2 ? (uint16_t)0xFFFF : (ex[i].age < 4 ? (uint16_t)ST77XX_YELLOW : (uint16_t)JT_ORANGE));
    }
  }
}

static void gameTankBattle() {
  GameOpt o[3] = {
    { "Mode",  { "Easy", "Normal", "Hard" }, 3, 1 },
    { "Map",   { "Bricks", "Maze", "Open" }, 3, 0 },
    { "Theme", { "Neon", "Retro", "Sunset", "Ocean" }, 4, 1 }
  };
  while (true) {
    if (!gameSetup("TANK BATTLE", "SW fire  Clear stages", ST77XX_GREEN, "tb", o, 3)) return;
    while (true) {
      long sc = gRun(playTankBattle, o);
      if (sc < 0) break;
      if (!gameOverScreen("Tank Battle", jtTh->c1, (uint32_t)sc, "tb")) break;
    }
  }
}

// ============================================================================
//  10. SKY HUNTER  (shooting gallery)
// ============================================================================
struct ShTarget { float x, y, vx, vy; uint8_t type, col; bool on; uint16_t t; };
struct ShPop    { int16_t x, y; uint8_t age; char txt[8]; uint16_t col; bool on; };

static long playSkyHunter(const GameOpt* o) {
  jtTh = &jtThemes[o[2].val];
  const uint16_t bg = jtTh->bg;
  const int mode = o[1].val;
  static const long timeMs[3] = { 30000, 60000, 90000 };
  static const float speedMul[3] = { 0.8f, 1.0f, 1.3f };
  static const int decoyPct[3] = { 0, 12, 20 };
  const int HUD = 12;
  const uint16_t RED = JT_RGB(255, 70, 50);
  const uint16_t skyTop = jtBlend(bg, jtTh->c1, 40), skyBot = jtBlend(bg, jtTh->c2, 150);
  const uint16_t groundCol = jtBlend(JT_RGB(20, 80, 40), bg, 110);

  int W = 0, H = 0, groundY = 0;
  ShTarget tg[7]; ShPop pp[4];
  memset(tg, 0, sizeof(tg)); memset(pp, 0, sizeof(pp));
  float cx = 64, cy = 70, cvx = 0, cvy = 0;
  int ammo = 6, reload = 0, streak = 0, flash = 0, spawnT = 20, hitFlashT = 0;
  long timeLeft = timeMs[o[0].val];
  uint32_t score = 0, frame = 0;
  int pcx = 64, pcy = 70;

  auto bandCol = [&](int band) -> uint16_t {
    int nb = (groundY - HUD - 1 + 7) / 8; if (nb < 1) nb = 1;
    return jtBlend(skyTop, skyBot, band * 255 / nb);
  };
  auto skyFill = [&](int x, int y, int w, int h) {
    if (x < 0) { w += x; x = 0; }
    if (x + w > W) w = W - x;
    if (y < HUD + 1) { h -= (HUD + 1 - y); y = HUD + 1; }
    if (y + h > groundY) h = groundY - y;
    if (w <= 0 || h <= 0) return;
    int yy = y;
    while (yy < y + h) {
      int band = (yy - HUD - 1) / 8;
      int bandEnd = HUD + 1 + (band + 1) * 8;
      int hh = ((bandEnd < y + h) ? bandEnd : y + h) - yy;
      tft.fillRect(x, yy, w, hh, bandCol(band));
      yy += hh;
    }
  };
  auto drawScene = [&]() {
    W = tft.width(); H = tft.height(); groundY = H - 34;
    skyFill(0, HUD + 1, W, groundY - HUD - 1);
    fastFillRect(0, groundY, W, H - groundY, groundCol);
    // rolling hills + trees
    tft.fillCircle(W / 4, groundY + 12, 26, jtBlend(groundCol, 0xFFFF, 25));
    tft.fillCircle(W * 3 / 4, groundY + 16, 30, jtBlend(groundCol, 0xFFFF, 40));
    fastFillRect(0, groundY + 14, W, H - groundY - 14, groundCol);
    for (int i = 0; i < 6; i++) {
      int x = 8 + i * (W - 16) / 5, y = groundY + 6 + (i & 1) * 6;
      tft.fillTriangle(x, y - 8, x - 5, y + 4, x + 5, y + 4, jtBlend(groundCol, 0, 110));
      tft.drawFastVLine(x, y + 4, 4, JT_RGB(70, 45, 20));
    }
  };
  auto drawHud = [&]() {
    tft.fillRect(0, 0, W, HUD, jtBlend(bg, jtTh->c1, 35));
    tft.drawFastHLine(0, HUD, W, jtBlend(bg, jtTh->c1, 140));
    char b[14]; snprintf(b, sizeof(b), "%lu", (unsigned long)score);
    gText(b, 2, 2, 1, ST77XX_WHITE);
    snprintf(b, sizeof(b), "%lds", timeLeft / 1000 + 1);
    gText(b, 44, 2, 1, timeLeft < 8000 ? RED : jtTh->c3);
    if (streak >= 3) { snprintf(b, sizeof(b), "x%d", 1 + (streak / 3 > 3 ? 3 : streak / 3)); gText(b, 70, 2, 1, jtTh->c4); }
    if (reload > 0) gText("RELOAD", W - 40, 2, 1, RED);
    else for (int i = 0; i < ammo; i++) tft.fillRect(W - 4 - (i + 1) * 5, 3, 3, 6, JT_GOLD);
  };
  auto addPop = [&](int x, int y, const char* s, uint16_t c) {
    for (int i = 0; i < 4; i++) if (!pp[i].on) { pp[i].on = true; pp[i].x = x; pp[i].y = y; pp[i].age = 0; pp[i].col = c; snprintf(pp[i].txt, sizeof(pp[i].txt), "%s", s); return; }
  };
  static const uint8_t TW[5] = { 14, 10, 10, 10, 14 }, TH[5] = { 10, 16, 6, 12, 10 };

  auto drawTarget = [&](const ShTarget &t) {
    int x = (int)t.x, y = (int)t.y;
    int dir = (t.vx >= 0) ? 1 : -1;
    switch (t.type) {
      case 0: case 4: {
        uint16_t c = (t.type == 4) ? JT_RGB(255, 215, 0) : jtTh->c3;
        uint16_t dk = jtBlend(c, 0, 120);
        tft.fillRoundRect(x - 6, y - 3, 12, 7, 3, c);
        tft.fillCircle(x + dir * 6, y - 4, 3, c);
        tft.fillRect(x + dir * 6 + (dir > 0 ? 3 : -5), y - 4, 2, 2, JT_ORANGE);
        tft.drawPixel(x + dir * 6 + dir, y - 5, 0x0000);
        if ((t.t >> 2) & 1) tft.fillRect(x - 3, y - 6, 6, 3, dk); else tft.fillRect(x - 3, y + 3, 6, 3, dk);
        if (t.type == 4 && ((t.t >> 1) & 1)) tft.drawPixel(x - dir * 4, y - 6, 0xFFFF);
      } break;
      case 1: {
        uint16_t c = (t.col == 0) ? jtTh->c1 : (t.col == 1) ? jtTh->c2 : (t.col == 2) ? jtTh->c3 : jtTh->c4;
        tft.fillCircle(x, y - 4, 6, c);
        tft.fillTriangle(x - 2, y + 4, x + 2, y + 4, x, y + 1, c);
        tft.drawPixel(x - 2, y - 7, 0xFFFF); tft.drawPixel(x - 3, y - 6, 0xFFFF);
        tft.drawFastVLine(x, y + 5, 6, JT_LIGHTGREY);
      } break;
      case 2: {
        uint16_t c = jtTh->c2;
        tft.fillTriangle(x + dir * 5, y, x - dir * 4, y - 3, x - dir * 4, y + 3, c);
        tft.drawPixel(x + dir * 2, y - 1, 0xFFFF);
        if ((t.t >> 1) & 1) tft.drawLine(x - dir * 2, y - 1, x - dir * 4, y - 4, c); else tft.drawLine(x - dir * 2, y + 1, x - dir * 4, y + 4, c);
      } break;
      default: {
        tft.fillCircle(x, y + 2, 5, JT_RGB(45, 45, 60));
        tft.drawPixel(x - 2, y, JT_RGB(130, 130, 150));
        tft.drawLine(x + 2, y - 3, x + 4, y - 6, JT_LIGHTGREY);
        tft.drawPixel(x + 5, y - 7, (t.t >> 1) & 1 ? (uint16_t)ST77XX_YELLOW : (uint16_t)JT_ORANGE);
        tft.fillRect(x - 2, y + 1, 5, 1, RED);
      }
    }
  };
  auto eraseTarget = [&](const ShTarget &t) { skyFill((int)t.x - 10, (int)t.y - 11, 21, 24); };

  auto spawnTarget = [&]() {
    for (int i = 0; i < 7; i++) if (!tg[i].on) {
      ShTarget &t = tg[i];
      t.on = true; t.t = random(30); t.col = random(4);
      int r = random(100);
      int dp = decoyPct[mode];
      if (r < dp) t.type = 3;
      else if (r < dp + 3) t.type = 4;
      else { int q = random(100); t.type = (q < 45) ? 0 : (q < 70) ? 1 : 2; }
      float sp = speedMul[mode];
      int dir = random(2) ? 1 : -1;
      int lo = HUD + 14, hi = groundY - 22;
      switch (t.type) {
        case 1: t.x = 12 + random(W - 24); t.y = groundY - 12; t.vx = 0; t.vy = -(0.8f + random(0, 7) / 10.0f) * sp; break;
        case 2: t.y = lo + random(hi - lo); t.vx = dir * (3.0f + random(0, 15) / 10.0f) * sp; t.vy = 0; t.x = dir > 0 ? -10.0f : W + 10.0f; break;
        case 3: t.y = lo + random(hi - lo); t.vx = dir * (1.2f + random(0, 8) / 10.0f) * sp; t.vy = 0; t.x = dir > 0 ? -10.0f : W + 10.0f; break;
        case 4: t.y = lo + random(hi - lo); t.vx = dir * 2.8f * sp; t.vy = 0; t.x = dir > 0 ? -10.0f : W + 10.0f; break;
        default: t.y = lo + random(hi - lo); t.vx = dir * (1.3f + random(0, 12) / 10.0f) * sp; t.vy = 0; t.x = dir > 0 ? -10.0f : W + 10.0f;
      }
      return;
    }
  };

  drawScene();
  cx = W / 2.0f; cy = groundY / 2.0f; pcx = (int)cx; pcy = (int)cy;
  drawHud();
  gameFlushButtons();
  unsigned long lastFrame = 0, lastTick = millis();
  bool shootReq = false;

  while (true) {
    if (backPressed()) return -1;
    if (rotatePressed()) {
      changeRotation();
      for (int i = 0; i < 7; i++) tg[i].on = false;
      for (int i = 0; i < 4; i++) pp[i].on = false;
      drawScene(); cx = W / 2.0f; cy = groundY / 2.0f; pcx = (int)cx; pcy = (int)cy; drawHud();
    }
    if (joyPressed()) shootReq = true;
    if (millis() - lastFrame < 33) { delay(1); continue; }
    unsigned long now = millis();
    timeLeft -= (long)(now - lastTick); lastTick = now;
    lastFrame = now; frame++;
    JoyDir d = readJoyDirection();

    // ---- erase ----
    skyFill(pcx - 8, pcy - 8, 17, 17);
    for (int i = 0; i < 7; i++) if (tg[i].on) eraseTarget(tg[i]);
    for (int i = 0; i < 4; i++) if (pp[i].on) skyFill(pp[i].x - 1, pp[i].y - 1, 26, 10);

    // ---- crosshair ----
    if (d.x != 0) { cvx += d.x * 0.7f; if (cvx > 4.6f) cvx = 4.6f; if (cvx < -4.6f) cvx = -4.6f; } else cvx *= 0.5f;
    if (d.y != 0) { cvy += d.y * 0.7f; if (cvy > 4.6f) cvy = 4.6f; if (cvy < -4.6f) cvy = -4.6f; } else cvy *= 0.5f;
    cx += cvx; cy += cvy;
    if (cx < 4) cx = 4; if (cx > W - 5) cx = W - 5;
    if (cy < HUD + 5) cy = HUD + 5; if (cy > groundY - 3) cy = groundY - 3;

    // ---- shooting ----
    bool hudDirty = false;
    if (reload > 0) { if (--reload == 0) { ammo = 6; hudDirty = true; } }
    if (shootReq) {
      shootReq = false;
      if (ammo > 0 && reload == 0) {
        ammo--; flash = 3; hudDirty = true;
        int hit = -1;
        for (int i = 6; i >= 0; i--) if (tg[i].on) {
          float hw = TW[tg[i].type] / 2.0f + 3, hh = TH[tg[i].type] / 2.0f + 3;
          float ty = tg[i].y + (tg[i].type == 1 ? -2.0f : 0.0f);
          if (fabsf(cx - tg[i].x) <= hw && fabsf(cy - ty) <= hh) { hit = i; break; }
        }
        if (hit >= 0) {
          ShTarget &t = tg[hit];
          if (t.type == 3) {
            timeLeft -= 3000; streak = 0;
            if ((long)score >= 20) score -= 20; else score = 0;
            addPop((int)t.x - 8, (int)t.y - 6, "-3s", RED);
          } else {
            static const int pts[5] = { 10, 20, 30, 0, 100 };
            int mult = 1 + (streak / 3 > 3 ? 3 : streak / 3);
            int gain = pts[t.type] * mult;
            streak++; score += gain;
            char b[8]; snprintf(b, sizeof(b), "+%d", gain);
            addPop((int)t.x - 8, (int)t.y - 6, b, t.type == 4 ? (uint16_t)JT_GOLD : (uint16_t)0xFFFF);
          }
          t.on = false; hudDirty = true;
        } else { streak = 0; }
        if (ammo == 0) { reload = 20; }
      }
    }

    // ---- targets ----
    for (int i = 0; i < 7; i++) if (tg[i].on) {
      ShTarget &t = tg[i];
      t.t++;
      t.x += t.vx; t.y += t.vy;
      if (t.type == 0 || t.type == 4) t.y += sinf(t.t * 0.15f) * 0.7f;
      if (t.type == 1) t.x += sinf(t.t * 0.1f) * 0.5f;
      if (t.type == 3) t.y += sinf(t.t * 0.12f) * 0.5f;
      if (t.y < HUD + 11 && t.type != 1) t.y = HUD + 11;
      if (t.x < -16 || t.x > W + 16 || t.y < HUD - 14) t.on = false;
    }
    if (--spawnT <= 0) { spawnTarget(); spawnT = (int)((16 + random(0, 22)) / speedMul[mode]); }
    for (int i = 0; i < 4; i++) if (pp[i].on) { pp[i].y -= 1; if (++pp[i].age > 22) pp[i].on = false; }
    if ((frame & 7) == 0) hudDirty = true;
    if (timeLeft <= 0) {
      gTextC("TIME UP!", W / 2, H / 2 - 12, 2, jtTh->c3); delay(900);
      return (long)score;
    }

    // ---- draw ----
    for (int i = 0; i < 7; i++) if (tg[i].on) drawTarget(tg[i]);
    for (int i = 0; i < 4; i++) if (pp[i].on) gText(pp[i].txt, pp[i].x, pp[i].y, 1, pp[i].col);
    int ix = (int)cx, iy = (int)cy;
    uint16_t cc = (flash > 0) ? (uint16_t)0xFFFF : RED;
    tft.drawCircle(ix, iy, 5, cc);
    tft.drawFastHLine(ix - 7, iy, 5, cc); tft.drawFastHLine(ix + 3, iy, 5, cc);
    tft.drawFastVLine(ix, iy - 7, 5, cc); tft.drawFastVLine(ix, iy + 3, 5, cc);
    if (flash > 0) { tft.fillCircle(ix, iy, 2, 0xFFFF); flash--; }
    pcx = ix; pcy = iy;
    if (hudDirty) drawHud();
  }
}

static void gameSkyHunter() {
  GameOpt o[3] = {
    { "Time",  { "30s", "60s", "90s" }, 3, 1 },
    { "Mode",  { "Easy", "Normal", "Hard" }, 3, 1 },
    { "Theme", { "Neon", "Retro", "Sunset", "Ocean" }, 4, 2 }
  };
  while (true) {
    if (!gameSetup("SKY HUNTER", "SW shoot  6 shots", ST77XX_YELLOW, "sh", o, 3)) return;
    while (true) {
      long sc = gRun(playSkyHunter, o);
      if (sc < 0) break;
      if (!gameOverScreen("Sky Hunter", jtTh->c1, (uint32_t)sc, "sh")) break;
    }
  }
}

// ============================================================================
//  EXTRA SHARED HELPERS (new game pack)
// ============================================================================
static inline bool gJoyHeld() { return digitalRead(JOY_SW) == LOW; }

// ============================================================================
//  7. TURBO ROAD  (pseudo-3D arcade racer)
// ============================================================================
struct TrObj { float z, lat, v; uint8_t kind; uint16_t col; bool on; };

static long playTurboRoad(const GameOpt* o) {
  jtTh = &jtThemes[o[2].val];
  const int mode = o[0].val;
  const int rowH = (o[1].val == 0) ? 2 : 4;
  static const int   maxCars[3]  = { 4, 6, 8 };
  static const int   timeStart[3] = { 60, 50, 40 };
  static const int   timeBonus[3] = { 25, 20, 15 };
  const uint16_t bg = jtTh->bg;
  const uint16_t skyTop = jtBlend(bg, jtTh->c2, 110), skyBot = jtBlend(jtBlend(bg, jtTh->c3, 150), jtTh->c2, 60);
  const uint16_t grassA = jtBlend(bg, jtTh->c4, 60), grassB = jtBlend(bg, jtTh->c4, 90);
  const uint16_t roadA = JT_RGB(72, 74, 88), roadB = JT_RGB(62, 64, 78);
  const uint16_t rumA = jtTh->c2, rumB = 0xFFFF;
  const uint16_t RED = JT_RGB(255, 70, 50);
  const int HUD = 13;
  static const uint16_t carCols[6] = { JT_RGB(60, 120, 255), JT_RGB(240, 240, 240), JT_RGB(255, 150, 30), JT_RGB(150, 70, 220), JT_RGB(40, 200, 200), JT_RGB(230, 230, 60) };

  int W = tft.width(), H = tft.height(), yh = 0;
  float roadHalf = 0, hwK = 0;
  float pos = 0, ps = 0, trackPos = 0, curve = 0, curveTarget = 0, dist = 0;
  int timeLeftF = timeStart[mode] * 30;   // in frames
  uint32_t score = 0, frame = 0;
  int crashFx = 0, cpFlash = 0, nextCp = 100, curveT = 120;
  TrObj obj[18];
  memset(obj, 0, sizeof(obj));
  const float ZNEAR = 8.0f;

  auto rowOf = [&](float z) -> float { return 700.0f / z - 1.0f; };

  auto drawSky = [&]() {
    for (int y = HUD; y < yh; y += 3) {
      int h = (y + 3 > yh) ? yh - y : 3;
      fastFillRect(0, y, W, h, jtBlend(skyTop, skyBot, (y - HUD) * 255 / (yh - HUD)));
    }
    // sun + distant hills
    tft.fillCircle(W / 2, yh - 6, 14, jtBlend(jtTh->c3, 0xFFFF, 60));
    tft.fillRect(0, yh - 5, W, 5, jtBlend(bg, jtTh->c1, 40));
    for (int x = 0; x < W; x += 2) {
      int hh = 3 + (int)(3.0f * (sinf(x * 0.11f) + sinf(x * 0.047f + 1.3f)));
      if (hh > 0) tft.drawFastVLine(x, yh - 5 - hh, hh + 1, jtBlend(bg, jtTh->c1, 40));
    }
  };

  auto drawHud = [&]() {
    tft.fillRect(0, 0, W, HUD, JT_RGB(10, 12, 24));
    char b[18];
    snprintf(b, sizeof(b), "T%02d", timeLeftF / 30);
    gText(b, 2, 3, 1, (timeLeftF < 300) ? RED : (uint16_t)jtTh->c3);
    snprintf(b, sizeof(b), "%lu", (unsigned long)score);
    gText(b, 32, 3, 1, ST77XX_WHITE);
    snprintf(b, sizeof(b), "%dkm", (int)(ps * 220));
    gText(b, W - 3 - (int)strlen(b) * 6, 3, 1, jtTh->c1);
  };

  auto relayout = [&]() {
    W = tft.width(); H = tft.height();
    yh = HUD + (H - HUD) * 38 / 100;
    roadHalf = W * 0.46f;
    hwK = roadHalf / (float)(H - yh);
    jtFillScreen(bg);
    drawSky();
    drawHud();
  };

  auto spawnObj = [&](int kind) {
    for (int i = 0; i < 18; i++) if (!obj[i].on) {
      TrObj &t = obj[i];
      t.on = true; t.kind = kind;
      if (kind == 0) {
        t.z = 60.0f + random(40); t.lat = (random(2) ? 0.5f : -0.5f) + (random(21) - 10) * 0.02f;
        t.v = 0.38f + random(30) * 0.01f; t.col = carCols[random(6)];
      } else {
        t.z = 95.0f + random(8); t.lat = (random(2) ? 1.0f : -1.0f) * (1.4f + random(6) * 0.1f);
        t.v = 0; t.col = 0;
      }
      return;
    }
  };

  auto seg = [&](int xa, int xb, int y, int h, uint16_t c) {
    if (xa < 0) xa = 0;
    if (xb > W) xb = W;
    if (xb > xa) fastFillRect(xa, y, xb - xa, h, c);
  };

  relayout();
  gameFlushButtons();
  unsigned long lastFrame = 0;

  while (true) {
    if (backPressed()) return -1;
    if (rotatePressed()) { changeRotation(); relayout(); for (int i = 0; i < 18; i++) obj[i].on = false; }
    if (millis() - lastFrame < 30) { delay(1); continue; }
    lastFrame = millis();
    frame++;
    JoyDir d = readJoyDirection();

    // ---------- update ----------
    bool nit = gJoyHeld();
    float maxSp = (fabsf(pos) > 1.05f) ? 0.35f : (nit ? 1.35f : 1.0f);
    if (d.y < 0 || nit) ps += 0.012f; else if (d.y > 0) ps -= 0.03f; else ps -= 0.004f;
    if (ps > maxSp) ps -= 0.03f;
    if (ps < 0) ps = 0;
    pos += d.x * 0.05f * (0.35f + ps * 0.65f);
    pos -= curve * ps * 0.035f;
    if (pos > 1.6f) pos = 1.6f;
    if (pos < -1.6f) pos = -1.6f;
    trackPos += ps * 0.5f;
    dist += ps * 0.5f;
    if (--curveT <= 0) { curveT = 100 + random(160); curveTarget = (random(3) == 0) ? 0.0f : (random(201) - 100) * 0.011f; }
    curve += (curveTarget - curve) * 0.03f;
    if (--timeLeftF <= 0) { delay(300); return (long)score; }
    score += (uint32_t)(ps * 3.0f);
    if (crashFx > 0) crashFx--;
    if (cpFlash > 0) cpFlash--;
    if (dist >= nextCp) { nextCp += 100; timeLeftF += timeBonus[mode] * 30; cpFlash = 40; score += 500; }

    int cars = 0, trees = 0;
    for (int i = 0; i < 18; i++) if (obj[i].on) { if (obj[i].kind == 0) cars++; else trees++; }
    if (cars < maxCars[mode] && random(100) < 6) spawnObj(0);
    if (trees < 8 && random(100) < 10) spawnObj(1);

    for (int i = 0; i < 18; i++) if (obj[i].on) {
      TrObj &t = obj[i];
      t.z -= (ps - t.v) * 0.5f;
      if (t.z > 130) { t.on = false; continue; }
      if (t.z < ZNEAR - 1.0f) {
        if (t.kind == 0) score += 50;
        t.on = false; continue;
      }
      if (t.kind == 0 && t.z < ZNEAR + 1.4f && fabsf(t.lat - pos) < 0.32f) {
        t.on = false; ps *= 0.3f; crashFx = 10; timeLeftF -= 60;
      }
    }

    // ---------- draw road (every row fully painted -> no flicker) ----------
    float rmax = (float)(H - yh);
    for (int y = yh; y < H; y += rowH) {
      int h = (y + rowH > H) ? H - y : rowH;
      float r = (y - yh) + h * 0.5f;
      float tt = r / rmax;
      float hw = 1.5f + tt * (roadHalf - 1.5f);
      float s = 1.0f - tt;
      float cx = W * 0.5f + curve * s * s * (W * 0.40f) - pos * hw;
      int band = (int)floorf(700.0f / (r + 1.0f) + trackPos);
      bool b = (band & 1);
      int curbW = (int)(hw * 0.14f) + 1;
      int xl = (int)(cx - hw), xr = (int)(cx + hw);
      seg(0, xl - curbW, y, h, b ? grassA : grassB);
      seg(xl - curbW, xl, y, h, b ? rumA : rumB);
      seg(xl, xr, y, h, b ? roadA : roadB);
      seg(xr, xr + curbW, y, h, b ? rumA : rumB);
      seg(xr + curbW, W, y, h, b ? grassA : grassB);
      if (b) { int lw = (int)(hw * 0.025f) + 1; seg((int)cx - lw, (int)cx + lw, y, h, 0xFFFF); }
    }

    // ---------- objects: far -> near ----------
    int order[18], n = 0;
    for (int i = 0; i < 18; i++) if (obj[i].on) order[n++] = i;
    for (int a = 1; a < n; a++) {       // insertion sort by z descending
      int k = order[a], bI = a - 1;
      while (bI >= 0 && obj[order[bI]].z < obj[k].z) { order[bI + 1] = order[bI]; bI--; }
      order[bI + 1] = k;
    }
    for (int q = 0; q < n; q++) {
      TrObj &t = obj[order[q]];
      float r = rowOf(t.z);
      if (r < 5 || r > rmax + 6) continue;
      float tt = r / rmax; if (tt > 1.0f) tt = 1.0f;
      float hw = 1.5f + tt * (roadHalf - 1.5f);
      float s = 1.0f - tt;
      float cx = W * 0.5f + curve * s * s * (W * 0.40f) - pos * hw;
      int x = (int)(cx + t.lat * hw), y = yh + (int)r;
      int w = (int)(hw * (t.kind == 0 ? 0.30f : 0.22f));
      if (w < 2) w = 2;
      if (t.kind == 0) {
        int hh = w * 6 / 10 + 1;
        tft.fillRect(x - w / 2, y - hh, w, hh, t.col);
        tft.fillRect(x - w / 2 + 1, y - hh + 1, w - 2, (hh > 4) ? hh / 3 : 1, JT_RGB(25, 35, 55));
        tft.fillRect(x - w / 2, y - 1, w / 4 + 1, 1, 0x0000);
        tft.fillRect(x + w / 2 - w / 4, y - 1, w / 4 + 1, 1, 0x0000);
        tft.fillRect(x - w / 2, y - hh + hh / 2, 2, 2, RED);
        tft.fillRect(x + w / 2 - 2, y - hh + hh / 2, 2, 2, RED);
      } else {
        tft.fillRect(x, y - w, 1 + w / 6, w, JT_RGB(90, 60, 30));
        tft.fillTriangle(x - w / 2, y - w, x + w / 2, y - w, x, y - w * 2, jtTh->c4);
      }
    }

    // ---------- player car ----------
    {
      int px0 = W / 2, py0 = H - 20 + ((frame & 2) && ps > 0.5f ? 1 : 0);
      int sh = d.x;
      uint16_t pc = jtTh->c1;
      tft.fillRect(px0 - 13, py0 + 6, 5, 6, 0x0000);
      tft.fillRect(px0 + 9, py0 + 6, 5, 6, 0x0000);
      tft.fillRoundRect(px0 - 12, py0 + 1, 25, 10, 3, pc);
      tft.fillRoundRect(px0 - 8 + sh, py0 - 4, 17, 7, 3, jtBlend(pc, 0x0000, 90));
      tft.fillRect(px0 - 6 + sh, py0 - 3, 13, 4, JT_RGB(25, 35, 55));
      tft.fillRect(px0 - 11, py0 + 6, 5, 3, RED);
      tft.fillRect(px0 + 7, py0 + 6, 5, 3, RED);
      tft.drawFastHLine(px0 - 10, py0 + 2, 21, jtBlend(pc, 0xFFFF, 130));
      if (nit && ps > 0.8f) {
        tft.fillTriangle(px0 - 5, py0 + 11, px0 - 1, py0 + 11, px0 - 3, py0 + 16, JT_ORANGE);
        tft.fillTriangle(px0 + 1, py0 + 11, px0 + 5, py0 + 11, px0 + 3, py0 + 16, JT_ORANGE);
      }
    }
    if (crashFx > 0) { tft.drawRect(0, yh, W, H - yh, RED); tft.drawRect(1, yh + 1, W - 2, H - yh - 2, RED); }
    if (cpFlash > 0) gTextC("CHECKPOINT!", W / 2, yh + 8, 1, ST77XX_YELLOW);
    if ((frame & 3) == 0) drawHud();
  }
}

static void gameTurboRoad() {
  GameOpt o[3] = {
    { "Mode",   { "Easy", "Normal", "Hard" }, 3, 1 },
    { "Detail", { "High", "Fast" }, 2, 0 },
    { "Theme",  { "Neon", "Retro", "Sunset", "Ocean" }, 4, 0 }
  };
  while (true) {
    if (!gameSetup("TURBO ROAD", "Up gas  Dn brake  SW nitro", ST77XX_ORANGE, "tr", o, 3)) return;
    while (true) {
      long sc = gRun(playTurboRoad, o);
      if (sc < 0) break;
      if (!gameOverScreen("Turbo Road", jtTh->c1, (uint32_t)sc, "tr")) break;
    }
  }
}


// ============================================================================
//  12. PONG DUEL  - joystick Up/Down moves your paddle, beat the CPU
// ============================================================================
static long playPong(const GameOpt* o) {
  jtTh = &jtThemes[o[2].val];
  const uint16_t bg = jtTh->bg, WHITE = 0xFFFF, RED = JT_RGB(255, 70, 50);
  static const float aiSpd[3]    = { 1.7f, 2.3f, 3.0f };
  static const float ballSpd[3]  = { 2.3f, 2.9f, 3.5f };
  static const int   padHs[3]    = { 32, 26, 20 };
  static const int   livesTab[3] = { 5, 3, 1 };
  const int mode = o[0].val;
  const int HUD = 12, PW = 4, BALLR = 3;
  const uint16_t pCol = jtTh->c1, aCol = jtTh->c2, ballCol = WHITE;
  const uint16_t midCol = jtBlend(bg, WHITE, 70);

  int W = 0, H = 0, pH = 26;
  float px = 5, ax = 0, py = 0, ay = 0, bx = 0, by = 0, vx = 0, vy = 0, aiErr = 0;
  float trx[5], try_[5];
  int lives = livesTab[o[1].val], serveT = 0, serveDir = 1, rally = 0;
  uint32_t score = 0;
  for (int i = 0; i < 5; i++) { trx[i] = -50; try_[i] = -50; }

  auto serve = [&]() {
    bx = W / 2.0f; by = (HUD + H) / 2.0f + random(-16, 17);
    vx = 0; vy = 0; serveT = 45; rally = 0;
    serveDir = random(2) ? 1 : -1;
    for (int i = 0; i < 5; i++) { trx[i] = bx; try_[i] = by; }
  };
  auto relayout = [&]() {
    W = tft.width(); H = tft.height();
    pH = padHs[mode]; if (pH > (H - HUD) / 3) pH = (H - HUD) / 3;
    px = 5; ax = W - 5 - PW;
    py = ay = (HUD + H) / 2.0f;
    serve();
  };
  auto launch = [&]() {
    float ang = random(-30, 31) * 0.01745f;
    vx = serveDir * ballSpd[mode] * cosf(ang);
    vy = ballSpd[mode] * sinf(ang);
  };
  auto bounce = [&](int dirX, float padY) {
    float rel = (by - padY) / (pH / 2.0f + BALLR);
    if (rel > 1) rel = 1;
    if (rel < -1) rel = -1;
    float extra = rally * 0.12f; if (extra > 2.0f) extra = 2.0f;
    float spd = ballSpd[mode] + extra;
    vx = dirX * spd * cosf(rel * 1.0f);
    vy = spd * sinf(rel * 1.0f);
    rally++;
    if (dirX == 1) aiErr = (float)random(-pH / 3, pH / 3 + 1) * (mode == 0 ? 1.0f : (mode == 1 ? 0.6f : 0.25f));
  };
  auto paddle = [&](float x, float y, uint16_t c) {
    int top = (int)(y - pH / 2.0f);
    tft.fillRoundRect((int)x, top, PW, pH, 2, c);
    tft.drawFastVLine((int)x + 1, top + 2, pH - 4, jtBlend(c, WHITE, 120));
  };
  auto render = [&]() {
    tft.fillScreen(bg);
    for (int y = HUD + 4; y < H; y += 10) tft.fillRect(W / 2 - 1, y, 2, 5, midCol);
    for (int i = 0; i < 5; i++) {
      if (trx[i] < 0) continue;
      uint16_t tc = jtBlend(bg, ballCol, 30 + i * 20);
      tft.fillCircle((int)trx[i], (int)try_[i], BALLR - (i > 2 ? 1 : 0), tc);
    }
    paddle(px, py, pCol);
    paddle(ax, ay, aCol);
    tft.fillCircle((int)bx, (int)by, BALLR, ballCol);
    // HUD
    tft.fillRect(0, 0, W, HUD, jtBlend(bg, jtTh->c1, 35));
    tft.drawFastHLine(0, HUD, W, jtBlend(bg, jtTh->c1, 140));
    char b[20];
    snprintf(b, sizeof(b), "SC %lu", (unsigned long)score);
    gText(b, 3, 2, 1, WHITE);
    int shown = lives > 5 ? 5 : lives;
    for (int i = 0; i < shown; i++) gHeart(W - 4 - (i + 1) * 10, 2, RED);
    if (serveT > 0) gTextC("SW = serve", W / 2, (HUD + H) / 2 + 22, 1, jtTh->c3);
  };

  relayout();
  gameFlushButtons();
  unsigned long lastFrame = 0;

  while (true) {
    if (backPressed()) return -1;
    if (rotatePressed()) { changeRotation(); relayout(); }
    bool fire = joyPressed();
    if (millis() - lastFrame < 33) { delay(1); continue; }
    lastFrame = millis();

    JoyDir d = readJoyDirection();
    py += d.y * 3.6f;
    float lo = HUD + 2 + pH / 2.0f, hi = H - 2 - pH / 2.0f;
    if (py < lo) py = lo;
    if (py > hi) py = hi;

    // CPU paddle
    float target = (vx > 0) ? (by + aiErr) : (HUD + H) / 2.0f;
    float diff = target - ay, mv = aiSpd[mode];
    if (diff > mv) ay += mv; else if (diff < -mv) ay -= mv; else ay = target;
    if (ay < lo) ay = lo;
    if (ay > hi) ay = hi;

    if (serveT > 0) {
      if (fire) serveT = 1;
      serveT--;
      if (serveT == 0) launch();
    } else {
      for (int i = 4; i > 0; i--) { trx[i] = trx[i - 1]; try_[i] = try_[i - 1]; }
      trx[0] = bx; try_[0] = by;
      bx += vx; by += vy;
      if (by < HUD + 2 + BALLR) { by = HUD + 2 + BALLR; vy = fabsf(vy); }
      if (by > H - 1 - BALLR)   { by = H - 1 - BALLR;   vy = -fabsf(vy); }
      if (vx < 0 && bx - BALLR <= px + PW && bx + BALLR >= px && fabsf(by - py) <= pH / 2.0f + BALLR) {
        bx = px + PW + BALLR + 1; bounce(1, py); score++;
      } else if (vx > 0 && bx + BALLR >= ax && bx - BALLR <= ax + PW && fabsf(by - ay) <= pH / 2.0f + BALLR) {
        bx = ax - BALLR - 1; bounce(-1, ay);
      }
      if (bx < -BALLR) {
        lives--;
        render();
        tft.fillRect(0, HUD + 1, 3, H - HUD - 1, RED);
        delay(350);
        if (lives <= 0) { delay(200); return (long)score; }
        serve();
      } else if (bx > W + BALLR) {
        score += 10;
        serve();
      }
    }
    render();
  }
}

static void gamePong() {
  GameOpt o[3] = {
    { "Level", { "Easy", "Normal", "Hard" }, 3, 1 },
    { "Lives", { "5", "3", "1" }, 3, 1 },
    { "Theme", { "Neon", "Retro", "Sunset", "Ocean" }, 4, 0 }
  };
  while (true) {
    if (!gameSetup("PONG DUEL", "Up/Dn paddle  SW serve", ST77XX_CYAN, "pg", o, 3)) return;
    while (true) {
      long sc = gRun(playPong, o);
      if (sc < 0) break;
      if (!gameOverScreen("Pong Duel", jtTh->c1, (uint32_t)sc, "pg")) break;
    }
  }
}

// ============================================================================
//  13. FROG HOP  - dodge traffic, ride the logs, reach the far bank
// ============================================================================
struct FgLane { float off, spd; uint8_t kind, len, period; uint16_t col; };   // kind 0 safe, 1 road, 2 river

static long playFrogHop(const GameOpt* o) {
  jtTh = &jtThemes[o[2].val];
  const uint16_t bg = jtTh->bg, WHITE = 0xFFFF, RED = JT_RGB(255, 70, 50);
  static const float spdBase[3]  = { 0.55f, 0.75f, 0.95f };
  static const int   livesTab[3] = { 5, 3, 2 };
  const int mode = o[0].val;
  const int HUD = 12, CELLSZ = 12;
  const uint16_t grass = jtBlend(bg, JT_RGB(40, 170, 70), 120);
  const uint16_t grassHi = jtBlend(grass, WHITE, 40);
  const uint16_t road = jtBlend(bg, JT_RGB(110, 110, 125), 110);
  const uint16_t dash = jtBlend(road, WHITE, 120);
  const uint16_t river = jtBlend(bg, JT_RGB(40, 110, 255), 130);
  const uint16_t wave = jtBlend(river, WHITE, 70);
  const uint16_t logCol = JT_RGB(150, 100, 50), logHi = JT_RGB(190, 140, 80);
  const uint16_t frogCol = JT_RGB(90, 255, 100), frogDk = JT_RGB(30, 150, 50);
  const uint16_t carCols[4] = { jtTh->c2, jtTh->c3, JT_RGB(235, 235, 235), jtTh->c1 };

  int W = 0, H = 0, NROWS = 12, nr = 4, nrd = 5;
  FgLane ln[16];
  memset(ln, 0, sizeof(ln));
  float fx = 0;
  int frow = 0, bestRow = 0, lives = livesTab[o[1].val], hopCool = 0, timeLeft = 0;
  int level = 1;
  uint32_t score = 0, frame = 0;
  const int FROG_TIME = 1000;

  auto setupLanes = [&]() {
    for (int r = 0; r < NROWS; r++) {
      FgLane &l = ln[r];
      l.kind = (r == 0) ? 0 : (r <= nr ? 2 : (r == nr + 1 ? 0 : (r <= nr + nrd + 1 ? 1 : 0)));
      if (l.kind == 0) { l.spd = 0; l.len = 0; l.period = 1; continue; }
      float base = spdBase[mode] + (level > 9 ? 8 : level - 1) * 0.07f + random(0, 30) * 0.01f;
      float dir = (r & 1) ? 1.0f : -1.0f;
      if (l.kind == 2) {
        l.spd = dir * base * 0.8f;
        l.len = 2 + random(2);
        l.period = l.len + 2 + random(2);
        l.col = logCol;
      } else {
        l.spd = dir * base;
        l.len = 1 + (random(3) == 0 ? 1 : 0);
        l.period = l.len + 2 + random(3);
        l.col = carCols[random(4)];
      }
      l.off = (float)random(l.period * CELLSZ);
    }
  };
  auto respawn = [&]() {
    frow = NROWS - 1; fx = (W / 2) - CELLSZ / 2; timeLeft = FROG_TIME; hopCool = 0;
  };
  auto relayout = [&]() {
    W = tft.width(); H = tft.height();
    NROWS = (H - HUD) / CELLSZ; if (NROWS > 16) NROWS = 16;
    nr = (NROWS - 3) / 2; nrd = NROWS - 3 - nr;
    setupLanes();
    respawn();
    bestRow = frow;
  };
  auto rowY = [&](int r) -> int { return HUD + 1 + r * CELLSZ; };

  // Is there a log under the frog's centre on row r? (returns lane speed through spd)
  auto logUnder = [&](int r, float &spd) -> bool {
    const FgLane &l = ln[r];
    float LANEL = (float)(l.period * CELLSZ), c = fx + CELLSZ / 2.0f;
    for (float x = l.off - LANEL; x < W + LANEL; x += LANEL) {
      if (c >= x - 2 && c <= x + l.len * CELLSZ + 2) { spd = l.spd; return true; }
    }
    return false;
  };
  auto hitCar = [&](int r) -> bool {
    const FgLane &l = ln[r];
    float LANEL = (float)(l.period * CELLSZ);
    for (float x = l.off - LANEL; x < W + LANEL; x += LANEL) {
      if (fx + CELLSZ - 3 > x + 1 && fx + 3 < x + l.len * CELLSZ - 1) return true;
    }
    return false;
  };

  auto drawFrog = [&](int x, int y, bool dead) {
    uint16_t c = dead ? RED : frogCol, dk = dead ? JT_RGB(140, 20, 20) : frogDk;
    tft.fillRoundRect(x + 2, y + 2, 8, 8, 3, c);
    tft.fillRect(x + 1, y + 8, 3, 3, dk); tft.fillRect(x + 8, y + 8, 3, 3, dk);
    tft.fillRect(x + 1, y + 1, 3, 3, dk); tft.fillRect(x + 8, y + 1, 3, 3, dk);
    tft.fillRect(x + 3, y + 2, 2, 2, WHITE); tft.fillRect(x + 7, y + 2, 2, 2, WHITE);
    tft.drawPixel(x + 4, y + 3, 0); tft.drawPixel(x + 8, y + 3, 0);
  };

  auto render = [&](bool dead) {
    tft.fillScreen(grass);
    for (int r = 0; r < NROWS; r++) {
      const FgLane &l = ln[r];
      int y = rowY(r);
      if (l.kind == 0) {
        for (int x = 2 + ((r * 5) & 7); x < W; x += 12) tft.drawPixel(x, y + 3 + (x & 3) * 2, grassHi);
        continue;
      }
      tft.fillRect(0, y, W, CELLSZ, l.kind == 1 ? road : river);
      float LANEL = (float)(l.period * CELLSZ);
      if (l.kind == 1) {
        tft.drawFastHLine(0, y, W, dash);
        for (float x = l.off - LANEL; x < W + LANEL; x += LANEL) {
          int xi = (int)x, w = l.len * CELLSZ;
          tft.fillRoundRect(xi + 1, y + 2, w - 2, CELLSZ - 4, 2, l.col);
          tft.fillRect(xi + 3, y + 4, w - 6, CELLSZ - 8, jtBlend(l.col, 0, 130));
          int hx = l.spd > 0 ? xi + w - 3 : xi + 1;
          tft.fillRect(hx, y + 3, 2, 2, JT_RGB(255, 240, 160));
          tft.fillRect(hx, y + CELLSZ - 5, 2, 2, JT_RGB(255, 240, 160));
        }
      } else {
        for (int x = ((frame >> 2) + r * 7) % 14; x < W; x += 14) tft.drawFastHLine(x, y + 2 + (r & 3), 4, wave);
        for (float x = l.off - LANEL; x < W + LANEL; x += LANEL) {
          int xi = (int)x, w = l.len * CELLSZ;
          tft.fillRoundRect(xi, y + 1, w, CELLSZ - 2, 4, l.col);
          tft.drawFastHLine(xi + 3, y + 3, w - 6, logHi);
          tft.drawFastHLine(xi + 3, y + CELLSZ - 4, w - 6, jtBlend(l.col, 0, 120));
          tft.drawFastVLine(xi + w / 2, y + 4, CELLSZ - 8, jtBlend(l.col, 0, 100));
        }
      }
    }
    int bottom = rowY(NROWS);
    if (bottom < H) tft.fillRect(0, bottom, W, H - bottom, grass);
    drawFrog((int)fx, rowY(frow), dead);
    // HUD
    tft.fillRect(0, 0, W, HUD, jtBlend(bg, jtTh->c1, 35));
    tft.drawFastHLine(0, HUD, W, jtBlend(bg, jtTh->c1, 140));
    char b[24];
    snprintf(b, sizeof(b), "SC %lu", (unsigned long)score);
    gText(b, 3, 2, 1, WHITE);
    snprintf(b, sizeof(b), "L%d", level);
    gText(b, W / 2 - 6, 2, 1, jtTh->c3);
    int shown = lives > 5 ? 5 : lives;
    for (int i = 0; i < shown; i++) gHeart(W - 3 - (i + 1) * 10, 2, RED);
    int bw = (W - 2) * timeLeft / FROG_TIME;
    tft.fillRect(1, HUD - 2, bw, 2, timeLeft < 250 ? RED : jtTh->c4);
  };

  relayout();
  gameFlushButtons();
  unsigned long lastFrame = 0;
  int prevDx = 0, prevDy = 0;

  while (true) {
    if (backPressed()) return -1;
    if (rotatePressed()) { changeRotation(); relayout(); }
    joyPressed();
    if (millis() - lastFrame < 33) { delay(1); continue; }
    lastFrame = millis();
    frame++;

    // lane movement
    for (int r = 0; r < NROWS; r++) {
      FgLane &l = ln[r];
      if (l.kind == 0) continue;
      float LANEL = (float)(l.period * CELLSZ);
      l.off += l.spd;
      while (l.off >= LANEL) l.off -= LANEL;
      while (l.off < 0)  l.off += LANEL;
    }

    // frog carried by the log it sits on
    bool dead = false;
    if (ln[frow].kind == 2) {
      float s;
      if (logUnder(frow, s)) {
        fx += s;
        if (fx + CELLSZ / 2.0f < 0 || fx + CELLSZ / 2.0f > W) dead = true;
      } else dead = true;
    }

    // hopping
    JoyDir d = readJoyDirection();
    if (hopCool > 0) hopCool--;
    bool newPress = (d.x != prevDx || d.y != prevDy);
    prevDx = d.x; prevDy = d.y;
    if ((d.x != 0 || d.y != 0) && !dead && (newPress || hopCool == 0)) {
      if (d.y < 0 && frow > 0) frow--;
      else if (d.y > 0 && frow < NROWS - 1) frow++;
      else if (d.y == 0 && d.x < 0) fx -= CELLSZ;
      else if (d.y == 0 && d.x > 0) fx += CELLSZ;
      hopCool = 6;
    }
    if (fx < -CELLSZ / 2) fx = -CELLSZ / 2;
    if (fx > W - CELLSZ / 2) fx = W - CELLSZ / 2;

    if (!dead) {
      if (frow < bestRow) { bestRow = frow; score += 10; }
      if (ln[frow].kind == 1 && hitCar(frow)) dead = true;
      if (ln[frow].kind == 2) { float s; if (!logUnder(frow, s)) dead = true; }
      if (--timeLeft <= 0) dead = true;
    }

    if (!dead && frow == 0) {
      score += 50 + (uint32_t)(timeLeft / 20);
      level++;
      setupLanes();
      respawn();
      bestRow = frow;
      render(false);
      gTextC("CROSSED!", W / 2, H / 2 - 4, 1, WHITE);
      delay(350);
      continue;
    }

    if (dead) {
      render(true);
      delay(450);
      lives--;
      if (lives <= 0) return (long)score;
      respawn();
      continue;
    }
    render(false);
  }
}

static void gameFrogHop() {
  GameOpt o[3] = {
    { "Traffic", { "Calm", "Normal", "Rush" }, 3, 1 },
    { "Lives", { "5", "3", "2" }, 3, 1 },
    { "Theme", { "Neon", "Retro", "Sunset", "Ocean" }, 4, 1 }
  };
  while (true) {
    if (!gameSetup("FROG HOP", "Hop with the stick", ST77XX_GREEN, "fg", o, 3)) return;
    while (true) {
      long sc = gRun(playFrogHop, o);
      if (sc < 0) break;
      if (!gameOverScreen("Frog Hop", JT_RGB(90, 255, 100), (uint32_t)sc, "fg")) break;
    }
  }
}

// ============================================================================
//  14. MAZE MUNCHER  - eat every dot, grab a power pellet, bite the ghosts
// ============================================================================
static const char* const MZ_MAP[13] = {
  "###############",
  "#o.....#.....o#",
  "#.##.#.#.#.##.#",
  "#.#..#...#..#.#",
  "#...##.#.##...#",
  "###.#.....#.###",
  "#.....#.#.....#",
  "#.###.#.#.###.#",
  "#.#.........#.#",
  "#.#.#.#.#.#.#.#",
  "#...#..P..#...#",
  "#o.#########.o#",
  "###############"
};
struct MzEnt { int8_t cx, cy, dir; float prog; };
struct MzGhost { MzEnt e; bool on; int16_t wait; };

static long playMazeMuncher(const GameOpt* o) {
  jtTh = &jtThemes[o[2].val];
  const uint16_t bg = jtTh->bg, WHITE = 0xFFFF, RED = JT_RGB(255, 70, 50);
  static const float ghostSpd[3] = { 0.80f, 0.95f, 1.10f };
  static const int   nGhostTab[3] = { 2, 3, 4 };
  const int mode = o[0].val;
  const int HUD = 12, CELLSZ = 8, MZW = 15, MZH = 13;
  const int nGhost = nGhostTab[o[1].val];
  const uint16_t wallDk = jtBlend(bg, jtTh->c1, 70), wallLt = jtBlend(bg, jtTh->c1, 230);
  const uint16_t dotCol = jtBlend(bg, jtTh->c3, 230);
  const uint16_t pacCol = JT_RGB(255, 225, 40);
  const uint16_t gCols[4] = { JT_RGB(255, 60, 60), JT_RGB(255, 140, 220), JT_RGB(80, 230, 255), JT_RGB(255, 170, 40) };
  static const int8_t MZDX[4] = { 0, 1, 0, -1 }, MZDY[4] = { -1, 0, 1, 0 };

  uint8_t maze[MZH][MZW];            // 0 empty, 1 wall, 2 dot, 3 power pellet
  int W = 0, H = 0, ox = 0, oy = 0, dotsLeft = 0;
  MzEnt pl; MzGhost gh[4];
  int want = 1, lives = 3, level = 1, fearT = 0, ghostCombo = 0;
  bool moving = false;
  uint32_t score = 0, frame = 0;
  const int8_t gHomeX[4] = { 7, 6, 8, 7 }, gHomeY[4] = { 5, 5, 5, 6 };

  auto canGo = [&](int cx, int cy, int dir) -> bool {
    int nx = cx + MZDX[dir], ny = cy + MZDY[dir];
    if (nx < 0 || ny < 0 || nx >= MZW || ny >= MZH) return false;
    return maze[ny][nx] != 1;
  };
  auto resetActors = [&]() {
    pl.cx = 7; pl.cy = 10; pl.dir = 1; pl.prog = 0; want = 1; moving = false;
    for (int i = 0; i < 4; i++) {
      gh[i].e.cx = gHomeX[i]; gh[i].e.cy = gHomeY[i]; gh[i].e.dir = 1; gh[i].e.prog = 0;
      gh[i].on = (i < nGhost);
      gh[i].wait = i * 50;
    }
    fearT = 0; ghostCombo = 0;
  };
  auto buildMaze = [&]() {
    dotsLeft = 0;
    for (int y = 0; y < MZH; y++) for (int x = 0; x < MZW; x++) {
      char c = MZ_MAP[y][x];
      uint8_t v = 0;
      if (c == '#') v = 1;
      else if (c == '.') { v = 2; dotsLeft++; }
      else if (c == 'o') { v = 3; dotsLeft++; }
      maze[y][x] = v;
    }
    maze[10][7] = 0;
    resetActors();
  };
  auto relayout = [&]() {
    W = tft.width(); H = tft.height();
    ox = (W - MZW * CELLSZ) / 2;
    oy = HUD + (H - HUD - MZH * CELLSZ) / 2;
    buildMaze();
    lives = 3; level = 1;
  };

  auto eatAt = [&](int cx, int cy) {
    uint8_t &m = maze[cy][cx];
    if (m == 2) { m = 0; dotsLeft--; score += 10; }
    else if (m == 3) { m = 0; dotsLeft--; score += 50; fearT = 260 - (level > 6 ? 6 : level) * 20; ghostCombo = 0; }
  };

  auto stepPlayer = [&](float speed) {
    if (moving && (want == ((pl.dir + 2) & 3)) && pl.prog > 0.01f) {
      pl.cx += MZDX[pl.dir]; pl.cy += MZDY[pl.dir];
      pl.prog = CELLSZ - pl.prog; pl.dir = want;
    }
    float rem = speed;
    int guard = 0;
    while (rem > 0.0001f && guard++ < 4) {
      if (pl.prog <= 0.0001f) {
        pl.prog = 0;
        if (canGo(pl.cx, pl.cy, want)) { pl.dir = want; moving = true; }
        else if (!(moving && canGo(pl.cx, pl.cy, pl.dir))) { moving = false; break; }
      }
      float step = CELLSZ - pl.prog; if (step > rem) step = rem;
      pl.prog += step; rem -= step;
      if (pl.prog >= CELLSZ - 0.0001f) {
        pl.cx += MZDX[pl.dir]; pl.cy += MZDY[pl.dir]; pl.prog = 0;
        eatAt(pl.cx, pl.cy);
      }
    }
  };

  auto chooseGhostDir = [&](int gi) {
    MzEnt &e = gh[gi].e;
    int cand[4], nc = 0;
    for (int d = 0; d < 4; d++) if (d != ((e.dir + 2) & 3) && canGo(e.cx, e.cy, d)) cand[nc++] = d;
    if (nc == 0) { for (int d = 0; d < 4; d++) if (canGo(e.cx, e.cy, d)) cand[nc++] = d; }
    if (nc == 0) return;
    int pick = cand[random(nc)];
    int chase = (mode == 0) ? 45 : (mode == 1 ? 65 : 80);
    if (fearT == 0 && random(100) < chase) {
      int tx = pl.cx, ty = pl.cy;
      if (gi == 1) { tx += MZDX[pl.dir] * 3; ty += MZDY[pl.dir] * 3; }
      if (gi == 3) { tx = (frame / 90) & 1 ? 1 : 13; ty = (frame / 90) & 1 ? 1 : 11; }
      int bestD = 1 << 30;
      for (int k = 0; k < nc; k++) {
        int dd = cand[k];
        int nx = e.cx + MZDX[dd] - tx, ny = e.cy + MZDY[dd] - ty;
        int dist = nx * nx + ny * ny;
        if (dist < bestD) { bestD = dist; pick = dd; }
      }
    }
    e.dir = pick;
  };

  auto stepGhost = [&](int gi, float speed) {
    MzEnt &e = gh[gi].e;
    float rem = speed;
    int guard = 0;
    while (rem > 0.0001f && guard++ < 4) {
      if (e.prog <= 0.0001f) { e.prog = 0; chooseGhostDir(gi); if (!canGo(e.cx, e.cy, e.dir)) break; }
      float step = CELLSZ - e.prog; if (step > rem) step = rem;
      e.prog += step; rem -= step;
      if (e.prog >= CELLSZ - 0.0001f) { e.cx += MZDX[e.dir]; e.cy += MZDY[e.dir]; e.prog = 0; }
    }
  };

  auto entX = [&](const MzEnt &e) -> float { return ox + e.cx * CELLSZ + MZDX[e.dir] * e.prog + CELLSZ / 2.0f; };
  auto entY = [&](const MzEnt &e) -> float { return oy + e.cy * CELLSZ + MZDY[e.dir] * e.prog + CELLSZ / 2.0f; };

  auto drawGhost = [&](int gi) {
    const MzEnt &e = gh[gi].e;
    int x = (int)entX(e), y = (int)entY(e);
    bool scared = fearT > 0;
    uint16_t c = scared ? ((fearT < 70 && ((frame >> 2) & 1)) ? WHITE : JT_RGB(40, 70, 255)) : gCols[gi];
    tft.fillCircle(x, y - 1, 3, c);
    tft.fillRect(x - 3, y - 1, 7, 4, c);
    if ((frame >> 3) & 1) { tft.drawPixel(x - 3, y + 3, bg); tft.drawPixel(x, y + 3, bg); tft.drawPixel(x + 3, y + 3, bg); }
    else { tft.drawPixel(x - 2, y + 3, bg); tft.drawPixel(x + 1, y + 3, bg); }
    if (scared) {
      tft.drawPixel(x - 1, y - 1, WHITE); tft.drawPixel(x + 1, y - 1, WHITE);
    } else {
      tft.fillRect(x - 2, y - 2, 2, 2, WHITE); tft.fillRect(x + 1, y - 2, 2, 2, WHITE);
      tft.drawPixel(x - 1 + MZDX[e.dir], y - 1 + MZDY[e.dir], 0x001F);
      tft.drawPixel(x + 2 + MZDX[e.dir], y - 1 + MZDY[e.dir], 0x001F);
    }
  };
  auto drawPac = [&](bool dying) {
    int x = (int)entX(pl), y = (int)entY(pl);
    tft.fillCircle(x, y, 3, pacCol);
    if (dying) return;
    bool open = moving && ((frame >> 1) & 1);
    if (open || !moving) {
      int mx = x + MZDX[pl.dir] * 3, my = y + MZDY[pl.dir] * 3;
      int sx = MZDY[pl.dir] != 0 ? 2 : 0, sy = MZDX[pl.dir] != 0 ? 2 : 0;
      tft.fillTriangle(x, y, mx + sx, my + sy, mx - sx, my - sy, bg);
    }
  };

  auto render = [&](bool dying) {
    tft.fillScreen(bg);
    for (int y = 0; y < MZH; y++) for (int x = 0; x < MZW; x++) {
      int px = ox + x * CELLSZ, py = oy + y * CELLSZ;
      uint8_t m = maze[y][x];
      if (m == 1) {
        tft.fillRect(px, py, CELLSZ, CELLSZ, wallDk);
        if (y > 0 && maze[y - 1][x] != 1)      tft.drawFastHLine(px, py, CELLSZ, wallLt);
        if (y < MZH - 1 && maze[y + 1][x] != 1) tft.drawFastHLine(px, py + CELLSZ - 1, CELLSZ, wallLt);
        if (x > 0 && maze[y][x - 1] != 1)      tft.drawFastVLine(px, py, CELLSZ, wallLt);
        if (x < MZW - 1 && maze[y][x + 1] != 1) tft.drawFastVLine(px + CELLSZ - 1, py, CELLSZ, wallLt);
      } else if (m == 2) {
        tft.fillRect(px + 3, py + 3, 2, 2, dotCol);
      } else if (m == 3) {
        if ((frame >> 3) & 1) tft.fillCircle(px + 4, py + 4, 2, WHITE);
        else tft.fillCircle(px + 4, py + 4, 2, dotCol);
      }
    }
    for (int i = 0; i < nGhost; i++) if (gh[i].on) drawGhost(i);
    drawPac(dying);
    tft.fillRect(0, 0, W, HUD, jtBlend(bg, jtTh->c1, 35));
    tft.drawFastHLine(0, HUD, W, jtBlend(bg, jtTh->c1, 140));
    char b[20];
    snprintf(b, sizeof(b), "SC %lu", (unsigned long)score);
    gText(b, 3, 2, 1, WHITE);
    snprintf(b, sizeof(b), "L%d", level);
    gText(b, W / 2 - 6, 2, 1, jtTh->c3);
    for (int i = 0; i < lives && i < 5; i++) gHeart(W - 3 - (i + 1) * 10, 2, RED);
  };

  relayout();
  gameFlushButtons();
  unsigned long lastFrame = 0;

  while (true) {
    if (backPressed()) return -1;
    if (rotatePressed()) { changeRotation(); relayout(); }
    joyPressed();
    if (millis() - lastFrame < 33) { delay(1); continue; }
    lastFrame = millis();
    frame++;

    JoyDir d = readJoyDirection();
    if (d.y < 0) want = 0; else if (d.y > 0) want = 2;
    else if (d.x > 0) want = 1; else if (d.x < 0) want = 3;

    stepPlayer(1.15f);
    float gs = ghostSpd[mode] + (level > 8 ? 8 : level - 1) * 0.03f;
    if (fearT > 0) { fearT--; gs *= 0.6f; }
    for (int i = 0; i < nGhost; i++) {
      if (!gh[i].on) {
        if (--gh[i].wait <= 0) { gh[i].on = true; gh[i].e.cx = gHomeX[i]; gh[i].e.cy = gHomeY[i]; gh[i].e.prog = 0; }
        continue;
      }
      if (gh[i].wait > 0) { gh[i].wait--; continue; }
      stepGhost(i, gs);
    }

    // collisions
    bool hit = false;
    for (int i = 0; i < nGhost; i++) {
      if (!gh[i].on || gh[i].wait > 0) continue;
      float dx = entX(gh[i].e) - entX(pl), dy = entY(gh[i].e) - entY(pl);
      if (dx * dx + dy * dy < 36.0f) {
        if (fearT > 0) {
          ghostCombo++;
          score += 100 * (1 << (ghostCombo > 3 ? 3 : ghostCombo));
          gh[i].on = false; gh[i].wait = 90;
        } else hit = true;
      }
    }

    if (hit) {
      render(true);
      for (int k = 0; k < 3; k++) {
        tft.drawCircle((int)entX(pl), (int)entY(pl), 3 + k * 3, RED);
        delay(130);
      }
      delay(250);
      lives--;
      if (lives <= 0) return (long)score;
      resetActors();
      continue;
    }

    if (dotsLeft <= 0) {
      render(false);
      gTextC("CLEARED!", W / 2, H / 2 - 4, 1, WHITE);
      delay(700);
      level++;
      score += 100;
      buildMaze();
      continue;
    }
    render(false);
  }
}

static void gameMazeMuncher() {
  GameOpt o[3] = {
    { "Level", { "Easy", "Normal", "Hard" }, 3, 1 },
    { "Ghosts", { "2", "3", "4" }, 3, 1 },
    { "Theme", { "Neon", "Retro", "Sunset", "Ocean" }, 4, 0 }
  };
  while (true) {
    if (!gameSetup("MAZE MUNCHER", "Steer  eat dots  o = bite", ST77XX_YELLOW, "mz", o, 3)) return;
    while (true) {
      long sc = gRun(playMazeMuncher, o);
      if (sc < 0) break;
      if (!gameOverScreen("Maze Muncher", JT_RGB(255, 225, 40), (uint32_t)sc, "mz")) break;
    }
  }
}

// ============================================================================
//  15. MEMORY MATCH  - flip cards, find the pairs, keep your combo going
// ============================================================================
static long playMemoryMatch(const GameOpt* o) {
  jtTh = &jtThemes[o[2].val];
  const uint16_t bg = jtTh->bg, WHITE = 0xFFFF, RED = JT_RGB(255, 70, 50);
  static const int triesTab[3] = { 12, 8, 5 };
  static const int pairsTab[3] = { 6, 8, 10 };
  const int HUD = 12;
  const uint16_t symCols[2] = { jtTh->c2, jtTh->c4 };
  const uint16_t backCol = jtBlend(bg, jtTh->c1, 90), backHi = jtBlend(bg, jtTh->c1, 200);
  const uint16_t faceCol = jtBlend(bg, WHITE, 40);

  int pairs = pairsTab[o[0].val];
  int tries = triesTab[o[1].val];
  int W = 0, H = 0, cols = 4, rows = 3, n = 12;
  uint8_t sym[20], st[20];       // st: 0 hidden, 1 face up, 2 matched
  int cur = 0, first = -1, second = -1, waitT = 0, combo = 0, curCool = 0;
  uint32_t score = 0, frame = 0;

  auto deal = [&]() {
    n = pairs * 2;
    bool land = (W >= H);
    if (n == 12)      { cols = land ? 4 : 3; rows = land ? 3 : 4; }
    else if (n == 16) { cols = 4; rows = 4; }
    else              { cols = land ? 5 : 4; rows = land ? 4 : 5; }
    for (int i = 0; i < n; i++) { sym[i] = (uint8_t)(i / 2); st[i] = 0; }
    for (int i = n - 1; i > 0; i--) { int j = random(i + 1); uint8_t t = sym[i]; sym[i] = sym[j]; sym[j] = t; }
    cur = 0; first = second = -1; waitT = 0;
  };
  auto relayout = [&]() {
    W = tft.width(); H = tft.height();
    deal();
  };

  auto drawSymbol = [&](int s, int cx, int cy, int r, uint16_t c) {
    switch (s % 5) {
      case 0: tft.fillCircle(cx, cy, r, c); break;
      case 1: tft.fillRect(cx - r + 1, cy - r + 1, 2 * r - 1, 2 * r - 1, c); break;
      case 2: tft.fillTriangle(cx, cy - r, cx - r, cy + r - 1, cx + r, cy + r - 1, c); break;
      case 3: tft.fillTriangle(cx - r, cy, cx + r, cy, cx, cy - r, c);
              tft.fillTriangle(cx - r, cy, cx + r, cy, cx, cy + r, c); break;
      default: tft.fillRect(cx - 1, cy - r, 3, 2 * r + 1, c); tft.fillRect(cx - r, cy - 1, 2 * r + 1, 3, c); break;
    }
  };

  auto render = [&]() {
    tft.fillScreen(bg);
    int top = HUD + 3;
    int cw = (W - 4) / cols, ch = (H - top - 3) / rows;
    int x0 = (W - cw * cols) / 2;
    for (int i = 0; i < n; i++) {
      int cx = x0 + (i % cols) * cw, cy = top + (i / cols) * ch;
      int w = cw - 3, h = ch - 3;
      if (st[i] == 0) {
        tft.fillRoundRect(cx, cy, w, h, 4, backCol);
        tft.drawRoundRect(cx, cy, w, h, 4, backHi);
        gTextC("?", cx + w / 2, cy + h / 2 - 3, 1, backHi);
      } else if (st[i] == 1) {
        tft.fillRoundRect(cx, cy, w, h, 4, faceCol);
        tft.drawRoundRect(cx, cy, w, h, 4, WHITE);
        int r = (w < h ? w : h) / 2 - 5; if (r < 3) r = 3;
        drawSymbol(sym[i], cx + w / 2, cy + h / 2, r, symCols[sym[i] / 5]);
      } else {
        tft.drawRoundRect(cx, cy, w, h, 4, jtBlend(bg, jtTh->c4, 120));
        int r = (w < h ? w : h) / 2 - 7; if (r < 2) r = 2;
        drawSymbol(sym[i], cx + w / 2, cy + h / 2, r, jtBlend(bg, symCols[sym[i] / 5], 110));
      }
      if (i == cur) {
        uint16_t cc = ((frame >> 2) & 1) ? jtTh->c3 : WHITE;
        tft.drawRoundRect(cx - 1, cy - 1, w + 2, h + 2, 5, cc);
        tft.drawRoundRect(cx - 2, cy - 2, w + 4, h + 4, 6, cc);
      }
    }
    tft.fillRect(0, 0, W, HUD, jtBlend(bg, jtTh->c1, 35));
    tft.drawFastHLine(0, HUD, W, jtBlend(bg, jtTh->c1, 140));
    char b[20];
    snprintf(b, sizeof(b), "SC %lu", (unsigned long)score);
    gText(b, 3, 2, 1, WHITE);
    snprintf(b, sizeof(b), "MISS %d", tries);
    gText(b, W - 3 - (int)strlen(b) * 6, 2, 1, tries <= 2 ? RED : jtTh->c3);
    if (combo > 1) { snprintf(b, sizeof(b), "x%d", combo); gText(b, W / 2 - 6, 2, 1, jtTh->c4); }
  };

  relayout();
  gameFlushButtons();
  unsigned long lastFrame = 0;
  int prevDx = 0, prevDy = 0;

  while (true) {
    if (backPressed()) return -1;
    if (rotatePressed()) { changeRotation(); relayout(); }
    bool fire = joyPressed();
    if (millis() - lastFrame < 33) { delay(1); continue; }
    lastFrame = millis();
    frame++;

    JoyDir d = readJoyDirection();
    if (curCool > 0) curCool--;
    bool newPress = (d.x != prevDx || d.y != prevDy);
    prevDx = d.x; prevDy = d.y;
    if ((d.x != 0 || d.y != 0) && (newPress || curCool == 0)) {
      int cx = cur % cols, cy = cur / cols;
      cx = (cx + d.x + cols) % cols;
      cy = (cy + d.y + rows) % rows;
      cur = cy * cols + cx;
      curCool = 5;
    }

    if (waitT > 0) {
      if (--waitT == 0) {
        if (sym[first] == sym[second]) {
          st[first] = st[second] = 2;
          combo++;
          score += 50 + 20 * (combo - 1);
        } else {
          st[first] = st[second] = 0;
          combo = 0;
          tries--;
        }
        first = second = -1;
        if (tries <= 0) {
          render();
          tft.fillRect(0, H / 2 - 10, W, 20, RED);
          gTextC("OUT OF TRIES", W / 2, H / 2 - 3, 1, WHITE);
          delay(700);
          return (long)score;
        }
        bool all = true;
        for (int i = 0; i < n; i++) if (st[i] != 2) all = false;
        if (all) {
          score += 100;
          tries += 3;
          render();
          tft.fillRect(0, H / 2 - 10, W, 20, jtTh->c4);
          gTextC("BOARD CLEARED!", W / 2, H / 2 - 3, 1, 0);
          delay(800);
          pairs = (pairs < 8) ? 8 : 10;
          deal();
        }
      }
    } else if (fire && st[cur] == 0) {
      st[cur] = 1;
      if (first < 0) first = cur;
      else { second = cur; waitT = 22; }
    }
    render();
  }
}

static void gameMemoryMatch() {
  GameOpt o[3] = {
    { "Cards", { "12", "16", "20" }, 3, 0 },
    { "Tries", { "12", "8", "5" }, 3, 1 },
    { "Theme", { "Neon", "Retro", "Sunset", "Ocean" }, 4, 0 }
  };
  while (true) {
    if (!gameSetup("MEMORY MATCH", "Move  SW flip card", ST77XX_MAGENTA, "mm", o, 3)) return;
    while (true) {
      long sc = gRun(playMemoryMatch, o);
      if (sc < 0) break;
      if (!gameOverScreen("Memory Match", jtTh->c2, (uint32_t)sc, "mm")) break;
    }
  }
}

// ============================================================================
//  16. CLOUD JUMPV  - bounce up from cloud to cloud, never fall
// ============================================================================
struct CjPlat { float x, y, vx; uint8_t type; bool on; };     // 0 normal, 1 moving, 2 crumbly, 3 spring

static long playCloudJump(const GameOpt* o) {
  jtTh = &jtThemes[o[2].val];
  const uint16_t bg = jtTh->bg, WHITE = 0xFFFF, SPRCOL = JT_RGB(255, 70, 50);
  static const int gapMin[3] = { 22, 28, 34 }, gapMax[3] = { 42, 52, 62 };
  static const uint16_t heroCols[4] = { JT_RGB(255, 210, 40), JT_RGB(255, 90, 140), JT_RGB(90, 230, 120), JT_RGB(90, 170, 255) };
  const int mode = o[0].val;
  const uint16_t hero = heroCols[o[1].val];
  const int CLOUD_W = 26, CLOUD_H = 6, HERO_HW = 10, FEETH = 5, HUD = 12;
  const float GRAV = 0.26f, JUMPV = 6.3f, SPRINGV = 9.8f;
  uint16_t sky[5];
  for (int i = 0; i < 5; i++) sky[i] = jtBlend(jtBlend(bg, jtTh->c1, 40), jtBlend(bg, jtTh->c2, 80), i * 255 / 4);

  int W = 0, H = 0;
  CjPlat pl[14];
  float x = 0, y = 0, vx = 0, vy = 0, topY = 0, height = 0;
  float starX[10], starY[10];
  bool lastSolid = true;
  uint32_t frame = 0;
  const uint32_t best = gameBest("cj");
  int face = 1, squash = 0;

  auto spawnAt = [&](float py) {
    for (int i = 0; i < 14; i++) if (!pl[i].on) {
      CjPlat &p = pl[i];
      p.on = true; p.y = py; p.vx = 0;
      p.x = (float)random(2, W - CLOUD_W - 1);
      int r = random(100);
      int mv = 8 + mode * 6, cr = 12 + mode * 7;
      if (height < 120) r = 99;        // gentle start: plain clouds only
      if (r < mv)                p.type = 1;
      else if (r < mv + cr)      p.type = lastSolid ? 2 : 0;
      else if (r < mv + cr + 7)  p.type = 3;
      else                       p.type = 0;
      if (p.type == 1) p.vx = (random(2) ? 1 : -1) * (0.6f + random(0, 8) * 0.1f);
      lastSolid = (p.type != 2);
      return;
    }
  };
  auto nextGap = [&]() -> float {
    int hi = gapMax[mode] + (int)(height / 500.0f); if (hi > 64) hi = 64;
    return (float)(gapMin[mode] + random(hi - gapMin[mode] + 1));
  };
  auto relayout = [&]() {
    W = tft.width(); H = tft.height();
    memset(pl, 0, sizeof(pl));
    height = 0; lastSolid = true; vx = 0; squash = 0; face = 1;
    pl[0].on = true; pl[0].type = 0; pl[0].x = W / 2.0f - CLOUD_W / 2.0f; pl[0].y = (float)(H - 14); pl[0].vx = 0;
    topY = pl[0].y;
    x = W / 2.0f; y = pl[0].y - FEETH; vy = -JUMPV;
    while (topY > -20) { topY -= nextGap(); spawnAt(topY); }
    for (int i = 0; i < 10; i++) { starX[i] = (float)random(W); starY[i] = (float)random(H); }
  };

  auto drawCloud = [&](const CjPlat &p) {
    int px = (int)p.x, py = (int)p.y;
    uint16_t c = WHITE, sh = JT_RGB(170, 190, 220);
    if (p.type == 1) { c = jtBlend(WHITE, jtTh->c1, 140); sh = jtBlend(c, 0, 90); }
    if (p.type == 2) { c = JT_RGB(200, 160, 110); sh = JT_RGB(140, 100, 60); }
    tft.fillRoundRect(px, py, CLOUD_W, CLOUD_H, 3, c);
    tft.fillRect(px + 3, py + CLOUD_H - 2, CLOUD_W - 6, 2, sh);
    if (p.type == 0 || p.type == 3) { tft.fillCircle(px + 6, py, 3, c); tft.fillCircle(px + CLOUD_W - 7, py - 1, 3, c); }
    if (p.type == 2) { tft.drawLine(px + 9, py, px + 12, py + CLOUD_H - 1, sh); tft.drawLine(px + 17, py, px + 14, py + CLOUD_H - 1, sh); }
    if (p.type == 3) {
      int sx = px + CLOUD_W / 2;
      tft.drawLine(sx - 3, py - 1, sx + 3, py - 3, jtTh->c3);
      tft.drawLine(sx - 3, py - 3, sx + 3, py - 5, jtTh->c3);
      tft.fillRect(sx - 4, py - 8, 9, 2, SPRCOL);
    }
  };
  auto drawHero = [&](int hx, int hy) {
    int bw = squash > 0 ? 12 : 10, bh = squash > 0 ? 8 : 10;
    int top = hy + FEETH - bh;
    tft.fillRoundRect(hx - bw / 2, top, bw, bh, 4, hero);
    tft.fillRect(hx - bw / 2 + 2, top + bh - 3, bw - 4, 2, jtBlend(hero, WHITE, 110));
    int ex = hx + face * 2;
    tft.fillRect(ex - 3, top + 2, 3, 3, WHITE); tft.fillRect(ex + 1, top + 2, 3, 3, WHITE);
    tft.drawPixel(ex - 2 + (face > 0 ? 1 : 0), top + 3, 0); tft.drawPixel(ex + 2 + (face > 0 ? 1 : 0), top + 3, 0);
    if (vy < 0) { tft.drawFastVLine(hx - 3, hy + FEETH, 2, hero); tft.drawFastVLine(hx + 3, hy + FEETH, 2, hero); }
    else        { tft.drawFastVLine(hx - 3, hy + FEETH, 3, hero); tft.drawFastVLine(hx + 3, hy + FEETH, 3, hero); }
  };
  auto render = [&]() {
    for (int i = 0; i < 5; i++) {
      int y0 = H * i / 5, y1 = H * (i + 1) / 5;
      tft.fillRect(0, y0, W, y1 - y0, sky[i]);
    }
    for (int i = 0; i < 10; i++) tft.drawPixel((int)starX[i], (int)starY[i], jtBlend(bg, WHITE, 150));
    for (int i = 0; i < 14; i++) if (pl[i].on) drawCloud(pl[i]);
    drawHero((int)x, (int)y);
    if (x < HERO_HW) drawHero((int)x + W, (int)y);
    if (x > W - HERO_HW) drawHero((int)x - W, (int)y);
    tft.fillRect(0, 0, W, HUD, jtBlend(bg, jtTh->c1, 35));
    tft.drawFastHLine(0, HUD, W, jtBlend(bg, jtTh->c1, 140));
    uint32_t sc = (uint32_t)(height / 10.0f);
    char b[20];
    snprintf(b, sizeof(b), "ALT %lu", (unsigned long)sc);
    gText(b, 3, 2, 1, WHITE);
    snprintf(b, sizeof(b), "BEST %lu", (unsigned long)(best > sc ? best : sc));
    gText(b, W - 3 - (int)strlen(b) * 6, 2, 1, JT_GOLD);
  };

  relayout();
  gameFlushButtons();
  unsigned long lastFrame = 0;

  while (true) {
    if (backPressed()) return -1;
    if (rotatePressed()) { changeRotation(); relayout(); }
    joyPressed();
    if (millis() - lastFrame < 33) { delay(1); continue; }
    lastFrame = millis();
    frame++;

    JoyDir d = readJoyDirection();
    vx += (d.x * 2.9f - vx) * 0.28f;
    if (d.x != 0) face = d.x;
    x += vx;
    if (x < -HERO_HW / 2) x = (float)(W + HERO_HW / 2);
    if (x > W + HERO_HW / 2) x = (float)(-HERO_HW / 2);
    if (squash > 0) squash--;

    float prevFeet = y + FEETH;
    vy += GRAV;
    y += vy;

    for (int i = 0; i < 14; i++) if (pl[i].on && pl[i].type == 1) {
      pl[i].x += pl[i].vx;
      if (pl[i].x < 0) { pl[i].x = 0; pl[i].vx = -pl[i].vx; }
      if (pl[i].x > W - CLOUD_W) { pl[i].x = (float)(W - CLOUD_W); pl[i].vx = -pl[i].vx; }
    }

    if (vy > 0) {
      float feet = y + FEETH;
      for (int i = 0; i < 14; i++) {
        CjPlat &p = pl[i];
        if (!p.on) continue;
        if (prevFeet <= p.y + 3 && feet >= p.y && x + HERO_HW / 2 - 1 > p.x && x - HERO_HW / 2 + 1 < p.x + CLOUD_W) {
          if (p.type == 2) { p.on = false; continue; }
          vy = (p.type == 3) ? -SPRINGV : -JUMPV;
          y = p.y - FEETH;
          squash = 6;
          break;
        }
      }
    }

    float lim = H * 0.38f;
    if (y < lim) {
      float sh = lim - y;
      y = lim;
      height += sh; topY += sh;
      for (int i = 0; i < 14; i++) if (pl[i].on) pl[i].y += sh;
      for (int i = 0; i < 10; i++) { starY[i] += sh * 0.3f; if (starY[i] >= H) { starY[i] -= H; starX[i] = (float)random(W); } }
    }
    for (int i = 0; i < 14; i++) if (pl[i].on && pl[i].y > H + 10) pl[i].on = false;
    while (topY > -20) { topY -= nextGap(); spawnAt(topY); }

    if (y > H + 10) {
      render();
      tft.fillRect(0, H / 2 - 10, W, 20, JT_RGB(255, 70, 50));
      gTextC("YOU FELL!", W / 2, H / 2 - 3, 1, WHITE);
      delay(600);
      return (long)(height / 10.0f);
    }
    render();
  }
}

static void gameCloudJump() {
  GameOpt o[3] = {
    { "Level", { "Easy", "Normal", "Hard" }, 3, 1 },
    { "Hero", { "Gold", "Pink", "Green", "Blue" }, 4, 0 },
    { "Theme", { "Neon", "Retro", "Sunset", "Ocean" }, 4, 3 }
  };
  while (true) {
    if (!gameSetup("CLOUD JUMP", "Tilt Left/Right  climb", ST77XX_CYAN, "cj", o, 3)) return;
    while (true) {
      long sc = gRun(playCloudJump, o);
      if (sc < 0) break;
      if (!gameOverScreen("Cloud Jump", jtTh->c1, (uint32_t)sc, "cj")) break;
    }
  }
}

// ---- end of flicker-free engine hooks ----
#undef tft
#undef fastFillRect
#undef jtFillScreen
#undef changeRotation
#undef backPressed
#undef delay

#endif
