// copyright (c) 2025 Md. Abu Naser Nayeem[Tanjib]_Mr.EchoFi

#include <Wire.h>
#include <U8g2lib.h>

// ── DISPLAY ────────────────────────────────────────────────────────────────
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0,        /* rotation */
  /* reset=*/ U8X8_PIN_NONE
);

// ── PINS ────────────────────────────────────────────────────────────────────
const uint8_t JOY_X_PIN   = 33;  // ADC1_CH0
const uint8_t JOY_Y_PIN   = 32;  // ADC1_CH3
//const uint8_t JOY_SW_PIN  = 26;  // now the Shoot button

const uint8_t BTN_SHOOT   = 26;
const uint8_t BTN_RELOAD  = 27;
const uint8_t BTN_SELECT  = 25;

// ── GAME SETTINGS ──────────────────────────────────────────────────────────
// increased times, now stored as unsigned long
const unsigned long LEVEL_TIME_MS[3]  = { 90000UL, 75000UL, 60000UL };
const uint8_t       TARGETS_REQ[3]    = {  5,     7,    10 };
const uint8_t       AMMO_MAX          = 5;
const uint8_t       LIVES_MAX         = 3;
const int           AIM_MIN_X         = 10;
const int           AIM_MAX_X         = 118;
const int           AIM_MIN_Y         = 10;
const int           AIM_MAX_Y         = 54;
const int           CROSSHAIR_RAD     = 8;
const int           STAR_COUNT        = 50;

// ── GLOBAL STATE ────────────────────────────────────────────────────────────
int      aimX, aimY;
int      starX[STAR_COUNT], starY[STAR_COUNT];
int      scanY = 0;

uint8_t  currLevel;
uint16_t targetsHit, targetsReq, ammo, lives;
int      targetX, targetY;
unsigned long levelStart;

// ── PROTOTYPES ──────────────────────────────────────────────────────────────
void initStars();
uint8_t showMenu();
bool playLevel(uint8_t lvl);
void spawnTarget();
void drawBackground();
void drawHUD(uint8_t lvl);
void explosionAt(int x, int y);
void showSplash();
void showLevelComplete(uint8_t lvl);
void showGameOver();
void showVictory();
void showExit();
void waitForSelect();

// ── SETUP ───────────────────────────────────────────────────────────────────
void setup() {
  pinMode(BTN_SHOOT,  INPUT_PULLUP);
  pinMode(BTN_RELOAD,  INPUT_PULLUP);
  pinMode(BTN_SELECT,  INPUT_PULLUP);

  analogReadResolution(12);

  Wire.begin(21,22);
  u8g2.begin();

  randomSeed(micros());
  aimX = (AIM_MIN_X + AIM_MAX_X) / 2;
  aimY = (AIM_MIN_Y + AIM_MAX_Y) / 2;

  initStars();
  showSplash();
}

// ── MAIN LOOP ───────────────────────────────────────────────────────────────
void loop() {
  currLevel = showMenu();
  if (currLevel == 0) {
    showExit();
    while (true);
  }

  bool done = false;
  while (!done) {
    bool win = playLevel(currLevel);
    if (win) {
      if (currLevel < 3) {
        showLevelComplete(currLevel);
        currLevel++;
      } else {
        showVictory();
        done = true;
      }
    } else {
      showGameOver();
      done = true;
    }
  }

  waitForSelect();
}

// ── MENU WITH EXIT OPTION ───────────────────────────────────────────────────
uint8_t showMenu() {
  uint8_t sel = 1;
  u8g2.setFont(u8g2_font_4x6_tr);

  while (true) {
    u8g2.clearBuffer();
    u8g2.drawStr(30,10, "Echo_Snipe");
    u8g2.drawStr(20,20, "Select Level:");
    const char* opts[4] = { "Exit", "1", "2", "3" };
    for (uint8_t i = 0; i < 4; i++) {
      int y = 30 + i*10;
      if (i == sel)  u8g2.drawStr(10,y,">");
      u8g2.drawStr(20,y, opts[i] );
    }
    u8g2.sendBuffer();

    if (digitalRead(BTN_SHOOT)  == LOW) { sel = (sel + 1) % 4; delay(200); }
    if (digitalRead(BTN_RELOAD)  == LOW) { sel = (sel + 3) % 4; delay(200); }
    if (digitalRead(BTN_SELECT)  == LOW) { delay(200); return sel; }
  }
}

// ── PLAY ONE LEVEL ─────────────────────────────────────────────────────────
bool playLevel(uint8_t lvl) {
  targetsReq = TARGETS_REQ[lvl - 1];
  levelStart = millis();
  targetsHit = 0;
  ammo       = AMMO_MAX;
  lives      = LIVES_MAX;
  spawnTarget();

  while (true) {
    unsigned long now     = millis();
    unsigned long elapsed = now - levelStart;
    if (elapsed >= LEVEL_TIME_MS[lvl-1]) break;

    int rawX = analogRead(JOY_X_PIN);
    int rawY = analogRead(JOY_Y_PIN);
    aimX = map(rawX, 0, 4095, AIM_MIN_X, AIM_MAX_X);
    aimY = map(rawY, 4095, 0, AIM_MIN_Y, AIM_MAX_Y);

    if (digitalRead(BTN_RELOAD) == LOW) {
      ammo = AMMO_MAX;
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_4x6_tr);
      u8g2.drawStr(30,28,"Reloading...");
      u8g2.sendBuffer();
      delay(800);
      levelStart += 800;
    }

    if (digitalRead(BTN_SHOOT) == LOW) {
      if (ammo > 0) {
        ammo--;
        bool hit = abs(targetX - aimX) <= CROSSHAIR_RAD
                && abs(targetY - aimY) <= CROSSHAIR_RAD;
        if (hit) {
          explosionAt(targetX, targetY);
          targetsHit++;
          spawnTarget();
        } else {
          u8g2.clearBuffer();
          u8g2.setFont(u8g2_font_4x6_tr);
          u8g2.drawStr(50,32,"MISS!");
          u8g2.sendBuffer();
          lives = max(0, lives - 1);
          delay(300);
          if (lives == 0) return false;
        }
      } else {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_4x6_tr);
        u8g2.drawStr(40,32,"No Ammo!");
        u8g2.sendBuffer();
        delay(300);
      }
    }

    drawBackground();
    drawHUD(lvl);

    // pumpkin body
    u8g2.drawDisc(targetX,targetY,6,U8G2_DRAW_ALL);
    // eyes
    u8g2.drawTriangle(targetX-3,targetY-2, targetX-1,targetY-5, targetX-1,targetY-2);
    u8g2.drawTriangle(targetX+3,targetY-2, targetX+1,targetY-5, targetX+1,targetY-2);
    // zig‑zag mouth
    u8g2.drawLine(targetX-4,targetY+3, targetX-2,targetY+5);
    u8g2.drawLine(targetX-2,targetY+5, targetX,targetY+3);
    u8g2.drawLine(targetX,targetY+3, targetX+2,targetY+5);
    u8g2.drawLine(targetX+2,targetY+5, targetX+4,targetY+3);

    // crosshair
    u8g2.drawCircle(aimX,aimY,CROSSHAIR_RAD,U8G2_DRAW_ALL);
    u8g2.drawCircle(aimX,aimY,CROSSHAIR_RAD/2,U8G2_DRAW_ALL);
    u8g2.drawLine(aimX-CROSSHAIR_RAD,aimY,aimX+CROSSHAIR_RAD,aimY);
    u8g2.drawLine(aimX,aimY-CROSSHAIR_RAD,aimX,aimY+CROSSHAIR_RAD);

    u8g2.sendBuffer();
    delay(30);

    if (targetsHit >= targetsReq) return true;
  }

  return (targetsHit >= targetsReq);
}

// ── SPAWN NEW TARGET ────────────────────────────────────────────────────────
void spawnTarget() {
  targetX = random(AIM_MIN_X, AIM_MAX_X);
  targetY = random(AIM_MIN_Y, AIM_MAX_Y);
}

// ── STARFIELD + SCANLINE ────────────────────────────────────────────────────
void initStars() {
  for (int i = 0; i < STAR_COUNT; i++) {
    starX[i] = random(0,128);
    starY[i] = random(0,64);
  }
}
void drawBackground() {
  u8g2.clearBuffer();
  for (int i = 0; i < STAR_COUNT; i++) {
    u8g2.drawPixel(starX[i], starY[i]);
    starY[i] = (starY[i] + 1) & 63;
    if (starY[i] == 0) starX[i] = random(0,128);
  }
  u8g2.drawLine(0, scanY, 127, scanY);
  scanY = (scanY + 1) & 63;
}

// ── HUD: TIME BAR, LEVEL/HITS, AMMO & LIVES ─────────────────────────────────
void drawHUD(uint8_t lvl) {
  unsigned long rem = LEVEL_TIME_MS[lvl-1] - (millis() - levelStart);
  int tb = map(rem, 0, LEVEL_TIME_MS[lvl-1], 0, 128);
  u8g2.drawBox(0,0, tb,3);
  u8g2.drawFrame(0,0,128,3);

  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(2,12, String("L"+String(lvl)).c_str());
  u8g2.drawStr(24,12, String(String(targetsHit)+"/"+targetsReq).c_str());

  for (uint8_t i = 0; i < ammo; i++)
    u8g2.drawFrame(118 - i*6, 4, 5, 7);

  for (uint8_t i = 0; i < lives; i++) {
    int bx = 2 + i*12, by = 58;
    u8g2.drawFrame(bx,by,8,4);
    u8g2.drawBox(bx+8,by+1,2,2);
  }
}

// ── EXPLOSION ANIMATION ─────────────────────────────────────────────────────
void explosionAt(int x, int y) {
  for (int r = 2; r <= 10; r += 4) {
    drawBackground();
    drawHUD(currLevel);
    u8g2.drawCircle(x,y,r,U8G2_DRAW_ALL);
    u8g2.drawCircle(x,y,r+2,U8G2_DRAW_ALL);
    u8g2.sendBuffer();
    delay(100);
  }
}

// ── TRANSITIONS & FEEDBACK ──────────────────────────────────────────────────
void showSplash() {
  u8g2.setFont(u8g2_font_4x6_tr);
  const char* t = "Echo_Snipe by T@n?iB";
  int w = u8g2.getUTF8Width(t);
  int x0 = (128-w)/2, y0 = 32;
  for (int i=0; i<3; i++) {
    u8g2.clearBuffer();
    u8g2.drawStr(x0,y0, t);
    u8g2.sendBuffer();
    delay(500);
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    delay(300);
  }
}

void showLevelComplete(uint8_t lvl) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  String msg = "Level "+String(lvl)+" Clear!";
  int w = u8g2.getUTF8Width(msg.c_str());
  u8g2.drawStr((128-w)/2,32, msg.c_str());
  u8g2.sendBuffer();
  delay(1200);
}

void showGameOver() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  const char* msg = "!!! GAME OVER !!!";
  int w = u8g2.getUTF8Width(msg);
  u8g2.drawStr((128-w)/2,32,msg);
  u8g2.sendBuffer();
  delay(1500);
}

void showVictory() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  const char* msg = "*** VICTORY ***";
  int w = u8g2.getUTF8Width(msg);
  u8g2.drawStr((128-w)/2,32,msg);
  u8g2.sendBuffer();
  delay(1500);
}

void showExit() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(40,32,"|Exit|");
  u8g2.sendBuffer();
  delay(600);
}

void waitForSelect() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(10,32,"Press Select to Restart");
  u8g2.sendBuffer();
  while (digitalRead(BTN_SELECT) == HIGH) delay(50);
  delay(200);
}
