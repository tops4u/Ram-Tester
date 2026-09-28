#line 1 "C:\\Users\\Andreas\\Documents\\DRAM_Decay_Test\\common.h"
// common.h - Shared definitions for the DRAM Decay Test (retention characterisation)
//=======================================================================================
//
// Hardware: RAM-Tester PCB (ATmega328P @ 16 MHz, SSD1306 OLED on SW-I2C PB5/PB4,
// bi-colour LED on the same two pins, DIP switches feed VCC to ONE socket and are read
// back on PD2 (16-pin), PD3 (18-pin) and PC5 (20-pin)). Same board as Ram_Tester 5.1.2.
//
// Files:
//   DRAM_Decay_Test.ino  run orchestration (row scheduler: write r, read r at +T) + search
//   common.h/.cpp        tunables, display screens, formatting, LED
//   patterns.h/.cpp      data backgrounds (checkerboard, solid, stripes, pseudo-random)
//   chip.h               chip-driver interface (geometry, 4 functions, marker masks)
//   chip_4164.cpp        16-pin x1 driver: 4164, 41256/41257, 4816
//   chip_4464.cpp        18-pin x4 driver: 4464, 4416
//   chip_514256.cpp      20-pin x4 driver: 514256/514258, 514400/514402
//   src/U8g2/            bundled, SW-I2C-accelerated u8g2 (from Ram_Tester 5.1.2)
//
//=======================================================================================

#ifndef DECAY_COMMON_H
#define DECAY_COMMON_H

#include <Arduino.h>
#include <avr/pgmspace.h>

// Shown on the splash screen and in every message title ("DRAM Decay v1.0").
#define DECAY_VERSION "1.0"

//=======================================================================================
// TUNABLES
//=======================================================================================

// --- Search (all times in microseconds) ---------------------------------------------
// First hold to try: 2 ms = the tightest refresh spec of all supported parts (2 ms
// 4164, 4816). The ramp doubles from here, so every spec value is checked on the way up
// and a part below spec is still measured. Raised to the floor T_MIN if that is longer.
#define START_US         2000UL
// Exponential ramp factor while no failure has been seen yet.
#define GALLOP_FACTOR    2
// Longest hold ever tried, bound by the 32-bit micros() scheduler (< 35 min). A part
// that still holds at this value is reported as ">= MAX".
#define MAX_HOLD_US      1800000000UL
// Bisection stops when (hi - lo) <= max(RES_ABS_US, hi * RES_PCT / 100).
#define RES_PCT          5
#define RES_ABS_US       100UL

// --- Row scheduling --------------------------------------------------------------------
// Row r is written at t0 + r*P and read at t0 + r*P + T. When T is shorter than a full
// pass the two event streams interleave (one write and one read per period P); the run
// planner picks P so that every event has a free gap of at least G_MIN before it, where
// G_MIN = measured pattern-generation time + SLACK_US (loop overhead, timer ISR).
// T_MIN = W + G_MIN (write one row, gap, read it) -- ~0.8 ms on a 4164, ~1.1 ms on a
// 514256, ~2 ms on a 514400.
#define SLACK_US         80

// --- Display -------------------------------------------------------------------------
// Countdown refresh during the hold (ms). A full-screen render costs ~30-60 ms and is
// only started while at least HOLD_DISPLAY_GUARD_US remain before the read pass.
#define HOLD_DISPLAY_MS       1000UL
#define HOLD_DISPLAY_GUARD_US 400000L
// How long the big "result" screen stays up when the retention value changes.
#define RESULT_SCREEN_MS      1500

// --- Logic-analyser markers -----------------------------------------------------------
// Row and busy markers on pins the active socket leaves free (see chip.h, markRow /
// markBusy): 16-pin: ROW = PD4, BUSY+ERR = PD5 (reachable at 20-pin socket pins 11/12);
// 18-pin: ROW = PD4 only (NC pin of that socket); 20-pin: none (every pin is in use).
// See docs/LA_Testkonzept.md. Set to 0 to keep the pins as plain inputs everywhere.
#define LA_MARKERS 1

//=======================================================================================
// LOW-LEVEL HELPERS
//=======================================================================================

#define NOP __asm__ __volatile__("nop\n\t")
#define SBI(port, bit) __asm__ __volatile__("sbi %0, %1" ::"I"(_SFR_IO_ADDR(port)), "I"(bit))
#define CBI(port, bit) __asm__ __volatile__("cbi %0, %1" ::"I"(_SFR_IO_ADDR(port)), "I"(bit))

// Marker helpers, driven by the active driver's PORTD masks (0 = no pin available).
// Writing a 1 to PINx toggles the PORTx bit -- one instruction, no read-modify-write.
#if LA_MARKERS
#define MARK_ROW(mask)      do { if (mask) PIND = (mask); } while (0)
#define MARK_BUSY_ON(mask)  do { PORTD |= (mask); } while (0)
#define MARK_BUSY_OFF(mask) do { PORTD &= (uint8_t)~(mask); } while (0)
#else
#define MARK_ROW(mask)      do { } while (0)
#define MARK_BUSY_ON(mask)  do { } while (0)
#define MARK_BUSY_OFF(mask) do { } while (0)
#endif

// DIP switch -> socket class (same encoding as the tester)
#define MODE_NONE  0
#define MODE_16PIN 2
#define MODE_18PIN 4
#define MODE_20PIN 5

//=======================================================================================
// DISPLAY
//=======================================================================================

#include "src/U8g2/U8g2lib.h"
extern U8G2_SSD1306_128X64_NONAME_2_SW_I2C display;

#define FONT_BIG   u8g2_font_7x14B_tr      // 18 chars/line
#define FONT_SMALL u8g2_font_6x10_tr       // 21 chars/line, 6 lines
#define FONT_NUM   u8g2_font_logisoso16_tn // digits only, result screen

// Release SDA/SCL to the open-drain idle state before every transfer (the passes drive
// PB4 as address line A4 and park PB5 low = red LED off). Same macro as the tester.
#define OLED_PREP() do { DDRB &= ~((1 << 4) | (1 << 5)); PORTB |= (1 << 4) | (1 << 5); } while (0)
#define OLED_BEGIN() OLED_PREP(); display.firstPage(); do {
#define OLED_END() } while (display.nextPage()); CBI(PORTB, 5)

// Everything the run screen shows. Filled by the .ino, rendered by dispRun().
struct RunView {
  const char *chip;      // PROGMEM chip name
  uint16_t run;          // run counter (1-based)
  const char *phase;     // PROGMEM phase word ("Ramp", "Bisect", ...)
  uint16_t phaseNum;     // appended to the phase word when != 0 (Confirm 3/10 -> num)
  uint8_t phaseDen;      //   ... and this (0 = none)
  const char *pat;       // RAM pattern name (<= 7 chars)
  uint32_t t;            // hold under test (us)
  const char *stage;     // PROGMEM stage word ("Write", "Hold", "Read")
  uint16_t holdLeft;     // seconds left, shown after "Hold" when stage == Hold
  uint32_t lo, hi;       // bracket of the current pattern (us); 0 / T_INF = unknown
  uint16_t lastErr;      // 0xFFFF = no run finished yet
  uint16_t lastRow, lastCol;
  uint16_t lastLateUs;   // worst scheduling lateness of the last run (age error bound)
  uint32_t result;       // T_INF = unknown (us)
  uint8_t  npat;         // patterns confirmed at result
  bool     capped;       // result reached MAX_HOLD_US
};

#define T_INF 0xFFFFFFFFUL

void dispInit(void);
void dispMsg(const __FlashStringHelper *l1, const __FlashStringHelper *l2);
void dispBoot(const char *chip, uint16_t rows, uint16_t cols, uint16_t specUs,
              uint16_t wUs, uint16_t rUs, uint32_t tminUs);
void dispRun(const RunView *v);
void dispResult(uint32_t us, uint8_t npat, bool capped, bool dead);

// "2.35ms" / "12.3ms" / "123ms" / "1.23s" / "12.3s" / "123s" / "1234s"; out >= 8 bytes.
void fmtTime(char *out, uint32_t us);

// Red LED (PB5). Only meaningful in terminal states -- the pin doubles as OLED SCL.
void ledRed(bool on);

#endif // DECAY_COMMON_H
