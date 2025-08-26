#include <FastLED.h>
#include <LedWeb.h>

/* ================== LED-HARDWARE ================== */
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

const uint16_t TRUNK_LEN  = 173;
const uint16_t BRANCH_LEN = 22;

#define PIN_TRUNK 16
#define PIN_L1    25   // links unten
#define PIN_L2    12   // links mitte
#define PIN_L3    32   // links oben
#define PIN_R1    2    // rechts unten
#define PIN_R2    4    // rechts mitte
#define PIN_R3    0    // rechts oben

CRGB leds_trunk[TRUNK_LEN];
CRGB leds_l1[BRANCH_LEN], leds_l2[BRANCH_LEN], leds_l3[BRANCH_LEN];
CRGB leds_r1[BRANCH_LEN], leds_r2[BRANCH_LEN], leds_r3[BRANCH_LEN];

/* ================== ANKER AM STAMM ================== */
const uint16_t Y_LEFT [3]  = { 21,  43,  75};   // links  unten→oben
const uint16_t Y_RIGHT[3]  = { 97, 129, 151};   // rechts unten→oben

/* ================== BUTTONS ================== */
// IO33 mit internem PullUp; IO34/35 mit externem 10k PullUp zu 3.3V
#define BTN_POWER 35   // kurz: Warmweiß 50% | lang: Aus
#define BTN_GREEN 33   // kurz: Mojito statisch ↔ softfade | lang halten: heller
#define BTN_PINK  34   // kurz: Pink | lang halten: dunkler

/* ================== HELLIGKEIT ================== */
const uint8_t BRIGHTNESS_MIN = 26;    // ~10%
const uint8_t BRIGHTNESS_MAX = 230;   // ~90%
const uint8_t BRIGHTNESS_DEF = 128;   // 50%
uint8_t brightness = BRIGHTNESS_DEF;

/* zentral: nur zeigen, wenn etwas geändert wurde */
bool frameDirty = false;

/* ================== FARBCODES ================== */
const CRGB WARM = CRGB(255, 140, 30);          // sehr warmes Warmweiß
const CRGB PINK = CRGB(255, 0, 80);

// Mojito-Palette
const CRGB MOJITO_LIME  = CRGB(120, 255, 80);
const CRGB MOJITO_MINT  = CRGB( 70, 220,120);
const CRGB MOJITO_LEAF  = CRGB( 30, 160, 80);
const CRGB MOJITO_ICE   = CRGB(230, 255,245);
const CRGB MOJITO_SODA  = CRGB(200, 255,230);
const CRGB MOJITO_AMBER = CRGB(255, 180, 80);

// ---------- Aperol Palette ----------
const CRGB AP_ORANGE   = CRGB(255,120, 10);  // Aperol-Orange (Basis)
const CRGB AP_BLOOD    = CRGB(255, 70,  0);  // Bitter-Orange/Blutorange
const CRGB AP_PEEL     = CRGB(255,150, 40);  // Orangenschale
const CRGB AP_PROSECCO = CRGB(255,240,180);  // Champagner-Gold
const CRGB AP_FOAM     = CRGB(255,250,230);  // Schaum / sehr hell
const CRGB AP_WOOD     = CRGB(255,170, 60);  // Holz/Bar (etwas anders als Mojito-AMBER)

/* ================== ZUORDNUNG & ORIENTIERUNG ================== */
CRGB* TRUNK = leds_trunk;
CRGB* L1 = leds_l1; CRGB* L2 = leds_l2; CRGB* L3 = leds_l3;
CRGB* R1 = leds_r1; CRGB* R2 = leds_r2; CRGB* R3 = leds_r3;

bool L_REVERSED[3] = {false,false,false};   // Index 0 außen (false)
bool R_REVERSED[3] = {false,false,false};

/* ================== RENDER-HELPER (optimiert) ================== */

// globales, schnelles Clear (räumt alle registrierten LED-Arrays)
inline void clearAllLayout() {
  FastLED.clear(false);            // kein show()
  frameDirty = true;
}


inline void setBrightness(uint8_t b) {
  if (b != brightness) {
    brightness = b;
    FastLED.setBrightness(brightness);
    frameDirty = true;             // Helligkeit geändert → neu anzeigen
  }
}

// Stamm direkt füllen (ohne setXY/Checks)
inline void colorTrunkRange(int y0, int y1, const CRGB& c) {
  if (y0 > y1) { int t=y0; y0=y1; y1=t; }
  y0 = max(0, y0); y1 = min<int>(TRUNK_LEN-1, y1);
  for (int y=y0; y<=y1; ++y) TRUNK[y] = c;
  frameDirty = true;
}

// Ast „roh“ füllen (ohne setXY/Seiten-Dispatch)
inline void fillBranchRaw(CRGB* arr, bool reversed,
                          uint8_t outerCount, const CRGB& outerColor,
                          uint8_t innerCount, const CRGB& innerColor) {
  uint8_t total = min<uint8_t>(BRANCH_LEN, outerCount + innerCount);
  for (uint8_t i=0; i<total; ++i) {
    uint8_t idx = reversed ? (BRANCH_LEN-1 - i) : i;
    arr[idx] = (i < outerCount) ? outerColor : innerColor;
  }
  frameDirty = true;
}

/* ===== (optional) alte setXY-Tools bleiben verfügbar, werden aber nicht mehr genutzt ===== */
inline void setBranchPixel(CRGB* arr, bool reversed, uint8_t fromOutside, const CRGB& col) {
  uint8_t i = fromOutside - 1;                   // 0..21
  if (reversed) i = BRANCH_LEN - 1 - i;
  arr[i] = col;
}
inline void setXY(int y, int x, const CRGB &c) {
  if (x == 0) { if (y >= 0 && y < (int)TRUNK_LEN) TRUNK[y] = c; return; }
  if (y < 0 || y >= (int)TRUNK_LEN) return;
  uint8_t fromOutside = abs(x);
  if (fromOutside < 1 || fromOutside > BRANCH_LEN) return;

  if (x > 0) { // rechts
    if      (y == Y_RIGHT[0]) setBranchPixel(R1, R_REVERSED[0], fromOutside, c);
    else if (y == Y_RIGHT[1]) setBranchPixel(R2, R_REVERSED[1], fromOutside, c);
    else if (y == Y_RIGHT[2]) setBranchPixel(R3, R_REVERSED[2], fromOutside, c);
  } else {     // links
    if      (y == Y_LEFT[0])  setBranchPixel(L1, L_REVERSED[0], fromOutside, c);
    else if (y == Y_LEFT[1])  setBranchPixel(L2, L_REVERSED[1], fromOutside, c);
    else if (y == Y_LEFT[2])  setBranchPixel(L3, L_REVERSED[2], fromOutside, c);
  }
}

/* ================== SZENEN ================== */
void sceneAllWarm() {
  clearAllLayout();
  // Stamm
  fill_solid(TRUNK, TRUNK_LEN, WARM);
  // Äste
  fill_solid(L1, BRANCH_LEN, WARM);
  fill_solid(L2, BRANCH_LEN, WARM);
  fill_solid(L3, BRANCH_LEN, WARM);
  fill_solid(R1, BRANCH_LEN, WARM);
  fill_solid(R2, BRANCH_LEN, WARM);
  fill_solid(R3, BRANCH_LEN, WARM);
  // kein show() hier – Dirty-Flag übernimmt
}

/* Mojito statisch */
void sceneMojitoStatic() {
  clearAllLayout();

  const int y_bottom   = 0;
  const int y_lWineEnd = Y_LEFT[2] + 6;
  const int y_midStart = Y_RIGHT[0] - 8;
  const int y_midEnd   = Y_RIGHT[1] + 8;
  const int y_topStart = Y_RIGHT[2] - 5;

  colorTrunkRange(y_bottom,  y_lWineEnd, MOJITO_AMBER);                     // unten: Amber
  colorTrunkRange(y_midStart,y_midEnd,  blend(MOJITO_MINT, MOJITO_LIME, 96)); // Mitte: Grün
  colorTrunkRange(y_topStart,TRUNK_LEN-1, MOJITO_ICE);                      // oben: Eis/Soda

  // Linke Äste
  fillBranchRaw(L1, L_REVERSED[0], 6, MOJITO_AMBER, 16, WARM);
  fillBranchRaw(L2, L_REVERSED[1], 4, MOJITO_AMBER, 18, blend(WARM, MOJITO_AMBER, 128));
  fillBranchRaw(L3, L_REVERSED[2], 6, MOJITO_AMBER, 16, WARM);

  // Rechte Äste
  fillBranchRaw(R1, R_REVERSED[0],  8, MOJITO_LIME, 14, MOJITO_MINT);
  fillBranchRaw(R2, R_REVERSED[1], 10, MOJITO_LIME, 12, MOJITO_MINT);
  fillBranchRaw(R3, R_REVERSED[2], 12, MOJITO_LIME, 10, MOJITO_SODA);
}

/* Mojito Softfade (ultra langsam) */
bool mojitoSoftActive = false;
unsigned long mojitoNextUpdate = 0;
const unsigned long MOJITO_PERIOD_MS = 180000UL; // 3 Minuten
const unsigned long MOJITO_STEP_MS   = 2000UL;   // Update-Intervall

void startMojitoSoftfade()  { mojitoSoftActive = true;  mojitoNextUpdate = 0; }
void stopMojitoSoftfade()   { mojitoSoftActive = false; }

void renderMojitoPhase(uint8_t kGreen, uint8_t kIce) {
  clearAllLayout();

  const int y_bottom   = 0;
  const int y_lWineEnd = Y_LEFT[2] + 6;
  const int y_midStart = Y_RIGHT[0] - 8;
  const int y_midEnd   = Y_RIGHT[1] + 8;
  const int y_topStart = Y_RIGHT[2] - 5;

  colorTrunkRange(y_bottom, y_lWineEnd, MOJITO_AMBER);
  colorTrunkRange(y_midStart, y_midEnd, blend(MOJITO_MINT, MOJITO_LIME, kGreen));
  colorTrunkRange(y_topStart, TRUNK_LEN-1, blend(MOJITO_ICE, MOJITO_SODA, kIce));

  fillBranchRaw(L1, L_REVERSED[0], 6, MOJITO_AMBER, 16, WARM);
  fillBranchRaw(L2, L_REVERSED[1], 4, MOJITO_AMBER, 18, blend(WARM, MOJITO_AMBER, 128));
  fillBranchRaw(L3, L_REVERSED[2], 6, MOJITO_AMBER, 16, WARM);

  fillBranchRaw(R1, R_REVERSED[0],  8, blend(MOJITO_LIME, MOJITO_MINT, kGreen), 14, MOJITO_MINT);
  fillBranchRaw(R2, R_REVERSED[1], 10, blend(MOJITO_LIME, MOJITO_MINT, kGreen), 12, MOJITO_MINT);
  fillBranchRaw(R3, R_REVERSED[2], 12, blend(MOJITO_LIME, MOJITO_SODA, kIce),   10, MOJITO_SODA);
}

void updateMojitoSoftfade() {
  if (!mojitoSoftActive) return;
  unsigned long now = millis();
  if (now < mojitoNextUpdate) return;
  mojitoNextUpdate = now + MOJITO_STEP_MS;

  uint8_t t  = sin8((uint32_t)(now % MOJITO_PERIOD_MS) * 255 / (MOJITO_PERIOD_MS/2));
  uint8_t t2 = sin8((uint32_t)((now + MOJITO_PERIOD_MS/6) % MOJITO_PERIOD_MS) * 255 / (MOJITO_PERIOD_MS/2));

  renderMojitoPhase(t, t2);
}

/* Aperol statisch */
void sceneAperolStatic() {
  clearAllLayout();

  const int y_bottom   = 0;
  const int y_lWineEnd = Y_LEFT[2] + 6;
  const int y_midStart = Y_RIGHT[0] - 8;
  const int y_midEnd   = Y_RIGHT[1] + 8;
  const int y_topStart = Y_RIGHT[2] - 5;

  // Stamm
  colorTrunkRange(y_bottom,  y_lWineEnd, AP_BLOOD);
  colorTrunkRange(y_midStart,y_midEnd,  AP_ORANGE);
  colorTrunkRange(y_topStart,TRUNK_LEN-1, AP_PROSECCO);

  // Linke Äste
  fillBranchRaw(L1, L_REVERSED[0], 6, AP_BLOOD,     16, AP_ORANGE);
  fillBranchRaw(L2, L_REVERSED[1], 4, AP_PEEL,      18, blend(AP_PEEL, AP_PROSECCO, 128));
  fillBranchRaw(L3, L_REVERSED[2], 6, AP_ORANGE,    16, AP_PROSECCO);

  // Rechte Äste
  fillBranchRaw(R1, R_REVERSED[0],  8, AP_ORANGE,   14, AP_PROSECCO);
  fillBranchRaw(R2, R_REVERSED[1], 10, AP_BLOOD,    12, AP_ORANGE);
  fillBranchRaw(R3, R_REVERSED[2], 12, AP_ORANGE,   10, AP_PROSECCO);
}

bool aperolSoftActive = false;
unsigned long aperolNextUpdate = 0;
const unsigned long APEROL_PERIOD_MS = 180000UL; // 3 Minuten
const unsigned long APEROL_STEP_MS   = 2000UL;

void startAperolSoftfade(){ aperolSoftActive = true;  aperolNextUpdate = 0; }
void stopAperolSoftfade() { aperolSoftActive = false; }

static void renderAperolPhase(uint8_t kOrange, uint8_t kGold) {
  clearAllLayout();

  const int y_bottom   = 0;
  const int y_lWineEnd = Y_LEFT[2] + 6;
  const int y_midStart = Y_RIGHT[0] - 8;
  const int y_midEnd   = Y_RIGHT[1] + 8;
  const int y_topStart = Y_RIGHT[2] - 5;

  // Stamm
  colorTrunkRange(y_bottom,  y_lWineEnd, blend(AP_BLOOD,   AP_ORANGE,   kOrange));
  colorTrunkRange(y_midStart,y_midEnd,   blend(AP_ORANGE,  AP_PEEL,     kOrange));
  colorTrunkRange(y_topStart,TRUNK_LEN-1,blend(AP_PROSECCO,AP_ORANGE,   kGold));

  // Linke Äste
  fillBranchRaw(L1, L_REVERSED[0], 6, blend(AP_BLOOD, AP_ORANGE, kOrange), 16, AP_ORANGE);
  fillBranchRaw(L2, L_REVERSED[1], 4, AP_ORANGE, 18, blend(AP_ORANGE, AP_PROSECCO, kGold));
  fillBranchRaw(L3, L_REVERSED[2], 6, AP_ORANGE, 16, blend(AP_PROSECCO, AP_ORANGE, kGold));

  // Rechte Äste
  fillBranchRaw(R1, R_REVERSED[0],  8, blend(AP_ORANGE, AP_BLOOD, kOrange), 14, blend(AP_ORANGE, AP_PROSECCO, kGold));
  fillBranchRaw(R2, R_REVERSED[1], 10, blend(AP_ORANGE, AP_BLOOD, kOrange), 12, blend(AP_ORANGE, AP_PROSECCO, kGold));
  fillBranchRaw(R3, R_REVERSED[2], 12, blend(AP_ORANGE, AP_PEEL,  kOrange), 10, blend(AP_PROSECCO, AP_ORANGE, kGold));
}

void updateAperolSoftfade() {
  if (!aperolSoftActive) return;
  unsigned long now = millis();
  if (now < aperolNextUpdate) return;
  aperolNextUpdate = now + APEROL_STEP_MS;

  uint8_t o = sin8((uint32_t)(now % APEROL_PERIOD_MS) * 255 / (APEROL_PERIOD_MS/2));
  uint8_t g = sin8((uint32_t)((now + APEROL_PERIOD_MS/4) % APEROL_PERIOD_MS) * 255 / (APEROL_PERIOD_MS/2));

  renderAperolPhase(o, g);
}

/* ================== BUTTON-LOGIK (polling, entprellt) ================== */
const unsigned long DEBOUNCE_MS = 30;
const unsigned long LONGPRESS_MS = 800;
const unsigned long DIM_INTERVAL = 40;
const uint8_t DIM_STEP = 3;

bool powerPressed=false, greenPressed=false, pinkPressed=false;
bool powerLongActive=false, greenLongActive=false, pinkLongActive=false;
bool powerShortFlag=false, greenShortFlag=false, pinkShortFlag=false;
unsigned long pressStartPower=0, pressStartGreen=0, pressStartPink=0;
unsigned long lastDimStep=0;

// Mojito-Mode: 0=statisch, 1=softfade
uint8_t mojitoMode = 0;

// Aperol-Mode: 0=statisch noch nicht gezeigt, 1=toggle softfade
uint8_t aperolMode = 0;

void handleButton(int pin, bool& pressed, bool& longActive, bool& shortFlag, unsigned long& pressStart) {
  static unsigned long lastChangeP=0, lastChangeG=0, lastChangeK=0;
  unsigned long &lastChange = (pin==BTN_POWER)?lastChangeP:((pin==BTN_GREEN)?lastChangeG:lastChangeK);
  bool reading = (digitalRead(pin)==LOW);
  unsigned long now=millis();
  if (reading != pressed && (now-lastChange)>DEBOUNCE_MS) {
    pressed = reading; lastChange=now;
    if (pressed) { pressStart=now; longActive=false; }
    else { if (!longActive) shortFlag=true; }
  }
}

// ======= Status für Web abrufbar machen =======
bool g_powerOn = true;  // einfacher Status (AUS = alles Black)

uint8_t getBrightnessForWeb(){ return brightness; }
bool    getPowerForWeb(){ return g_powerOn; }

// Hook den du schon hast, nur erweitert:
inline void showIfDirty() {
  if (frameDirty) {
    FastLED.show();
    LedWeb::broadcastFrame();    // <---- NEU: live an Clients senden
    frameDirty = false;
  }
}

// Setter, die vom Web aufgerufen werden:
void webSetBrightness(uint8_t b){
  setBrightness(b);
}

void webSetPower(bool on){
  g_powerOn = on;
  if (!on) {
    // AUS
    stopMojitoSoftfade(); mojitoMode=0;
    stopAperolSoftfade(); aperolMode=0;
    clearAllLayout();
  } else {
    // AN – Startszene
    setBrightness(brightness); // markiert dirty
    sceneAllWarm();
  }
}


/* ================== SETUP ================== */
void setup() {
  FastLED.addLeds<LED_TYPE, PIN_TRUNK, COLOR_ORDER>(leds_trunk, TRUNK_LEN);
  FastLED.addLeds<LED_TYPE, PIN_L1,    COLOR_ORDER>(leds_l1, BRANCH_LEN);
  FastLED.addLeds<LED_TYPE, PIN_L2,    COLOR_ORDER>(leds_l2, BRANCH_LEN);
  FastLED.addLeds<LED_TYPE, PIN_L3,    COLOR_ORDER>(leds_l3, BRANCH_LEN);
  FastLED.addLeds<LED_TYPE, PIN_R1,    COLOR_ORDER>(leds_r1, BRANCH_LEN);
  FastLED.addLeds<LED_TYPE, PIN_R2,    COLOR_ORDER>(leds_r2, BRANCH_LEN);
  FastLED.addLeds<LED_TYPE, PIN_R3,    COLOR_ORDER>(leds_r3, BRANCH_LEN);

  setBrightness(BRIGHTNESS_DEF);
  clearAllLayout(); // kein FastLED.show() hier – showIfDirty macht das im ersten loop()

  pinMode(BTN_POWER, INPUT);        // extern 10k
  pinMode(BTN_PINK,  INPUT);        // extern 10k
  pinMode(BTN_GREEN, INPUT_PULLUP); // intern PullUp

  // ---- LedWeb anbinden ----
  LedWeb::attachLayout(
    TRUNK, TRUNK_LEN, L1, L2, L3, R1, R2, R3,
    Y_LEFT, Y_RIGHT, L_REVERSED, R_REVERSED, BRANCH_LEN
  );
  LedWeb::setBrightnessSetter(&webSetBrightness);
  LedWeb::setPowerSetter(&webSetPower);
  LedWeb::setStatusGetter(&getBrightnessForWeb, &getPowerForWeb);
  LedWeb::begin("Barschrank", "12345678");   // Passwort min. 8 Zeichen!
}

/* ================== LOOP ================== */
void loop() {
  handleButton(BTN_POWER, powerPressed, powerLongActive, powerShortFlag, pressStartPower);
  handleButton(BTN_GREEN, greenPressed, greenLongActive, greenShortFlag, pressStartGreen);
  handleButton(BTN_PINK,  pinkPressed,  pinkLongActive,  pinkShortFlag,  pressStartPink);

  unsigned long now=millis();

  // --- POWER ---
  if (powerShortFlag) {
    powerShortFlag=false;
    g_powerOn = true;                     // <--- NEU
    stopMojitoSoftfade(); mojitoMode = 0;
    stopAperolSoftfade(); aperolMode = 0;

    setBrightness(BRIGHTNESS_DEF);
    sceneAllWarm();    // Reset warmweiß
  }
  if (powerPressed && (now-pressStartPower>LONGPRESS_MS)) {
    powerLongActive=true;
    g_powerOn = false;                    // <--- NEU
    stopMojitoSoftfade(); mojitoMode=0;
    stopAperolSoftfade(); aperolMode=0;
    clearAllLayout();   // AUS
  }

  // --- GREEN (kurz = Mojito statisch <-> Softfade | lang halten = heller) ---
  if (greenShortFlag && !greenLongActive) {
    greenShortFlag=false;
    // Mojito-Welt aktivieren, Aperol stoppen
    stopAperolSoftfade(); aperolMode = 0;

    if (mojitoMode==0) {             // statisch einschalten
      stopMojitoSoftfade();
      sceneMojitoStatic();
      mojitoMode = 1;                // nächster Druck toggelt Softfade
    } else {                         // Softfade toggeln
      if (!mojitoSoftActive) { startMojitoSoftfade(); }
      else { stopMojitoSoftfade(); sceneMojitoStatic(); }
    }
  }
  if (greenPressed && (now-pressStartGreen>LONGPRESS_MS)) {
    greenLongActive=true;
    if (now-lastDimStep>DIM_INTERVAL) {
      if (brightness<BRIGHTNESS_MAX) {
        setBrightness(min<uint8_t>(brightness+DIM_STEP, BRIGHTNESS_MAX));
      }
      lastDimStep=now;
    }
  }

  // --- PINK (kurz = Aperol statisch <-> Softfade | lang halten = dunkler) ---
  if (pinkShortFlag && !pinkLongActive) {
    pinkShortFlag=false;
    stopMojitoSoftfade(); mojitoMode = 0;

    if (aperolMode==0) {              // 1. Druck: statisch
      stopAperolSoftfade();
      sceneAperolStatic();
      aperolMode = 1;                 // nächster Druck toggelt Softfade
    } else {
      if (!aperolSoftActive) { startAperolSoftfade(); }
      else { stopAperolSoftfade(); sceneAperolStatic(); }
    }
  }
  if (pinkPressed && (now-pressStartPink>LONGPRESS_MS)) {
    pinkLongActive=true;
    if (now-lastDimStep>DIM_INTERVAL) {
      if (brightness>BRIGHTNESS_MIN) {
        setBrightness(max<uint8_t>(brightness-DIM_STEP, BRIGHTNESS_MIN));
      }
      lastDimStep=now;
    }
  }

  // Mojito-Softfade (ultra langsam)
  updateMojitoSoftfade();
  // Aperol-Softfade (ultra langsam)
  updateAperolSoftfade();

  // nur ein einziges show() pro Loop – wenn nötig
  showIfDirty();
}
