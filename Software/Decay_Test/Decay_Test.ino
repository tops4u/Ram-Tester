/*
  =====================================================================================
  DRAM Decay Test  v1.0  -  retention-time finder for the weakest cell (RAM-Tester PCB)
  =====================================================================================

  Author:   Andreas Hoffmann  (structure and hardware layer follow Ram_Tester 5.1.2)
  Licence:  GPL 3.0
  Version:  DECAY_VERSION in common.h -- shown on the splash screen and in every title.

  PURPOSE
    Find out how long a DRAM keeps its data WITHOUT refresh at the current temperature.
    The answer is defined by the weakest cell of the whole array, so every run writes the
    complete array, ages every cell for a chosen hold time T and reads the complete
    array back. The parts are specified for 2-16 ms of refresh; at room temperature the
    real value is typically tens of seconds to minutes. The tool measures from ~0.8 ms
    (T_MIN, per part) up to 30 minutes.

  SUPPORTED PARTS  (the DIP switch selects the socket, detect() the part)
    16-pin  chip_4164.cpp     4164 (64Kx1), 41256/41257 (256Kx1), 4816 (16Kx1)
    18-pin  chip_4464.cpp     4464 (64Kx4), 4416 (16Kx4)
    20-pin  chip_514256.cpp   514256/514258 (256Kx4), 514400/514402 (1Mx4)
    Adding a part or a socket: see chip.h and bindDriver() below.

  ONE RUN  (the unit of everything below)
    One pattern, all rows. Row r is written at t0 + r*P and read at t0 + r*P + T, with
    write and read cycle-matched cell for cell, so the age of EVERY cell is T +/- a few
    us -- whatever T is. For T shorter than a full pass the write and read streams
    simply interleave (write r+k .. read r .. write r+k+1 ..), exactly like the retention
    pipeline of the RAM tester, only with T as a free parameter: the run planner picks
    the period P so that one write, one read and two free gaps fit into it. Nothing has
    to be refreshed: between the write and the read of a row only OTHER rows are touched.
    Verified with the LA markers on D4/D5, see docs/LA_Testkonzept.md.

  SEARCH  (docs/Konzept_Retention_Suche.md)
    1. Ramp:    2 ms, 4 ms, 8 ms ... (x2) with the first pattern until the first failure.
    2. Bisect:  halve the bracket [last pass, first fail] until it is <= RES (5 % / 100 us).
                The lower end is the retention for that pattern.
    3. Confirm: every further fixed pattern (CB, CB~, 00, FF, Row, Row~, Col, Col~, Rnd,
                Rnd~) is run once at the current answer. Pass -> next pattern. Fail ->
                the bracket is reopened below the answer for THAT pattern (Down: halve
                until a pass, then Bisect) and the answer shrinks.
    4. Soak:    endless Rnd/Rnd~ pairs with new seeds at the answer, same rule. The
                displayed value only ever goes down: it is the largest hold that every
                pattern so far has survived.
    Cost: about (2 + 0.75*log2(T/res)) * T for the search plus one run of T per further
    pattern -- for T = 100 s roughly 10 minutes to the first value, 25 minutes for the
    confirmed one. Runs below one second add ~0.2-1 s of pass time each.

  DISPLAY (6 lines)
    <chip>                 Run <n>          run counter
    <phase [k/N]>          <pattern>        Ramp / Bisect / Down / Confirm k/N / Soak n
    Test <t>               Run|Hold <s>
    ok <lo>                bad <hi>         bracket of the current pattern
    Last: OK late <us> | Last: <n> err @<row>,<col>   previous run; "late" = worst
                                            scheduling error of that run (age bound)
    Ret. <R> (<k> pat)                      current answer, patterns confirmed at it
    Whenever the answer changes, and once the fixed set is complete, a big-number
    "Retention" screen is shown for 1.5 s.

  TERMINAL STATES (red LED on)
    "Set one DIP"       no DIP switch on, or more than one
    "No RAM / defect"   presence test failed
    "RAM defect"        the array does not even survive the calibration pass
    "Retention FAIL"    a pattern fails at the floor T_MIN during the search

  BUILD
    Arduino IDE / arduino-cli, MiniCore ATmega328P, 16 MHz external, LTO. No external
    libraries: the display driver is bundled in src/U8g2 (Ram_Tester 5.1.2 version with
    the ~600 kHz software-I2C patch, plus the 6x10 and logisoso16 fonts).

  VERSION HISTORY
    0.1   Prototype (kept in v1.0_archive/): monolithic, linear ramp 30 s + bisection,
          write-all / hold / read-all with a ~0.3 s floor, both polarities per trial,
          full-library U8g2 (~0.4 s per screen update).
    0.2   Rewrite around the chip-driver interface, 16-pin only: row scheduler with
          exact per-cell aging for any hold (floor ~0.8 ms, interleaved write/read
          streams below one pass length), exponential ramp + bisection + pattern
          confirmation + soak, ten data backgrounds, 6-line status screen, bundled fast
          U8g2, cycle-matched assembly cell loops, logic-analyser markers.
    1.0   First release. 18-pin (4464/4416) and 20-pin (514256/514258/514400/514402)
          drivers, socket table by DIP mode, per-driver LA marker pins, datasheet
          refresh spec and version number on the splash screen.
*/

#include "common.h"
#include "chip.h"
#include "patterns.h"

DramDriver g_dram;

//=======================================================================================
// RUN-TIME CONSTANTS (from calibration)
//=======================================================================================

static uint16_t g_wUs;        // slowest write row (cli .. sei), us
static uint16_t g_rUs;        // slowest read row, us
static uint16_t g_gMinUs;     // minimum free gap before an event: pattern gen + slack
static uint32_t g_tminUs;     // floor: W + G_MIN
static uint8_t  g_bits[MAX_COLS / 8];   // one row of pattern bits (one per column)

//=======================================================================================
// SEARCH STATE
//=======================================================================================

#define F_CAPPED 0x01

struct Search {
  uint32_t lo, hi;      // bracket of the CURRENT pattern: lo = largest pass (0 = none),
                        // hi = smallest fail (T_INF = none)
  uint32_t t;           // hold for the next run (us)
  uint32_t R;           // answer so far (min over converged patterns); T_INF = none yet
  uint16_t patIdx;      // 0 .. NUM_FIXED_PATTERNS-1 = fixed set, beyond = soak
  Pattern  pat;
  uint16_t run;         // completed runs
  uint8_t  npat;        // patterns confirmed at R
  uint8_t  flags;
};
static Search S;
static RunView V;
static char g_patName[8];

static const char ph_ramp[]    PROGMEM = "Ramp";
static const char ph_bisect[]  PROGMEM = "Bisect";
static const char ph_down[]    PROGMEM = "Down";
static const char ph_confirm[] PROGMEM = "Confirm";
static const char ph_soak[]    PROGMEM = "Soak";
static const char st_run[]     PROGMEM = "Run";
static const char st_hold[]    PROGMEM = "Hold";

static void fillView(const char *stage, uint16_t holdLeft);

static void halt(const __FlashStringHelper *l1, const __FlashStringHelper *l2) __attribute__((noreturn));
static void halt(const __FlashStringHelper *l1, const __FlashStringHelper *l2) {
  dispMsg(l1, l2);
  ledRed(true);
  for (;;) { }
}

//=======================================================================================
// RUN PLANNER
//=======================================================================================
//
// Two event streams with the same period P: write(r) at t0 + r*P, read(r) at t0 + r*P + T.
// Let W and R be the row durations and G the minimum gap an event needs in front of it
// (pattern generation + loop overhead).
//
//   Long holds: if the whole write pass fits before the first read (T >= rows*P + G),
//     P = max(W, R) + G and the streams never overlap: each pass runs at full speed and
//     the array is idle for the rest of T.
//
//   Short holds: every period holds one write, one read and two gaps:
//     |<-- W -->|<- g ->|<-- R -->|<- g ->|     P = W + R + 2g
//     write r+k            read r
//     The read of row r lands k periods plus (W + g) after its write, so with
//     phi = T - k*P the two conditions are  phi - W >= G  and  P - R - phi >= G, i.e.
//       P <= (T - W - G) / k          and          P >= (T + R + G) / (k + 1).
//     The planner takes the largest k for which that interval is not empty and puts P
//     in its middle (equal gaps); k = 0 is "write a row, wait T, read it" and gives the
//     floor T_MIN = W + G. Exact integer arithmetic -- no rounding can shrink a gap.
//
// Every row event is started by waiting for its exact time, so P only has to be large
// enough -- any jitter shows up as "late" (measured, displayed), never as a shift.

// Row period P for hold T; 0 if T is below the floor.
static uint32_t planPeriod(uint32_t T) {
  const uint32_t W = g_wUs, R = g_rUs, G = g_gMinUs;
  if (T < W + G) return 0;
  const uint32_t Pfast = (W > R ? W : R) + G;
  if (T >= (uint32_t)g_dram.rows * Pfast + G) return Pfast;   // passes do not overlap
  uint32_t k = (T - W - G) / (W + R + 2 * G);                   // upper estimate
  for (; k; k--) {
    uint32_t Pmax = (T - W - G) / k;
    uint32_t Pmin = (T + R + G + k) / (k + 1);
    if (Pmin <= Pmax) return Pmin + (Pmax - Pmin) / 2;
  }
  return T + R + G;                                             // k = 0
}

//=======================================================================================
// ONE RUN
//=======================================================================================

static inline void waitUntil(uint32_t target) {
  while ((int32_t)(micros() - target) < 0) { }
}

// Write + age + verify the whole array with pattern p and hold T. Returns the error
// count (saturated at 0xFFFE); first failing cell and the worst lateness via pointers.
static uint16_t runOnce(const Pattern *p, uint32_t T, uint16_t *firstRow, uint16_t *firstCol,
                        uint16_t *maxLate) {
  const uint32_t P = planPeriod(T);          // T >= T_MIN is guaranteed by the search
  const uint16_t rows = g_dram.rows;
  uint16_t nw = 0, nr = 0, total = 0, late = 0;
  uint32_t lastDisp = millis();

  const uint8_t mRow = g_dram.markRow, mBusy = g_dram.markBusy;
  g_dram.setupPorts();
  MARK_BUSY_ON(mBusy);
  uint32_t t0 = micros() + 2000UL;           // row 0 waits like every other row
  while (nr < rows) {
    // next event: the earlier of write(nw) and read(nr)
    uint32_t tW = t0 + (uint32_t)nw * P;
    uint32_t tR = t0 + (uint32_t)nr * P + T;
    bool doWrite = (nw < rows) && ((int32_t)(tW - tR) < 0);
    uint32_t tEv = doWrite ? tW : tR;
    uint16_t r = doWrite ? nw : nr;
    patRow(p, r, g_bits, g_dram.cols);

    // long idle gap (long holds only): countdown on the OLED, well clear of the event
    while ((int32_t)(tEv - micros()) > HOLD_DISPLAY_GUARD_US) {
      if ((uint32_t)(millis() - lastDisp) >= HOLD_DISPLAY_MS) {
        lastDisp = millis();
        int32_t rem = (int32_t)(tEv - micros());
        fillView(st_hold, (uint16_t)((rem + 999999L) / 1000000L));
        g_dram.setupPorts();                 // the OLED transfer drove PB4/PB5
      }
    }
    waitUntil(tEv);
    uint32_t l = micros() - tEv;             // 0..~5 us normally (micros granularity)
    if (l > late) late = (l > 0xFFFF) ? 0xFFFF : (uint16_t)l;
    MARK_ROW(mRow);
    cli();
    if (doWrite) {
      g_dram.writeRow(r, g_bits);
      nw++;
    } else {
      uint16_t c = 0;
      uint16_t e = g_dram.readRow(r, g_bits, &c);
      if (e) {
        if (!total) { *firstRow = r; *firstCol = c; }
        total = (total > 0xFFFEu - e) ? 0xFFFEu : (uint16_t)(total + e);
      }
      nr++;
    }
    sei();
  }
  MARK_BUSY_OFF(mBusy);
  *maxLate = late;
  return total;
}

// Measure the slowest write row, read row and pattern generation (random kind, the
// slowest) and derive G_MIN and T_MIN. Doubles as a functional check: the read pass
// follows the write pass immediately (~0.2 s), a part failing that is reported.
static void calibrate(void) {
  Pattern p;
  patFixed(PAT_RND, &p);
  patPrepare(&p);
  g_dram.setupPorts();
  uint16_t wMax = 0, rMax = 0, gMax = 0, errs = 0, c;
  for (uint8_t pass = 0; pass < 2; pass++) {
    for (uint16_t r = 0; r < g_dram.rows; r++) {
      uint32_t t = micros();
      patRow(&p, r, g_bits, g_dram.cols);
      uint16_t d = (uint16_t)(micros() - t);
      if (d > gMax) gMax = d;
      t = micros();
      cli();
      if (pass == 0) g_dram.writeRow(r, g_bits);
      else errs |= g_dram.readRow(r, g_bits, &c);
      sei();
      d = (uint16_t)(micros() - t);
      if (pass == 0) { if (d > wMax) wMax = d; }
      else           { if (d > rMax) rMax = d; }
    }
  }
  if (errs) halt(F("RAM defect"), F("fails within 1 pass"));
  g_wUs = wMax + 4;                          // + micros() granularity
  g_rUs = rMax + 4;
  g_gMinUs = gMax + SLACK_US;
  g_tminUs = (uint32_t)g_wUs + g_gMinUs;
}

//=======================================================================================
// SEARCH
//=======================================================================================

static uint32_t resolution(uint32_t hi) {
  uint32_t r = hi / 100 * RES_PCT;
  return r > RES_ABS_US ? r : RES_ABS_US;
}

static void nextPattern(void) {
  S.lo = 0;
  S.hi = T_INF;
  S.patIdx++;
  if (S.patIdx < NUM_FIXED_PATTERNS) patFixed((uint8_t)S.patIdx, &S.pat);
  else patSoak(S.patIdx - NUM_FIXED_PATTERNS, &S.pat);
}

// Pick the hold for the next run from the bracket (see the header).
static void nextCandidate(void) {
  if (S.hi == T_INF) {
    if (S.R == T_INF) S.t = S.lo ? S.lo * GALLOP_FACTOR : START_US;   // ramp
    else S.t = S.R;                                                    // confirm at the answer
  } else if (S.lo == 0) {
    S.t = S.hi / 2;                                                    // down: no pass yet
  } else {
    S.t = S.lo + (S.hi - S.lo) / 2;                                    // bisect
  }
  if (S.t < g_tminUs) S.t = g_tminUs;
  if (S.t > MAX_HOLD_US) S.t = MAX_HOLD_US;
}

static void afterRun(uint16_t errs) {
  S.run++;
  if (errs == 0) S.lo = S.t; else S.hi = S.t;

  if (errs && S.t <= g_tminUs) {           // cannot be measured any lower
    dispResult(0, S.npat, false, true);
    ledRed(true);
    for (;;) { }
  }

  bool conv = false;
  if (S.R != T_INF && S.lo >= S.R) conv = true;                                   // confirmed at the answer
  else if (S.hi != T_INF && S.lo && (S.hi - S.lo) <= resolution(S.hi)) conv = true; // bracket closed
  else if (S.lo >= MAX_HOLD_US) { conv = true; S.flags |= F_CAPPED; }             // still holds at the cap

  if (conv) {
    bool changed = (S.lo < S.R);
    if (changed) S.R = S.lo;
    S.npat++;
    if (changed || S.patIdx == NUM_FIXED_PATTERNS - 1) {
      dispResult(S.R, S.npat, S.flags & F_CAPPED, false);
      delay(RESULT_SCREEN_MS);
    }
    nextPattern();
  }
  nextCandidate();
}

//=======================================================================================
// DISPLAY GLUE
//=======================================================================================

static void fillView(const char *stage, uint16_t holdLeft) {
  V.chip = g_dram.name;
  V.run = S.run + 1;
  V.phaseNum = 0;
  V.phaseDen = 0;
  if (S.hi == T_INF) {
    if (S.R == T_INF) {
      V.phase = ph_ramp;
    } else if (S.patIdx < NUM_FIXED_PATTERNS) {
      V.phase = ph_confirm;
      V.phaseNum = S.patIdx + 1;
      V.phaseDen = NUM_FIXED_PATTERNS;
    } else {
      V.phase = ph_soak;
      V.phaseNum = S.patIdx - NUM_FIXED_PATTERNS + 1;
    }
  } else {
    V.phase = S.lo ? ph_bisect : ph_down;
  }
  V.pat = g_patName;
  V.t = S.t;
  V.stage = stage;
  V.holdLeft = holdLeft;
  V.lo = S.lo;
  V.hi = S.hi;
  V.result = S.R;
  V.npat = S.npat;
  V.capped = S.flags & F_CAPPED;
  dispRun(&V);
}

//=======================================================================================
// SETUP / LOOP
//=======================================================================================

// DIP mode -> driver. To add a socket or family: write the driver (chip.h), add a line.
static bool bindDriver(uint8_t mode) {
  switch (mode) {
    case MODE_16PIN: bind_16pin_x1(); return true;
    case MODE_18PIN: bind_18pin_x4(); return true;
    case MODE_20PIN: bind_20pin_x4(); return true;
    default:         return false;
  }
}

void setup() {
  // Read the DIP switches with every pin still a quiet input, before the display init
  // drives PB4 (see Ram_Tester 5.1.2 setup()).
  DDRB &= 0b11100000;
  DDRC &= 0b11000000;
  DDRD = 0x00;
  uint8_t mode = MODE_NONE;
  if (PINC & (1 << PC5)) mode += MODE_20PIN;
  if (PIND & (1 << PD3)) mode += MODE_18PIN;
  if (PIND & (1 << PD2)) mode += MODE_16PIN;

  dispInit();
  if (!bindDriver(mode)) halt(F("Set one DIP"), F("16 / 18 / 20-pin"));

  g_dram.setupPorts();
  if (!g_dram.detect()) halt(F("No RAM / defect"), F("check socket"));

  calibrate();
  dispBoot(g_dram.name, g_dram.rows, g_dram.cols, g_dram.specUs, g_wUs, g_rUs, g_tminUs);
  delay(2500);

  S.lo = 0;
  S.hi = T_INF;
  S.R = T_INF;
  S.patIdx = 0;
  patFixed(0, &S.pat);
  S.run = 0;
  S.npat = 0;
  S.flags = 0;
  V.lastErr = 0xFFFF;
  nextCandidate();
}

void loop() {
  patPrepare(&S.pat);
  patName(&S.pat, g_patName);
  fillView(st_run, 0);

  uint16_t fr = 0, fc = 0, late = 0;
  uint16_t errs = runOnce(&S.pat, S.t, &fr, &fc, &late);
  V.lastErr = errs;
  V.lastRow = fr;
  V.lastCol = fc;
  V.lastLateUs = late;
  afterRun(errs);
}
