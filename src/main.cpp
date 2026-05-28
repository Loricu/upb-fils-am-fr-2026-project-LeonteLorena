/* ============================================================================
 *  ETCH-A-SYNTH  —  Interactive Visual Synthesizer
 *  ---------------------------------------------------------------------------
 *  An Etch-a-Sketch / step-sequencer hybrid for the Arduino Nano.
 *
 *  The user draws a curve on a 128x64 SSD1306 OLED using four directional
 *  buttons. The X axis is time (sequence step), the Y axis is pitch. Pressing
 *  PLAY scans the drawing left-to-right and sonifies it through a passive
 *  buzzer. CLEAR wipes the canvas and returns to drawing mode.
 *
 *  Target : Arduino Nano V3  (ATmega328P @ 16 MHz, 32 KB Flash, 2 KB SRAM)
 *  Display: SSD1306 128x64 I2C OLED
 *  Audio  : Passive buzzer (KY-006) on a PWM-capable pin
 *
 *  Author : Senior Embedded Systems Engineer
 *  License: MIT
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/* ============================================================================
 *  DISPLAY CONFIGURATION
 * ==========================================================================*/
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1        // SSD1306 shares the MCU reset (no dedicated pin)
#define OLED_ADDR      0x3C      // Common address; some modules use 0x3D

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* ============================================================================
 *  PIN MAP
 *  ---------------------------------------------------------------------------
 *  Buzzer is on D9. The original spec suggested D9 as well — good choice,
 *  because tone() on the Nano uses Timer2, and D9/D10 are driven by Timer1.
 *  tone() does NOT touch Timer1, so PWM on D9/D10 is otherwise free and there
 *  is no conflict with the Wire/I2C peripheral. We keep D9.
 * ==========================================================================*/
#define PIN_BTN_UP     2
#define PIN_BTN_DOWN   3
#define PIN_BTN_LEFT   4
#define PIN_BTN_RIGHT  5
#define PIN_BTN_PLAY   6
#define PIN_BTN_CLEAR  7
#define PIN_BUZZER     9

/* ============================================================================
 *  DRAWING DATA MODEL
 *  ---------------------------------------------------------------------------
 *  We store ONE Y value per X column: uint8_t canvas[128].
 *
 *  Rationale (memory architecture):
 *    - A full framebuffer for 128x64 already costs 1024 bytes (half of SRAM)
 *      and is owned by the Adafruit_SSD1306 library internally.
 *    - Storing the drawing as a 1-bit bitmap would cost another 1024 bytes —
 *      impossible alongside the library buffer on a 2 KB part.
 *    - A "one Y per X" height-map is the natural representation for a
 *      sequencer (X = time, Y = pitch) and costs only 128 bytes.
 *    - It also guarantees a single, well-defined note per time step, which is
 *      exactly what monophonic playback needs.
 *
 *  Sentinel: CANVAS_EMPTY (0xFF) marks a column the user has not drawn yet.
 *  Valid Y values are 0..63, so 0xFF can never collide with real data.
 * ==========================================================================*/
static const uint8_t CANVAS_EMPTY = 0xFF;
uint8_t canvas[SCREEN_WIDTH];

/* Cursor state (current drawing position). */
int16_t cursorX = 0;
int16_t cursorY = SCREEN_HEIGHT / 2;

/* Application mode. */
enum Mode : uint8_t { MODE_DRAW, MODE_PLAY };
Mode mode = MODE_DRAW;

/* ============================================================================
 *  BUTTON HANDLING  (non-blocking software debounce)
 *  ---------------------------------------------------------------------------
 *  Strategy:
 *    - All buttons use INPUT_PULLUP; a press reads LOW.
 *    - We sample every loop and timestamp the last change. A state is only
 *      accepted once it has been stable for DEBOUNCE_MS.
 *    - Two event types are exposed:
 *        pressedEdge()  -> true exactly once on a clean HIGH->LOW transition
 *                          (used for PLAY / CLEAR: one action per press).
 *        isHeld()       -> true while stably held (used for cursor movement,
 *                          with our own repeat timer so holding a direction
 *                          glides the cursor smoothly).
 * ==========================================================================*/
static const uint8_t  DEBOUNCE_MS   = 12;   // contact settling window
static const uint16_t MOVE_REPEAT_MS = 28;  // cursor auto-repeat while held

struct Button {
  uint8_t  pin;
  uint8_t  stableState;   // last debounced level (HIGH = released)
  uint8_t  lastReading;   // last raw read
  uint32_t lastChangeMs;  // when lastReading last changed
  bool     edgeConsumed;  // guards one-shot edge events

  void begin(uint8_t p) {
    pin          = p;
    pinMode(pin, INPUT_PULLUP);
    stableState  = HIGH;
    lastReading  = HIGH;
    lastChangeMs = 0;
    edgeConsumed = true;
  }

  // Must be called every loop to update the debounced state.
  void update(uint32_t now) {
    uint8_t reading = digitalRead(pin);
    if (reading != lastReading) {
      lastReading  = reading;
      lastChangeMs = now;
    }
    if ((now - lastChangeMs) >= DEBOUNCE_MS && reading != stableState) {
      stableState = reading;
      if (stableState == LOW) edgeConsumed = false; // fresh press to consume
    }
  }

  bool isHeld() const { return stableState == LOW; }

  // Returns true once per physical press.
  bool pressedEdge() {
    if (stableState == LOW && !edgeConsumed) {
      edgeConsumed = true;
      return true;
    }
    return false;
  }
};

Button btnUp, btnDown, btnLeft, btnRight, btnPlay, btnClear;

/* ============================================================================
 *  FREQUENCY MAPPING
 *  ---------------------------------------------------------------------------
 *  Screen Y is inverted (0 = top). Top of screen = high pitch.
 *
 *  We map the 64 vertical pixels onto a musical range and QUANTIZE to a
 *  C-major scale. Quantization is worth the small cost: free-running linear
 *  pitch sounds like a siren, whereas snapping to scale degrees makes any
 *  drawing sound intentionally "musical".
 *
 *  Range: ~C3 (130 Hz) at the bottom up to ~C6 (1047 Hz) at the top,
 *  three octaves of a C-major scale (21 notes + top C = 22 steps).
 * ==========================================================================*/
const uint16_t SCALE[] PROGMEM = {
  131, 147, 165, 175, 196, 220, 247,   // C3 D3 E3 F3 G3 A3 B3
  262, 294, 330, 349, 392, 440, 494,   // C4 D4 E4 F4 G4 A4 B4
  523, 587, 659, 698, 784, 880, 988,   // C5 D5 E5 F5 G5 A5 B5
  1047                                  // C6
};
const uint8_t SCALE_LEN = sizeof(SCALE) / sizeof(SCALE[0]);

// Convert a Y pixel (0..63) into a quantized frequency in Hz.
uint16_t yToFrequency(uint8_t y) {
  // Invert so y=0 (top) -> highest scale index.
  uint8_t topIndex = SCALE_LEN - 1;
  // Map 0..63 -> topIndex..0
  uint16_t idx = (uint16_t)(SCREEN_HEIGHT - 1 - y) * topIndex / (SCREEN_HEIGHT - 1);
  if (idx > topIndex) idx = topIndex;
  return pgm_read_word(&SCALE[idx]);
}

/* ============================================================================
 *  CANVAS HELPERS
 * ==========================================================================*/
void clearCanvas() {
  for (uint8_t x = 0; x < SCREEN_WIDTH; x++) canvas[x] = CANVAS_EMPTY;
}

// Render the stored drawing (and optionally the cursor) to the OLED buffer.
void renderCanvas(bool showCursor) {
  display.clearDisplay();

  // Draw stored points. A 1px dot per column keeps the look clean and is cheap.
  for (uint8_t x = 0; x < SCREEN_WIDTH; x++) {
    if (canvas[x] != CANVAS_EMPTY) {
      display.drawPixel(x, canvas[x], SSD1306_WHITE);
    }
  }

  if (showCursor) {
    // A small plus-sign cursor so it stays visible over drawn pixels.
    display.drawPixel(cursorX, cursorY, SSD1306_WHITE);
    if (cursorX > 0)                display.drawPixel(cursorX - 1, cursorY, SSD1306_WHITE);
    if (cursorX < SCREEN_WIDTH - 1) display.drawPixel(cursorX + 1, cursorY, SSD1306_WHITE);
    if (cursorY > 0)                display.drawPixel(cursorX, cursorY - 1, SSD1306_WHITE);
    if (cursorY < SCREEN_HEIGHT - 1)display.drawPixel(cursorX, cursorY + 1, SSD1306_WHITE);
  }

  display.display();
}

/* ============================================================================
 *  SPLASH SCREEN
 * ==========================================================================*/
void showSplash() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(6, 8);
  display.println(F("ETCH-A"));
  display.setCursor(30, 28);
  display.println(F("SYNTH"));

  display.setTextSize(1);
  display.setCursor(10, 52);
  display.println(F("draw . play . repeat"));

  display.display();
  delay(1600);
}

/* ============================================================================
 *  DRAWING MODE
 *  ---------------------------------------------------------------------------
 *  - LEFT/RIGHT move the cursor in X and "commit" the trail at the new column.
 *  - UP/DOWN change the pitch (Y) of the current column.
 *  - Cursor is clamped to the screen.
 *  - We auto-repeat while a direction is held, using a dedicated timer so the
 *    motion is smooth but not runaway-fast.
 *
 *  Every column the cursor sits in is recorded into the canvas, so the drawing
 *  is reconstructed reliably for playback.
 * ==========================================================================*/
uint32_t lastMoveMs = 0;

void commitCursor() {
  canvas[cursorX] = (uint8_t)cursorY;   // record current pitch at this column
}

void handleDrawing(uint32_t now) {
  bool moved = false;

  if ((now - lastMoveMs) >= MOVE_REPEAT_MS) {
    if (btnUp.isHeld()    && cursorY > 0)                { cursorY--; moved = true; }
    if (btnDown.isHeld()  && cursorY < SCREEN_HEIGHT - 1){ cursorY++; moved = true; }
    if (btnLeft.isHeld()  && cursorX > 0)                { cursorX--; moved = true; }
    if (btnRight.isHeld() && cursorX < SCREEN_WIDTH - 1) { cursorX++; moved = true; }

    if (moved) {
      lastMoveMs = now;
      commitCursor();
      renderCanvas(true);
    }
  }
}

/* ============================================================================
 *  PLAYBACK MODE
 *  ---------------------------------------------------------------------------
 *  Scans X = 0..127. For each column:
 *    - If a point exists: convert Y to frequency, play it, advance indicator.
 *    - If the column is EMPTY: HOLD the previous note.
 *
 *  Empty-column policy — we HOLD. Reasoning:
 *    * SILENCE makes sparse drawings stutter and sound broken.
 *    * SKIP would desync the visual indicator from the audio timeline.
 *    * HOLD produces a continuous, legato line that matches the visual curve
 *      and is the most musical for a hand-drawn shape. The very first columns
 *      before any drawn point stay silent (nothing to hold yet).
 *
 *  We keep a per-step delay (STEP_MS) but poll CLEAR so playback can be
 *  interrupted. tone() is non-blocking (it runs on Timer2), so the only
 *  blocking element is the deliberate step delay.
 * ==========================================================================*/
static const uint16_t STEP_MS = 60;   // duration of each time step (~tempo)

void playback() {
  mode = MODE_PLAY;
  bool haveNote = false;

  for (uint8_t x = 0; x < SCREEN_WIDTH; x++) {
    // Allow the user to abort playback with CLEAR.
    btnClear.update(millis());
    if (btnClear.pressedEdge()) break;

    if (canvas[x] != CANVAS_EMPTY) {
      uint16_t f = yToFrequency(canvas[x]);
      tone(PIN_BUZZER, f);
      haveNote = true;
    }
    // else: HOLD — leave the current tone running.

    // Redraw with a vertical playback indicator at column x.
    display.clearDisplay();
    for (uint8_t i = 0; i < SCREEN_WIDTH; i++) {
      if (canvas[i] != CANVAS_EMPTY) display.drawPixel(i, canvas[i], SSD1306_WHITE);
    }
    if (haveNote) display.drawFastVLine(x, 0, SCREEN_HEIGHT, SSD1306_WHITE);
    display.display();

    delay(STEP_MS);
  }

  // Clean stop.
  noTone(PIN_BUZZER);
  mode = MODE_DRAW;
  renderCanvas(true);
}

/* ============================================================================
 *  SETUP
 * ==========================================================================*/
void setup() {
  // I2C: bump to 400 kHz (fast mode) so full-frame display() calls are quick,
  // which directly reduces perceived flicker during drawing/playback.
  Wire.begin();
  Wire.setClock(400000UL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    // If the OLED is missing/misaddressed there is no UI to show the error,
    // so we blink the on-board LED forever as a hardware fault indicator.
    pinMode(LED_BUILTIN, OUTPUT);
    for (;;) {
      digitalWrite(LED_BUILTIN, HIGH); delay(120);
      digitalWrite(LED_BUILTIN, LOW);  delay(120);
    }
  }

  btnUp.begin(PIN_BTN_UP);
  btnDown.begin(PIN_BTN_DOWN);
  btnLeft.begin(PIN_BTN_LEFT);
  btnRight.begin(PIN_BTN_RIGHT);
  btnPlay.begin(PIN_BTN_PLAY);
  btnClear.begin(PIN_BTN_CLEAR);

  pinMode(PIN_BUZZER, OUTPUT);

  clearCanvas();

  showSplash();

  // Seed the starting column so playback has a defined first note.
  commitCursor();
  renderCanvas(true);
}

/* ============================================================================
 *  MAIN LOOP
 * ==========================================================================*/
void loop() {
  uint32_t now = millis();


  // Always service the debouncers.
  btnUp.update(now);
  btnDown.update(now);
  btnLeft.update(now);
  btnRight.update(now);
  btnPlay.update(now);
  btnClear.update(now);

  // One-shot actions.
  if (btnPlay.pressedEdge()) {
    playback();
    return;                 // loop restarts fresh after playback
  }

  if (btnClear.pressedEdge()) {
    clearCanvas();
    cursorX = 0;
    cursorY = SCREEN_HEIGHT / 2;
    commitCursor();
    renderCanvas(true);
    return;
  }

  // Continuous drawing input.
  handleDrawing(now);
}
