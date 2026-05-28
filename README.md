# ETCH-A-SYNTH

An Etch-a-Sketch style musical step-sequencer for the Arduino Nano V3 (ATmega328P).

Draw a curve on the OLED with four directional buttons. The X axis is time, the Y axis is pitch. Press PLAY to hear your drawing — pitches are quantized to the C-major scale. Press CLEAR to start over.

---

## Hardware

| Component | Detail |
|-----------|--------|
| MCU | Arduino Nano V3 (ATmega328P @ 16 MHz) |
| Display | SSD1306 128×64 OLED (I2C, address `0x3C`) |
| Audio | Passive buzzer (e.g. KY-006) |
| Buttons | 6× momentary push-button (active-LOW with INPUT_PULLUP) |

### Pin map

| Signal | Arduino pin |
|--------|-------------|
| BTN_UP | D2 |
| BTN_DOWN | D3 |
| BTN_LEFT | D4 |
| BTN_RIGHT | D5 |
| BTN_PLAY | D6 |
| BTN_CLEAR | D7 |
| BUZZER | D9 |
| SDA (OLED) | A4 |
| SCL (OLED) | A5 |

---

## Project structure

```
proiect_lorena/
├── platformio.ini      # build & dependency configuration
├── src/
│   └── main.cpp        # complete firmware
├── include/            # project-local headers (currently empty)
├── lib/                # project-local libraries (currently empty)
└── README.md
```

---

## Building & flashing

### Prerequisites

Install the PlatformIO Core CLI:

```bash
pip install platformio
# or via the standalone installer:
curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py -o get-platformio.py
python3 get-platformio.py
```

### Build

```bash
cd proiect_lorena
pio run
```

### Flash (old-bootloader Nano clone, 57600 baud)

```bash
pio run --target upload
```

### Flash (new-bootloader Nano, 115200 baud)

```bash
pio run --environment nanoatmega328new --target upload
```

### Serial monitor

```bash
pio device monitor
```

---

## How to use

1. Power on — splash screen appears for ~1.6 s.
2. **Draw**: use UP / DOWN to change pitch; LEFT / RIGHT to move along the timeline. Every position the cursor visits is recorded.
3. **Play** (D6): scans the drawing left-to-right and plays each note through the buzzer. Empty columns sustain the last note (legato). A vertical playhead sweeps across the OLED.
4. **Clear** (D7): wipes the canvas and resets the cursor. Also aborts playback mid-sequence.

---

## Musical details

* **Scale**: C-major across 3 octaves (C3 – C6, 22 notes).
* **Pitch mapping**: top of screen → C6 (1047 Hz); bottom → C3 (131 Hz).
* **Tempo**: 60 ms per step (~16.7 steps/second, roughly ♩ = 250 BPM at 16th-note resolution). Adjust `STEP_MS` in `src/main.cpp` to taste.

---

## Memory footprint (estimated)

| Resource | Used | Available |
|----------|------|-----------|
| Flash | ~10–12 KB | 32 KB |
| SRAM | ~1.3 KB | 2 KB |

The canvas array costs exactly 128 bytes. The SSD1306 framebuffer (1024 bytes) is managed by the Adafruit library.

---

## License

MIT
