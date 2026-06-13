// 16Pin.cpp - Implementation of 16-Pin DRAM testing functions
//=======================================================================================
//
// This file contains all test logic for 16-pin DRAM packages. It implements:
// - Optimized split lookup tables for fast address setting (saves ~1.4KB flash)
// - Checkerboard coupling test (passes 0-1, up/down) + pseudo-random patterns (4-5)
// - 4532 half-functional chip detection and handling
// - Page mode read/write operations with minimal timing overhead
//
// Supported chips:
// - 4164 (64Kx1, 256 rows x 256 cols, 2ms refresh)
// - 41256 (256Kx1, 512 rows x 512 cols, 4ms refresh)
// - 41257 (256Kx1 Nibble Mode, 512 rows x 512 cols, 4ms refresh)
// - 4816 (16Kx1, 128 rows x 128 cols, 2ms refresh, no -5V/+12V)
// - TMS4532 (32Kx1, RAS-split half-good 4164, 128 rows x 256 cols, 4ms refresh)
// - MSM3732 (32Kx1, CAS-split half-good 4164, 256 rows x 256 cols, 4ms refresh, -L/-H)
//
//=======================================================================================

#include "16Pin.h"

//=======================================================================================
// 16-PIN PORT MAPPINGS
//=======================================================================================

// Port bit to physical pin mappings for ground short detection
// Format: [bit0, bit1, ..., bit7] where EOL=End of List, NC=Not Connected
const uint8_t CPU_16PORTB[] = { 13, 4, 12, 3, EOL, EOL, EOL, EOL };  // PB0-PB3 used
const uint8_t CPU_16PORTC[] = { 1, 2, 14, 15, 5, EOL, EOL, EOL };    // PC0-PC4 used
const uint8_t CPU_16PORTD[] = { 6, 7, 8, NC, NC, NC, 9, 10 };        // PD0-PD2, PD6-PD7 used

// Control signal pin assignments
const uint8_t RAS_16PIN = 9;   // Row Address Strobe on Digital Pin 9 (PB1)
const uint8_t CAS_16PIN = 17;  // Column Address Strobe on Analog Pin 3 / Digital Pin 17 (PC3)


//=======================================================================================
// 16-PIN SPLIT LOOKUP TABLE GENERATION (Optimized)
// Flash savings: ~1.4 KB compared to full 512-entry table
//=======================================================================================

// Bit extraction macros - convert address bits to port configurations
// These map the 9-bit address (A0-A8) to the scattered port pin assignments
#define GET_PB(a) ((((a)&0x0010)) | (((a)&0x0008) >> 1) | (((a)&0x0040) >> 6))                      // Extract A4, A3, A6
#define GET_PC(a) ((((a)&0x0001) << 4) | (((a)&0x0100) >> 8))                                       // Extract A0, A8
#define GET_PD(a) ((((a)&0x0080) >> 1) | (((a)&0x0020) << 2) | (((a)&0x0004) >> 2) | ((a)&0x0002))  // Extract A7, A5, A2, A1

// Helper macros for generating 8-entry rows in lookup tables
#define ROW8_PB(b) GET_PB(b + 0), GET_PB(b + 1), GET_PB(b + 2), GET_PB(b + 3), GET_PB(b + 4), GET_PB(b + 5), GET_PB(b + 6), GET_PB(b + 7)
#define ROW8_PC(b) GET_PC(b + 0), GET_PC(b + 1), GET_PC(b + 2), GET_PC(b + 3), GET_PC(b + 4), GET_PC(b + 5), GET_PC(b + 6), GET_PC(b + 7)
#define ROW8_PD(b) GET_PD(b + 0), GET_PD(b + 1), GET_PD(b + 2), GET_PD(b + 3), GET_PD(b + 4), GET_PD(b + 5), GET_PD(b + 6), GET_PD(b + 7)

// Helper macros for generating 32-entry rows (4x8)
#define ROW32_PB(b) ROW8_PB(b), ROW8_PB(b + 8), ROW8_PB(b + 16), ROW8_PB(b + 24)
#define ROW32_PC(b) ROW8_PC(b), ROW8_PC(b + 8), ROW8_PC(b + 16), ROW8_PC(b + 24)
#define ROW32_PD(b) ROW8_PD(b), ROW8_PD(b + 8), ROW8_PD(b + 16), ROW8_PD(b + 24)

// LOW (inner / per-column) LUT: 32 entries for address bits A0-A4. INTERLEAVED into one
// struct array in RAM so the three scattered port bytes of a column share ONE address
// computation (&lut_16_low[c]) + ldd Z+0/+1/+2, instead of recomputing base+c three times
// (the separate-array form forced movw+subi+sbci per read). 96 B RAM (same as the former
// 3x 32 B); not PROGMEM (lpm forces Z and cannot use the +offset form). The high LUTs stay
// PROGMEM (read only once per 32-col chunk).
struct LutLow16 { uint8_t pb, pc, pd; };
#define LUT16_ENTRY(i) { (uint8_t)GET_PB(i), (uint8_t)GET_PC(i), (uint8_t)GET_PD(i) }
#define ROW8_LUT16(b)  LUT16_ENTRY(b + 0), LUT16_ENTRY(b + 1), LUT16_ENTRY(b + 2), LUT16_ENTRY(b + 3), \
                       LUT16_ENTRY(b + 4), LUT16_ENTRY(b + 5), LUT16_ENTRY(b + 6), LUT16_ENTRY(b + 7)
#define ROW32_LUT16(b) ROW8_LUT16(b), ROW8_LUT16(b + 8), ROW8_LUT16(b + 16), ROW8_LUT16(b + 24)
const struct LutLow16 lut_16_low[32] = { ROW32_LUT16(0) };

// HIGH TABLE: 16 entries for address bits A5-A8 (steps of 32)
// Used for outer loop in page mode access
const uint8_t lut_16_high_b[16] PROGMEM = {
  GET_PB(0), GET_PB(32), GET_PB(64), GET_PB(96), GET_PB(128), GET_PB(160), GET_PB(192), GET_PB(224),
  GET_PB(256), GET_PB(288), GET_PB(320), GET_PB(352), GET_PB(384), GET_PB(416), GET_PB(448), GET_PB(480)
};
const uint8_t lut_16_high_c[16] PROGMEM = {
  GET_PC(0), GET_PC(32), GET_PC(64), GET_PC(96), GET_PC(128), GET_PC(160), GET_PC(192), GET_PC(224),
  GET_PC(256), GET_PC(288), GET_PC(320), GET_PC(352), GET_PC(384), GET_PC(416), GET_PC(448), GET_PC(480)
};
const uint8_t lut_16_high_d[16] PROGMEM = {
  GET_PD(0), GET_PD(32), GET_PD(64), GET_PD(96), GET_PD(128), GET_PD(160), GET_PD(192), GET_PD(224),
  GET_PD(256), GET_PD(288), GET_PD(320), GET_PD(352), GET_PD(384), GET_PD(416), GET_PD(448), GET_PD(480)
};

//=======================================================================================
// CORE HELPER FUNCTIONS (OPTIMIZED)
//=======================================================================================

/**
 * Set address for random access using split lookup tables
 *
 * This function sets a 9-bit address on the multiplexed address bus by looking up
 * pre-calculated port values from split tables. It's faster than bit manipulation
 * and preserves control signal pins (RAS, CAS, WE) and data input pin (DIN).
 *
 * @param a Address to set (0-511 for 41256, 0-255 for 4164)
 */
static inline void setAddr16_Random(uint16_t a) {
  uint8_t idx_low = a & 0x1F;  // Lower 5 bits (0-31) for inner loop
  uint8_t idx_high = a >> 5;   // Upper 4 bits for outer loop

  // Fetch low address bits — one struct -> one address calc + ldd Z+0/+1/+2
  const struct LutLow16 *ll = &lut_16_low[idx_low];
  uint8_t lb = ll->pb;
  uint8_t lc = ll->pc;
  uint8_t ld = ll->pd;

  // Fetch high address bits from tables
  uint8_t hb = pgm_read_byte(&lut_16_high_b[idx_high]);
  uint8_t hc = pgm_read_byte(&lut_16_high_c[idx_high]);
  uint8_t hd = pgm_read_byte(&lut_16_high_d[idx_high]);

  // Apply to ports while preserving DIN (PC1) and control pins
  PORTB = (PORTB & 0xEA) | lb | hb;  // Preserve PB1(RAS), PB3(WE), PB5(LED)
  PORTC = (PORTC & 0xEE) | lc | hc;  // Preserve PC1(DIN), PC3(CAS)
  PORTD = (PORTD & 0x3C) | ld | hd;  // Preserve PD2-PD5
}

// Macro for high-speed inner loop address setting
// Expects base_b, base_c, base_d to be defined in local scope
// This eliminates the need to recalculate high bits on every column
#define SET_ADDR_16_LOW(idx) \
  PORTB = base_b | lut_16_low[idx].pb; \
  PORTC = base_c | lut_16_low[idx].pc; \
  PORTD = base_d | lut_16_low[idx].pd

// Macro for setting data input pin (PC1)
#define SET_DIN_16(d) \
  if (d & 0x04) PORTC |= 0x02; \
  else PORTC &= ~0x02;

// Wrapper function for compatibility with address checking code
// Combines address setting and data input in one call
static inline void setAddrData(uint16_t addr, uint8_t dataVal) {
  setAddr16_Random(addr);
  SET_DIN_16(dataVal);
}

// Redefine header macro to use optimized version
#undef SET_ADDR_PIN16
#define SET_ADDR_PIN16(row) setAddr16_Random(row)


//=======================================================================================
// MAIN TEST FUNCTION
//=======================================================================================

// Forward declarations
static void run_16Pin_tests();
static void detect41257_16Pin();        // Nibble mode detection after 41256 sensing
static void casBeforeRasRefresh_16Pin();  // CBR ("hidden") refresh (defined below)
static void fastPatternTest_16Pin(uint8_t passNr, bool runCBR);  // 41256 checkerboard (FPM)
static void fastPatternNibble_16Pin(uint8_t passNr, bool runCBR);  // 41257 checkerboard (nibble)

// Quadrant error tracking for TMS4532/MSM3732 detection
// Bits 0-3 = Q1-Q4: qbit = ((row>=128)?2:0) | ((col>=128)?1:0)
static uint8_t quadrantErrors;              // Accumulated quadrant error bitmap
static uint8_t quadrantErrorsAfterChecker;  // Snapshot after checkerboard passes (before retention)
// Baseline quadrant pattern captured at the end of the first do-while iteration
// (loop / stress-test guard). 0xFF acts as a "not yet captured" sentinel — any
// real quadrant-errors value is at most 0x0F so 0xFF cannot collide with one.
// Any quadrant bit lit up in a later iteration that was NOT in this baseline
// means a previously-good quadrant has newly failed → stress test aborts.
static uint8_t loopQuadrantSnapshot = 0xFF;
static uint8_t retryActive;  // 0=normal, 1=retry armed (T_4164 4ms), 2=failure intercepted

void test_16Pin() {
  // Configure I/O for 16-pin DRAM interface
  // DDRB: Set PB0-PB5 as outputs (A3, A4, A6, WE, RAS, LED)
  // PORTB: Initialize with RAS=HIGH, WE=HIGH, others LOW
  DDRB = 0b00111111;
  PORTB = 0b00101010;

  // DDRC: Set PC0-PC4 as outputs, PC2 as input (data out from DRAM)
  // PORTC: Initialize with CAS=HIGH
  DDRC = 0b00011011;
  PORTC = 0b00001000;

  // DDRD: Set PD0-PD2, PD6-PD7 as outputs (address lines)
  DDRD = 0b11000011;
  PORTD = 0x00;

  // Verify RAM is present in socket
  if (!ram_present_16Pin())
    error(0, 0);  // No RAM detected - trigger error display

  // Determine chip type by testing address lines A7 and A8
  // Detects: T_41256, T_4164, T_4816 (4532/3732 detected via quadrant errors)
  sense41256_16Pin();

  // If 41256 detected, check for nibble mode (41257)
  if (type == T_41256) {
    detect41257_16Pin();
  }

  // Display chip type on OLED (4164 shown initially; half-good identified after tests)
  // Runtime gated: only show "or 32Kx1" hint when 32K detection is enabled
  if (CFG_32K_ACTIVE && type == T_4164)
    writeRAMType((const __FlashStringHelper *)F("4164 or 32Kx1"));
  else
    writeRAMType((const __FlashStringHelper *)ramTypes[type].name);

  // Restore I/O configuration after chip sensing
  DDRB = 0b00111111;
  PORTB = 0b00101010;

  // Verify all address lines decode correctly
  checkAddressing_16Pin();

  // ===== TEST LOOP (single pass or stress loop, depending on CFG_LOOP_ACTIVE) =====
  // For half-good chips (4532/3732), quadrant tracking is re-evaluated every
  // iteration; a stable half-good chip reports the same quadrant pattern each
  // pass. run_16Pin_tests() resets quadrantErrors and retryActive internally.
  do {
    // Run comprehensive pattern tests
    run_16Pin_tests();

    if (CFG_32K_ACTIVE && (type == T_4164 || type == T_4164_2MS)) {
      // ===== QUADRANT EVALUATION (T_4164/T_4164_2MS) =====
      // qbit = ((row>=128)?2:0) | ((col>=128)?1:0)  →  Q1=bit0, Q2=bit1, Q3=bit2, Q4=bit3
      //
      // Two-quadrant (definitive):
      //   0x03 = Q1+Q2 (row-half A7=0) → TMS4532-4
      //   0x0C = Q3+Q4 (row-half A7=1) → TMS4532-3
      //   0x05 = Q1+Q3 (col-half A7=0) → MSM3732-H
      //   0x0A = Q2+Q4 (col-half A7=1) → MSM3732-L
      //
      // Single-quadrant (ambiguous — can't distinguish RAS-split from CAS-split):
      //   0x01 = Q1 only → 4532-4 or 3732-H
      //   0x02 = Q2 only → 4532-4 or 3732-L
      //   0x04 = Q3 only → 4532-3 or 3732-H
      //   0x08 = Q4 only → 4532-3 or 3732-L
      //
      // Diagonal or 3+: defective
      if (loopQuadrantSnapshot == 0xFF) {
        // ----- FIRST ITERATION: classify + capture baseline -----
        // Runs whether the chip is half-good or a plain 4164 (baseline=0).
        if (quadrantErrors) {
          const char *qs;
          switch (quadrantErrors) {
            case 0x03: qs = qs_4532_4; break;  // Definitive TMS4532-4
            case 0x0C: qs = qs_4532_3; break;  // Definitive TMS4532-3
            case 0x05: qs = qs_3732_H; break;  // Definitive MSM3732-H
            case 0x0A: qs = qs_3732_L; break;  // Definitive MSM3732-L
            case 0x01: qs = qs_Q1; break;      // Ambiguous Q1
            case 0x02: qs = qs_Q2; break;      // Ambiguous Q2
            case 0x04: qs = qs_Q3; break;      // Ambiguous Q3
            case 0x08: qs = qs_Q4; break;      // Ambiguous Q4
            default: error(0, 2);              // Diagonal/3+ → defective
          }
          typeSuffix = (const __FlashStringHelper *)qs;

          // Set LED blink code: 1=4532 (1G-5O), 2=3732 (1G-6O), 3=ambig (1G-5O+1G-6O)
          halfGoodBlink = (quadrantErrors & (quadrantErrors - 1))
                            ? ((quadrantErrors == 0x03 || quadrantErrors == 0x0C) ? 1 : 2)
                            : 3;
        }
        // Capture the baseline regardless of value — 0 for a plain 4164 means
        // no quadrant is allowed to fail in subsequent stress iterations.
        loopQuadrantSnapshot = quadrantErrors;
      } else {
        // ----- SUBSEQUENT ITERATION: stress-test guard -----
        // Any bit set in quadrantErrors that was NOT in the iter-1 baseline
        // is a previously-good quadrant that just failed → abort.
        // Covers all cases:
        //   plain 4164 (baseline=0) → ANY new quadrant failure aborts
        //   4532-4   (baseline=0x03) → Q3 or Q4 newly failing aborts
        //   ambig Q1 (baseline=0x01) → Q2/Q3/Q4 newly failing aborts
        // Same-or-subset is tolerated (a once-failing quadrant passing this
        // run is normal stress-test jitter, not a degradation).
        if (quadrantErrors & ~loopQuadrantSnapshot) {
          error(0, 2);  // Previously-good quadrant just failed → defective
        }
      }
    }

    // CBR refresh-counter test AFTER the full regular test (run_16Pin_tests above did the
    // checkerboard + retention), loop mode only, every 10th run, 41256/41257 only.
    // fastPatternTest_16Pin(0,true): re-write + ~60 s CBR (own "CBR:<sec>" screen + LED off)
    // + verify -> the success screen is not shown prematurely.
    if (CFG_LOOP_ACTIVE && (s_runCount % 10 == 1)) {
      if (type == T_41257) {
        cbrScreenPrep();  // render BEFORE the write phase (25-60 ms zero-refresh)
        fastPatternNibble_16Pin(0, true);
      } else if (type == T_41256) {
        cbrScreenPrep();
        fastPatternTest_16Pin(0, true);
      }
    }

    // All tests passed.
    // In loop mode testOK() updates the OLED (RUN counter) and returns so the
    // next stress iteration can start. In single-pass mode it never returns.
    testOK();
  } while (CFG_LOOP_ACTIVE);
}

/**
 * Checkerboard coupling test (passes 0-1) using port-ordered column iteration
 *
 * Instead of translating logical addresses through lookup tables (3 port writes
 * + 3 PROGMEM reads per CAS cycle), this function iterates column addresses in
 * "port-natural order" — nesting loops by port register so the innermost loop
 * needs only ONE port write per CAS cycle.
 *
 * Each row is written then IMMEDIATELY read back within the same outer loop
 * iteration. This ensures we stay well within the DRAM retention time window.
 *
 * Loop nesting (innermost first):
 *   PORTD: 16 combinations (A1, A2, A5, A7)  → 1 port write per CAS
 *   PORTB:  8 combinations (A3, A4, A6)       → 1 port write per 16 CAS
 *   PORTC:  2-4 combinations (A0, [A8])       → 1 port write per 128 CAS
 *
 * Total: 16 × 8 × 2 = 256 (4164) or 16 × 8 × 4 = 512 (41256/41257)
 *
 * Inner loop cost: ~10 cycles/col vs ~39 cycles/col with LUT → ~4× speedup
 *
 * Data: CHECKERBOARD background  bit = ((row ^ col) & 1) ^ passNr.
 *   Adjacent cells (row±1 or col±1) hold opposite values → inter-cell / row /
 *   column coupling coverage (much stronger than the former solid 0/F and
 *   A0-only patterns). The column LSB A0 == (pc_i & 1), so the written bit
 *   depends only on (row & 1) ^ (pc_i & 1) ^ passNr and is folded into the
 *   per-row PORTC value with zero inner-loop overhead.
 *
 * Two passes (called from run_16Pin_tests) give full SAF coverage and both
 * vertical aggressor/victim orderings:
 *   passNr 0: ascending  row scan, normal background.
 *   passNr 1: descending row scan, inverted background.
 *
 * @param passNr Pass number (0 = ascending/normal, 1 = descending/inverted)
 */

/**
 * RAS recycle WITH a CAS-before-RAS ("hidden") refresh — used only by the 5.0.3
 * write-all -> read-all checkerboard. It precharges the current row (tRAS reset +
 * refresh), issues ONE CBR refresh (the chip's internal counter sweeps all rows,
 * keeping every row alive across the long write/read sweeps), then re-opens the
 * current row to continue the page.
 *
 * Entry expects CAS HIGH (callers raise CAS first). WE is left untouched: the
 * write phase raises WE before calling and lowers it after (clean refresh, no
 * spurious write); the read phase keeps WE high throughout.
 *
 * Cadence: one CBR per ~9.5 µs burst recycle sweeps all rows in ~2.5 ms, well
 * inside the 4 ms retention window. NOTE: relies on plain CBR (no WCBR test mode
 * on vintage 4164/41256). HW-verify on a logic analyser.
 */
static void __attribute__((noinline)) rasHandlingHidden_16Pin(uint16_t row) {
  // Entry: CAS LOW. True hidden refresh — toggle RAS while CAS stays low so the
  // CBR runs with NO extra CAS edges (and a read's DOUT would stay valid) — then
  // re-open the current row. The WRITE path enters here with CAS already low from
  // the burst (= real hidden refresh). The READ path samples DOUT after CAS rises
  // (settling fix), so it lowers CAS itself (CAS_LOW16) before calling — there it
  // is an ordinary CBR. Either way one counter row is refreshed per call.
  RAS_HIGH16;             // precharge the active row (refresh R); CAS stays LOW
  NOP;                    // tRP
  RAS_LOW16;              // RAS down with CAS low -> CBR (hidden) refresh
  NOP;
  NOP;
  RAS_HIGH16;             // end the refresh
  CAS_HIGH16;             // finish the cycle (raise CAS)
  setAddr16_Random(row);  // re-address the current row (CAS/RAS/WE preserved)
  RAS_LOW16;              // re-open it for the next burst
}

/**
 * Checkerboard passes (0-1) for the QUADRANT / small group:
 *   4164 / 4164_2MS / 4532 / 3732 (quadrant-tracked) and 4816 (no quadrant).
 *
 * Inter-row coupling test via a 2-row write/read LAG pipeline: write row N, then read
 * row N-LAG (LAG=2). When a victim row is read, BOTH of its vertical neighbours have
 * already been written, so inter-row coupling is exercised — yet at most LAG rows are
 * ever "pending" (written-but-not-read), so each row's write->read window stays well
 * under the 2/4 ms refresh period and no extra refresh is needed. (The 41256/41257 use
 * CBR / hidden refresh in fastPatternTest_16Pin; these older types predate it.) tRAS
 * (NMOS max 10 us, no page-mode allowance) is bounded by 2-cell bursts with the inline
 * row-snapshot recycle (5.0.5 — the former 8-cell bursts measured up to 19 us).
 *
 * Two passes give both directions: pass 0 ascending, pass 1 descending (background
 * inverted). Quadrant tracking (TMS4532/MSM3732 detection) is only valid for the full
 * 4164 geometry; other types fail immediately on the first mismatch.
 */
static void __attribute__((hot)) checkerQuadrant_16Pin(uint8_t passNr) {
  uint16_t total_rows = ramTypes[type].rows;
  uint8_t num_pc = (ramTypes[type].columns > 256) ? 4 : 2;
  bool track_quadrants = CFG_32K_ACTIVE && (type == T_4164 || type == T_4164_2MS);

  uint8_t inv = passNr & 1;        // background inversion for pass 1
  bool ascending = (passNr == 0);  // pass 0 up, pass 1 down
  const uint8_t LAG = 2;           // read trails write by 2 rows (both neighbours set)

  // --- Build PORTD LUT: 16 entries for A2(PD0), A1(PD1), A7(PD6), A5(PD7) ---
  uint8_t pd_base = PORTD & 0x3C;  // Preserve PD2-PD5 (inputs)
  uint8_t pd_lut[16];
  for (uint8_t i = 0; i < 16; i++)
    pd_lut[i] = pd_base | (i & 0x03) | ((i & 0x0C) << 4);

  // --- PORTB address parts: 8 entries for A6(PB0), A3(PB2), A4(PB4) (compile-time const) ---
  uint8_t pb_addr[8] = { 0, 1, 4, 5, 16, 17, 20, 21 };

  // --- PORTC read LUT (A0=PC4, A8=PC0, CAS=PC3=HIGH); also the write base ---
  uint8_t pc_r[4];
  pc_r[0] = 0x08;
  pc_r[1] = 0x08 | 0x10;
  pc_r[2] = 0x08 | 0x01;
  pc_r[3] = 0x08 | 0x11;

  CAS_HIGH16;
  cli();
  for (uint16_t i = 0; i < total_rows + LAG; i++) {
    // ---------- WRITE row_w (write index i, while rows remain) ----------
    if (i < total_rows) {
      uint16_t row_w = ascending ? i : (uint16_t)(total_rows - 1 - i);
      uint8_t rowbit = ((uint8_t)row_w & 1) ^ inv;
      uint8_t din0 = rowbit ? 0x02 : 0x00;
      uint8_t din1 = din0 ^ 0x02;
      uint8_t pc_w[4];
      pc_w[0] = pc_r[0] | din0;
      pc_w[1] = pc_r[1] | din1;
      pc_w[2] = pc_r[2] | din0;
      pc_w[3] = pc_r[3] | din1;

      DDRC |= 0x02;  // DIN as OUTPUT
      rasHandling_16Pin(row_w);
      WE_LOW16;
      // Row-port snapshot for the inline tRAS recycle (same scheme as writeRow_16Pin,
      // HW-validated there): NMOS tRAS max = 10 us, the former 8-cell bursts measured
      // up to 19 us. 2-cell bursts + cheap snapshot recycle -> window ~2 us. row_b
      // (RAS LOW) feeds pb_base for the cells; row_b_hi (RAS HIGH) is the restore.
      uint8_t row_b = PORTB, row_c = PORTC, row_d = PORTD;
      uint8_t row_b_hi = row_b | 0x02;
      uint8_t pb_base = row_b & 0xEA;  // loop-invariant (recycle restores the snapshot)
      for (uint8_t pc_i = 0; pc_i < num_pc; pc_i++) {
        uint8_t pcw = pc_w[pc_i];
        for (uint8_t pb_i = 0; pb_i < 8; pb_i++) {
          uint8_t pbv = pb_base | pb_addr[pb_i];
          for (uint8_t q = 0; q < 16; q += 2) {  // 2-cell bursts through pd_lut
            PORTB = pbv;  // re-assert column-phase ports after the previous recycle
            PORTC = pcw;
            for (uint8_t k = 0; k < 2; k++) {
              CAS_HIGH16;
              PORTD = pd_lut[q | k];
              CAS_LOW16;
            }
            CAS_HIGH16;        // end the write strobe
            RAS_HIGH16;        // inline row-snapshot recycle (RAS-only, no CBR)
            PORTB = row_b_hi;  // restore with RAS kept HIGH (~375 ns precharge)
            PORTC = row_c;
            PORTD = row_d;
            RAS_LOW16;         // real falling edge: full row address on all ports
          }
        }
      }
      WE_HIGH16;
      RAS_HIGH16;
    }

    // ---------- READ row_r (trails write by LAG: index i-LAG) ----------
    if (i >= LAG) {
      uint16_t jr = i - LAG;
      uint16_t row_r = ascending ? jr : (uint16_t)(total_rows - 1 - jr);
      uint8_t rowbit = ((uint8_t)row_r & 1) ^ inv;
      uint8_t exp0 = rowbit ? 0x04 : 0x00;
      uint8_t exp1 = exp0 ^ 0x04;
      uint8_t exp_a0[4];
      exp_a0[0] = exp_a0[2] = exp0;
      exp_a0[1] = exp_a0[3] = exp1;

      PORTC &= ~0x02;
      DDRC &= ~0x02;  // DIN as INPUT (shared Din/Dout chips)
      rasHandling_16Pin(row_r);
      // Row-port snapshot recycle, 2-cell bursts (see write phase above; WE stays high).
      uint8_t row_b = PORTB, row_c = PORTC, row_d = PORTD;
      uint8_t row_b_hi = row_b | 0x02;
      uint8_t pb_base = row_b & 0xEA;  // loop-invariant (recycle restores the snapshot)
      for (uint8_t pc_i = 0; pc_i < num_pc; pc_i++) {
        uint8_t pcr = pc_r[pc_i];
        uint8_t exp = exp_a0[pc_i];
        for (uint8_t pb_i = 0; pb_i < 8; pb_i++) {
          uint8_t pbv = pb_base | pb_addr[pb_i];
          for (uint8_t q = 0; q < 16; q += 2) {  // 2-cell bursts through pd_lut
            PORTB = pbv;  // re-assert column-phase ports after the previous recycle
            PORTC = pcr;
            for (uint8_t k = 0; k < 2; k++) {
              PORTD = pd_lut[q | k];
              CAS_LOW16;
              NOP;  // tCAC
              CAS_HIGH16;
              if ((PINC ^ exp) & 0x04) {
                if (track_quadrants) {
                  uint8_t qbit = ((row_r >= 128) ? 2 : 0) | ((PORTD & 0x40) ? 1 : 0);
                  quadrantErrors |= (1 << qbit);
                  if (quadrantErrors != 0x0F)
                    continue;  // record the quadrant, keep scanning (k-loop semantics)
                }
                sei();
                RAS_HIGH16;
                DDRC |= 0x02;
                error(passNr, 2);
                return;
              }
            }
            RAS_HIGH16;        // inline row-snapshot recycle (CAS already high)
            PORTB = row_b_hi;  // restore with RAS kept HIGH (~375 ns precharge)
            PORTC = row_c;
            PORTD = row_d;
            RAS_LOW16;         // real falling edge: full row address on all ports
          }
        }
      }
      RAS_HIGH16;
    }
  }
  sei();
  DDRC |= 0x02;
}

// Checkerboard passes (0-1) for the 41256 ONLY (large page-mode type; dispatched
// from run_16Pin_tests). Write-all -> read-all so inter-row coupling is caught;
// each 4-CAS burst is followed by a RAS phase — a true HIDDEN refresh on writes
// (CAS stays low) and a CBR on reads (DOUT sampled after CAS rises). That counter
// sweep keeps the long sweeps alive and, at the tuned cadence, doubles as the
// retention check. No quadrant logic (41256 is not a half-good type — 4164/4532/
// 3732 use checkerQuadrant_16Pin).
static void __attribute__((hot)) fastPatternTest_16Pin(uint8_t passNr, bool runCBR) {
  uint16_t total_rows = ramTypes[type].rows;
  uint8_t num_pc = (ramTypes[type].columns > 256) ? 4 : 2;

  uint8_t inv = passNr & 1;        // background inversion for pass 1
  bool ascending = (passNr == 0);  // pass 0 up, pass 1 down

  // --- Build PORTD LUT: 16 entries for A2(PD0), A1(PD1), A7(PD6), A5(PD7) ---
  uint8_t pd_base = PORTD & 0x3C;  // Preserve PD2-PD5 (inputs)
  uint8_t pd_lut[16];
  for (uint8_t i = 0; i < 16; i++) {
    // i bits 0,1 → PD0,PD1 (A2,A1);  i bits 2,3 → PD6,PD7 (A7,A5)
    pd_lut[i] = pd_base | (i & 0x03) | ((i & 0x0C) << 4);
  }

  // --- PORTB address parts: 8 entries for A6(PB0), A3(PB2), A4(PB4) (compile-time const) ---
  uint8_t pb_addr[8] = { 0, 1, 4, 5, 16, 17, 20, 21 };

  // --- PORTC read LUT (A0=PC4, A8=PC0, CAS=PC3=HIGH); also the write base.
  // DIN (PC1) is OR-ed in per row to form pc_w (checkerboard, see loop). ---
  uint8_t pc_r[4];
  pc_r[0] = 0x08;         // A8=0 A0=0
  pc_r[1] = 0x08 | 0x10;  // A8=0 A0=1
  pc_r[2] = 0x08 | 0x01;  // A8=1 A0=0
  pc_r[3] = 0x08 | 0x11;  // A8=1 A0=1

  uint8_t pc_w[4];    // write PORTC values (pc_r | DIN), rebuilt per row
  uint8_t exp_a0[4];  // expected DOUT (PC2 = 0x04) per A0 state, per row

  // ============= WRITE-ALL ROWS, then READ-ALL ROWS (5.0.3) =============
  // All rows are written first, then all read back, so a fault where writing
  // one row disturbs another (inter-row coupling) is caught — the per-row
  // write-then-read of earlier versions could not see this. To keep every row
  // alive across the two long sweeps, the per-burst RAS recycle also issues a
  // refresh (rasHandlingHidden_16Pin) whose internal counter sweeps all rows well
  // inside the retention window — hidden on writes (CAS stays low), CBR on reads.

  CAS_HIGH16;
  cli();

  // ---------------------- WRITE ALL ROWS ----------------------
  DDRC |= 0x02;  // DIN as OUTPUT
  for (uint16_t i = 0; i < total_rows; i++) {
    uint16_t row = ascending ? i : (uint16_t)(total_rows - 1 - i);

    // Per-row checkerboard data: bit = (row & 1) ^ A0 ^ inv
    uint8_t rowbit = ((uint8_t)row & 1) ^ inv;
    uint8_t din0 = rowbit ? 0x02 : 0x00;  // DIN (PC1) for A0=0 columns
    uint8_t din1 = din0 ^ 0x02;           // complement for A0=1 columns
    pc_w[0] = pc_r[0] | din0;             // A8=0 A0=0
    pc_w[1] = pc_r[1] | din1;             // A8=0 A0=1
    pc_w[2] = pc_r[2] | din0;             // A8=1 A0=0
    pc_w[3] = pc_r[3] | din1;             // A8=1 A0=1

    rasHandling_16Pin(row);
    WE_LOW16;
    uint8_t pb_base = PORTB & 0xEA;

    for (uint8_t pc_i = 0; pc_i < num_pc; pc_i++) {
      PORTC = pc_w[pc_i];
      for (uint8_t pb_i = 0; pb_i < 8; pb_i++) {
        // 4-CAS bursts, each followed by a true hidden refresh (CAS stays low).
        for (uint8_t q = 0; q < 16; q += 4) {
          PORTB = pb_base | pb_addr[pb_i];
          const uint8_t *pd = &pd_lut[q];  // hoist base: the k-loop then reads via ld Z+
          for (uint8_t k = 0; k < 4; k++) {  // (was ~8 cyc/iter recomputing &pd_lut[q|k])
            CAS_HIGH16;
            PORTD = *pd++;
            CAS_LOW16;
          }
          // CAS is LOW here -> true hidden refresh (no extra CAS edge)
          WE_HIGH16;                  // WE high so the CBR cannot write
          rasHandlingHidden_16Pin(row);
          WE_LOW16;                   // resume write
          pb_base = PORTB & 0xEA;
          PORTC = pc_w[pc_i];
        }
      }
    }
    WE_HIGH16;
    RAS_HIGH16;
  }

  // CBR refresh-COUNTER test (~60 s, loop mode): refresh the just-written array via
  // CAS-before-RAS only, then the read-all below verifies it survived (tests the on-chip
  // refresh counter). Shared helper: millis-bounded 60 s + LED off + "R:<sec>" countdown.
  if (runCBR) cbrRefreshPhase(&casBeforeRasRefresh_16Pin);

  // ---------------------- READ ALL ROWS ----------------------
  PORTC &= ~0x02;
  DDRC &= ~0x02;  // DIN as INPUT (shared Din/Dout chips)
  for (uint16_t i = 0; i < total_rows; i++) {
    uint16_t row = ascending ? i : (uint16_t)(total_rows - 1 - i);

    uint8_t rowbit = ((uint8_t)row & 1) ^ inv;
    uint8_t exp0 = rowbit ? 0x04 : 0x00;  // expected DOUT (PC2) for A0=0
    uint8_t exp1 = exp0 ^ 0x04;
    exp_a0[0] = exp_a0[2] = exp0;
    exp_a0[1] = exp_a0[3] = exp1;

    rasHandling_16Pin(row);
    uint8_t pb_base = PORTB & 0xEA;

    for (uint8_t pc_i = 0; pc_i < num_pc; pc_i++) {
      PORTC = pc_r[pc_i];
      uint8_t exp = exp_a0[pc_i];
      for (uint8_t pb_i = 0; pb_i < 8; pb_i++) {
        // 4-CAS read bursts; CBR refresh between them (read ends CAS-high).
        for (uint8_t q = 0; q < 16; q += 4) {
          PORTB = pb_base | pb_addr[pb_i];
          const uint8_t *pd = &pd_lut[q];  // hoist base (read via ld Z+, see write phase)
          for (uint8_t k = 0; k < 4; k++) {
            PORTD = *pd++;
            CAS_LOW16;
            NOP;  // tCAC: CAS access time
            CAS_HIGH16;
            if ((PINC ^ exp) & 0x04) {
              sei();
              RAS_HIGH16;
              DDRC |= 0x02;
              error(passNr, runCBR ? 5 : 2);  // 5 = "CBR Timer fault" (CBR run); else checkerboard
              return;
            }
          }
          // read burst ends CAS-high -> lower CAS, then CBR via the shared helper
          CAS_LOW16;
          rasHandlingHidden_16Pin(row);
          pb_base = PORTB & 0xEA;
          PORTC = pc_r[pc_i];
        }
      }
    }
    RAS_HIGH16;
  }
  sei();
  DDRC |= 0x02;
}

// Checkerboard passes (0-1) for the 41257 (NIBBLE mode). The FPM path above leaves
// colA1/colA2 = 0 for the 41257 because its nibble counter ignores the external column
// after the 1st CAS (it cycles {rowA8, colA8}); so that path only reaches 1/4 of the
// columns. This nibble-aware path instead sweeps EVERY nibble base (rowA0-7 x colA0-7
// via setAddr16_Random) and lets the 4 CAS toggles cover the {rowA8, colA8} nibble ->
// full cell coverage. The 4 nibble cells differ only in A8, so (row^col)&1 (the LSB
// parity) is identical for all 4 -> the same DIN is written to the whole nibble (that is
// correct; intra-nibble coupling, which a checkerboard cannot express here, is covered by
// the random/retention path 4-5). write-all -> read-all for inter-row/col coupling; a CBR
// after every base keeps all rows alive across the long sweep (WE high around it -> WCBR
// guard). data/exp bit = PC2 (0x04); DIN via SET_DIN_16 (PC1).
static void __attribute__((hot)) fastPatternNibble_16Pin(uint8_t passNr, bool runCBR) {
  uint8_t inv = passNr & 1;
  bool ascending = (passNr == 0);
  uint16_t nrows = ramTypes[type].rows / 2;      // rowA8 is part of the nibble -> 256
  uint16_t ncols = ramTypes[type].columns / 2;   // colA8 is part of the nibble -> 256

  CAS_HIGH16;
  DDRC |= 0x02;  // DIN as OUTPUT
  cli();
  // ---------------------- WRITE ALL NIBBLE BASES ----------------------
  for (uint16_t r = 0; r < nrows; r++) {
    uint16_t row = ascending ? r : (uint16_t)(nrows - 1 - r);
    for (uint16_t col = 0; col < ncols; col++) {
      uint8_t bit = (uint8_t)((((uint8_t)(row ^ col) & 1) ^ inv) << 2);  // 0x04 or 0x00
      setAddr16_Random(row);
      RAS_LOW16;
      setAddr16_Random(col);
      SET_DIN_16(bit);
      WE_LOW16;
      CAS_LOW16; NOP; CAS_HIGH16;  // nibble bit 0  (rowA8=0,colA8=0)
      CAS_LOW16; NOP; CAS_HIGH16;  // bit 1
      CAS_LOW16; NOP; CAS_HIGH16;  // bit 2
      CAS_LOW16; NOP; CAS_HIGH16;  // bit 3
      WE_HIGH16;                   // WE high BEFORE the CBR (WCBR guard)
      RAS_HIGH16;                  // reset the nibble counter
      casBeforeRasRefresh_16Pin(); // keep all rows alive during the long sweep
    }
  }

  // CBR refresh-COUNTER test (~60 s, loop mode): shared helper (LED off + countdown).
  if (runCBR) cbrRefreshPhase(&casBeforeRasRefresh_16Pin);

  // ---------------------- READ ALL NIBBLE BASES ----------------------
  PORTC &= ~0x02;
  DDRC &= ~0x02;  // DIN as INPUT (WE stays HIGH = read)
  for (uint16_t r = 0; r < nrows; r++) {
    uint16_t row = ascending ? r : (uint16_t)(nrows - 1 - r);
    for (uint16_t col = 0; col < ncols; col++) {
      uint8_t exp = (uint8_t)((((uint8_t)(row ^ col) & 1) ^ inv) << 2);  // 0x04 or 0x00
      setAddr16_Random(row);
      RAS_LOW16;
      setAddr16_Random(col);
      // Sample DOUT AFTER CAS_HIGH16 (tCAC = NOP + CAS_HIGH16 settle, ~3 cyc),
      // identical to the proven FPM read above; sampling between NOP and CAS_HIGH16
      // is ~1 cyc too early -> tCAC violation -> stale data -> false Pattern-0 fail.
      CAS_LOW16; NOP; CAS_HIGH16; uint8_t a = PINC;  // nibble bit 0  (rowA8=0,colA8=0)
      CAS_LOW16; NOP; CAS_HIGH16; uint8_t b = PINC;  // bit 1
      CAS_LOW16; NOP; CAS_HIGH16; uint8_t c = PINC;  // bit 2
      CAS_LOW16; NOP; CAS_HIGH16; uint8_t d = PINC;  // bit 3
      RAS_HIGH16;
      if (((a ^ exp) | (b ^ exp) | (c ^ exp) | (d ^ exp)) & 0x04) {
        sei();
        DDRC |= 0x02;
        error(passNr, runCBR ? 5 : 2);  // 5 = "CBR Timer fault" (CBR run); else checkerboard
        return;
      }
      casBeforeRasRefresh_16Pin();
    }
  }
  sei();
  DDRC |= 0x02;
}

/**
 * Run all test patterns on detected RAM chip
 *
 * Passes 0-1: Checkerboard coupling test, per-row write-then-read (ascending +
 *             descending, normal + inverted background). Replaces the former
 *             solid 0/F + A0-checkerboard constant patterns.
 * Patterns 4-5: Pseudo-random with retention testing (ordered access required)
 *
 * Quadrant error tracking:
 *   For T_4164, errors are tracked by quadrant (row A7 × col A7).
 *   After the checkerboard passes, quadrantErrors bitmap is evaluated by caller
 *   to identify TMS4532/MSM3732 variants.
 *   4ms→2ms retention fallback restores quadrant snapshot from the passes.
 */

static void run_16Pin_tests() {
  uint16_t total_rows = ramTypes[type].rows;

  if (CFG_32K_ACTIVE)
    quadrantErrors = 0;


  // Passes 0-1: checkerboard coupling test (ascending/normal + descending/inverted)
  // Path split:
  //  - 41256: write-all -> read-all FPM path (fastPatternTest_16Pin, hidden CBR).
  //  - 41257: nibble-aware write-all -> read-all (fastPatternNibble_16Pin) — sweeps
  //    every nibble base; the 4 CAS toggles cover the {rowA8, colA8} nibble (the FPM
  //    path would leave colA1/A2 stuck at 0 -> 1/4 coverage on this chip).
  //  - 4164/4164_2MS/4532/3732/4816 use the per-row quadrant path (no CBR).
  for (uint8_t passNr = 0; passNr < 2; passNr++) {
    if (type == T_41257)
      fastPatternNibble_16Pin(passNr, false);  // nibble-aware: full column coverage
    else if (type == T_41256)
      fastPatternTest_16Pin(passNr, false);    // FPM page-mode
    else
      checkerQuadrant_16Pin(passNr);
  }

  if (CFG_32K_ACTIVE)
    // Snapshot after the checkerboard passes (before retention testing)
    quadrantErrorsAfterChecker = quadrantErrors;

  // ---- Patterns 4-5: pseudo-random WITH retention testing (ALL 16-pin types) ----
  // 5.0.3 revert: 41256 + 41257 use the SAME retention pipeline as the 4164 group
  // (writeRow_16Pin writes a whole row — WE stays low throughout — then, delayRows
  // rows later / after the retention delay, reads it back via checkRow_16Pin). The
  // checkerboard passes 0-1 above keep the new coupling path.
  if (ramTypes[type].flags & RAM_FLAG_NIBBLE_MODE) total_rows /= 2;  // A8 = nibble bit
  // Arm retry for T_4164: if patterns 4-5 fail at 4ms, retry with 2ms timing
  if (type == T_4164) retryActive = 1;

  // Patterns 4-5: Pseudo-random with retention testing (needs sequential access)
pat45_start:
  for (uint8_t patNr = 4; patNr <= 5; patNr++) {
    if (patNr == 5) invertRandomTable();
    for (uint16_t row = 0; row < total_rows; row++) {
      writeRow_16Pin(row, ramTypes[type].columns, patNr);
      if (retryActive == 2) goto pat45_failed;
    }
  }
  goto pat45_done;

pat45_failed:
  // 4ms retention failed → retry with 2ms
  retryActive = 0;
  type = T_4164_2MS;
  if (CFG_32K_ACTIVE)
    quadrantErrors = quadrantErrorsAfterChecker;  // Discard 4ms retention errors

  generateRandomTable();
  goto pat45_start;

pat45_done:
  retryActive = 0;
}

//=======================================================================================
// DETECTION & SENSING LOGIC
//=======================================================================================

/**
 * Write a single bit to a specific row/column address
 *
 * @param row Row address to write to
 * @param col Column address to write to
 * @param data Data bit to write (0 or 1)
 */
static inline void write16Pin(uint16_t row, uint16_t col, uint8_t data) {
  DDRC |= 0x02;  // Ensure PC1 (Din) is OUTPUT for shared Din/Dout chips
  SET_ADDR_PIN16(row);
  RAS_LOW16;
  WE_LOW16;
  setAddrData(col, data ? 0x04 : 0);
  CAS_LOW16;
  NOP;
  CAS_HIGH16;
  WE_HIGH16;
  RAS_HIGH16;
}

/**
 * Read a single bit from a specific row/column address
 *
 * NOTE: For chips where Din/Dout are connected (e.g., 41256 in A501),
 * PC1 (Din) must be switched to INPUT before reading to avoid bus contention.
 *
 * @param row Row address to read from
 * @param col Column address to read from
 * @return Data bit read (0 or 1)
 */
static inline uint8_t read16Pin(uint16_t row, uint16_t col) {
  // Set Din to Input - fix for in-circuit testing
  PORTC &= ~0x02;
  DDRC &= ~0x02;
  SET_ADDR_PIN16(row);
  RAS_LOW16;
  setAddrData(col, 0);
  CAS_LOW16;
  NOP;  // Hold time
  NOP;
  uint8_t result = (PINC & 0x04) >> 2;
  CAS_HIGH16;
  RAS_HIGH16;

  return result;
}

/**
 * Check if RAM chip is present in socket
 *
 * Performs basic presence detection by writing and reading test patterns:
 * 1. Writes 0 to address 0
 * 2. Writes 1 to address 1
 * 3. Reads back both addresses and verifies values
 * 4. Re-reads address 0 to ensure data was retained
 *
 * This test confirms:
 * - Data lines are not floating (chip is inserted)
 * - Basic write/read functionality works
 * - Different addresses can hold different values
 *
 * @return true if RAM detected and responding, false if socket empty or chip faulty
 */
bool ram_present_16Pin(void) {
  // Write test pattern: 0 to row 0, 1 to row 1
  // Half-good chips (4532/3732) still respond here — they alias A7 but rows 0/1 are fine
  write16Pin(0, 0, 0);
  write16Pin(1, 0, 1);

  uint8_t r0 = read16Pin(0, 0);  // Should be 0
  uint8_t r1 = read16Pin(1, 0);  // Should be 1

  // for half good RAMs make sure we did not hit a broken col/row/bit so check another location
  write16Pin(4, 32, 0);
  write16Pin(8, 64, 1);

  uint8_t r2 = read16Pin(4, 32);  // Should be 0
  uint8_t r3 = read16Pin(8, 64);  // Should be 1

  return ((r0 == 0 && r1 == 1) || (r2 == 0 && r3 == 1));
}

/**
 * Detect RAM capacity and type for 16-pin DRAMs
 *
 * Detection sequence:
 *   1. Basic write/read at (0,0) or (128,128) to verify chip responds
 *   2. A8 alias test: row 0 vs row 256 → 41256 if distinct, else 4164-class
 *   3. Row A7 alias test: row 0 vs row 128 → if aliased:
 *      - Col A7 test: col 0 vs 128 → aliased = 4816, independent = T_4164
 *   4. Otherwise → T_4164 (TMS4532/MSM3732 detected via quadrant errors)
 *
 * Sets global 'type' to T_4164, T_41256, or T_4816.
 */
void sense41256_16Pin() {
  CAS_HIGH16;

  // ========== STEP 1: BASIC FUNCTIONALITY CHECK ==========
  write16Pin(0, 0, 0);
  if (read16Pin(0, 0) != 0) {
    // Row 0, Col 0 doesn't hold data — try another location
    write16Pin(0, 192, 0);
    if (read16Pin(0, 192) != 0) {
      error(0, 0);  // No functional RAM
    }
    // At least some cells work, continue detection
  }

  // ========== STEP 2: TEST A8 LINE (4164 vs 41256) ==========
  write16Pin(0, 0, 0);
  write16Pin(256, 0, 1);
  if (read16Pin(0, 0) == 0) {
    type = T_41256;  // A8 functional → 256Kx1
    return;
  }

  // ========== STEP 3: TEST A7 ROW LINE (4164 vs 4816) ==========
  write16Pin(0, 0, 0);
  write16Pin(128, 0, 1);
  if (read16Pin(0, 0) != 0) {
    // Row A7 aliased — could be 4816 or half-good 4164
    // Test col A7: if also aliased → 4816, otherwise → T_4164 (quadrant test handles 4532)
    write16Pin(0, 0, 0);
    write16Pin(0, 128, 1);
    if (read16Pin(0, 0) == 0) {
      type = T_4164;  // Col A7 works + Row A7 aliased → quadrant test will detect 4532
    } else {
      type = T_4816;  // Col A7 also aliased (128 cols) → 16Kx1
    }
    return;
  }

  // Quadrant-based detection handles TMS4532/MSM3732 during pattern tests
  type = T_4164;
}

/**
 * Detect if a 41256 is actually a 41257 (nibble mode DRAM)
 *
 * Uses nibble mode for BOTH writing and reading, making the detection
 * robust against CAS timing margins. Since write and read use the same
 * CAS toggle mechanism, the internal nibble counter behaves identically
 * in both phases — even if CAS HIGH time is borderline and causes counter
 * resets, both phases reset symmetrically and the pattern still matches.
 *
 * Detection sequence:
 *   1. Clear cols 0-3 to 0 (standard single writes)
 *   2. Nibble WRITE: 1,0,1,0 via 4 CAS toggles (WE low, address fixed at 0)
 *   3. Nibble READ: 4 CAS toggles (WE high), capture Dout after each CAS↓
 *   4. Evaluate captured values
 *
 * Expected results:
 *   41257 (nibble mode): Write reaches cols 0-3, read returns [1,0,1,0] ✓
 *   41256 (no nibble):   All CAS hits col 0 only
 *                        Write: col 0 overwritten 4× with last value 0
 *                        Read:  col 0 read 4× → [0,0,0,0]
 *
 * CAS timing note:
 *   Din for the next nibble is pre-loaded while CAS is still LOW (data is
 *   latched at CAS↓). This minimizes CAS HIGH time to ~125ns.
 *
 * Must be called after sense41256_16Pin() detects T_41256.
 */
static void detect41257_16Pin() {
  // ===== STEP 1: Clear all 4 A8 row/col combinations =====
  // A8 is shared for row and column addressing. On 41257, A8 maps to nibble counter.
  // Must clear all combinations to ensure nibble test reads known-zero state.
  write16Pin(0, 0, 0);      // row_A8=0, col_A8=0
  write16Pin(0, 256, 0);    // row_A8=0, col_A8=1
  write16Pin(256, 0, 0);    // row_A8=1, col_A8=0
  write16Pin(256, 256, 0);  // row_A8=1, col_A8=1

  // ===== STEP 2: Nibble mode WRITE — 1,0,1,0 to cols 0-3 =====
  // Din pre-loaded during CAS LOW to keep CAS HIGH minimal (tNCH)
  DDRC |= 0x02;       // Ensure DIN is OUTPUT
  SET_ADDR_PIN16(0);  // Address bus = 0 (row 0, column 0)
  RAS_LOW16;          // Latch row 0
  WE_LOW16;           // Write mode

  // Nibble 0 → col 0: Din=1
  PORTC |= 0x02;   // Din=1
  NOP;             // tDS: data setup before CAS↓
  CAS_LOW16;       // CAS↓: latch write, counter starts at col 0
  NOP;             // tCAS: CAS low pulse width
  CAS_HIGH16;      // CAS↑: counter → col 1
  PORTC &= ~0x02;  // Pre-load Din=0 for nibble
  NOP;             // tNRH: nibble recovery
  // Nibble 1 → col 1: Din=0
  CAS_LOW16;      // CAS↓: latch write to col 1
  NOP;            // tCAS
  CAS_HIGH16;     // counter → col 2
  PORTC |= 0x02;  // Pre-load Din=1 for nibble 2
  NOP;            // tNRH
  // Nibble 2 → col 2: Din=1
  CAS_LOW16;       // CAS↓: latch write to col 2
  NOP;             // tCAS
  CAS_HIGH16;      // counter → col 3
  PORTC &= ~0x02;  // Pre-load Din=0 for nibble 3
  NOP;             // tNRH
  // Nibble 3 → col 3: Din=0
  CAS_LOW16;  // CAS↓: latch write to col 3
  NOP;
  NOP;  // tCAS: ensure last write completes
  CAS_HIGH16;
  NOP;  // tWR: write recovery
  WE_HIGH16;
  NOP;  // tRWL: WE-to-RAS recovery
  RAS_HIGH16;
  // ===== STEP 3: Nibble mode READ — 4× CAS toggle, capture Dout =====
  PORTC &= ~0x02;
  DDRC &= ~0x02;  // Din as INPUT (shared Din/Dout chips)

  SET_ADDR_PIN16(0);  // Address bus = 0 (row 0, column 0)
  RAS_LOW16;          // Latch row 0
  NOP;                // tRCD: RAS-to-CAS delay

  // Nibble 0 → read col 0
  CAS_LOW16;  // CAS↓: latch col 0, Dout becomes valid after tCAC
  NOP;        // tCAC: CAS access time
  NOP;
  uint8_t v0 = PINC;  // Capture Dout (bit 2)
  CAS_HIGH16;         // CAS↑: counter → col 1
  NOP;                // tNRH

  // Nibble 1 → read col 1
  CAS_LOW16;
  NOP;  // tNCAC: nibble CAS access time
  NOP;
  uint8_t v1 = PINC;
  CAS_HIGH16;
  NOP;

  // Nibble 2 → read col 2
  CAS_LOW16;
  NOP;
  NOP;
  uint8_t v2 = PINC;
  CAS_HIGH16;
  NOP;

  // Nibble 3 → read col 3
  CAS_LOW16;
  NOP;
  NOP;
  uint8_t v3 = PINC;
  CAS_HIGH16;

  RAS_HIGH16;
  DDRC |= 0x02;  // Restore Din as OUTPUT

  // ===== STEP 4: Evaluate — check Dout (PC2 = 0x04) =====
  // 41257: [1,0,1,0] → nibble mode confirmed
  // 41256: [0,0,0,0] → no nibble mode (col 0 = 0, read 4×)
  if ((v0 & 0x04) && !(v1 & 0x04) && (v2 & 0x04) && !(v3 & 0x04)) {
    type = T_41257;
  }
}

/**
 * Verify all address lines and decoder logic function correctly
 *
 * Tests each address bit (both row and column) independently to ensure:
 * - No address lines are stuck high or low
 * - No crosstalk between address lines
 * - Address decoder properly distinguishes between addresses
 *
 * Test Method:
 * For each address bit:
 * 1. Write 0 to base address (bit=0)
 * 2. Write 1 to peer address (bit=1, all other bits=0)
 * 3. Read both addresses and verify they hold different values
 *
 * Row Test: Tests each row address bit (A0-A7 for 4164, A0-A8 for 41256)
 * Column Test: Tests each column address bit
 *
 * Special 4532 Handling:
 * For 4164 chips, A7 test may fail if chip is actually a 4532 (half-functional).
 * In this case, error is deferred to pattern testing which will identify the variant.
 *
 * Calls error() if any address bit fails, displaying bit number in error code.
 */
void checkAddressing_16Pin(void) {
  checkAddressShorts(0x15, (type == T_41256 || type == T_41257) ? 0x11 : 0x10, 0xC3);

  // Restore I/O direction: checkAddressShorts leaves all address pins as inputs with pull-ups
  DDRB = 0b00111111;
  PORTB = 0b00101010;  // RAS=HIGH, WE=HIGH
  DDRC = 0b00011011;
  PORTC = 0b00001000;  // CAS=HIGH
  DDRD = 0b11000011;
  PORTD = 0x00;

  uint16_t max_rows = ramTypes[type].rows;
  uint16_t max_cols = ramTypes[type].columns;

  // Calculate number of address bits needed
  uint8_t row_bits = countBits(max_rows - 1);
  uint8_t col_bits = countBits(max_cols - 1);

  CAS_HIGH16;
  RAS_HIGH16;
  // (a stray WE_LOW16 sat here — dead: write16Pin/read16Pin manage WE themselves)

  // ========== ROW ADDRESS LINE TEST ==========
  // Test each row address bit independently
  for (uint8_t b = 0; b < row_bits; b++) {
    uint16_t base_row = 0;          // All address bits = 0
    uint16_t peer_row = (1u << b);  // Only bit 'b' = 1

    // Write 0 to base row, 1 to peer row
    write16Pin(base_row, 0, 0);
    write16Pin(peer_row, 0, 1);

    // Read back both addresses
    uint8_t base_result = read16Pin(base_row, 0);
    uint8_t peer_result = read16Pin(peer_row, 0);

    // T_4164: skip A7 ROW test — RAS-split half-good chips (TMS4532) have an
    // aliased/dead row half. Quadrant error tracking during pattern tests will
    // detect this. Runtime gate: only skip when 32K detection is enabled;
    // otherwise A7 is tested normally and a real 4532 fails the address check.
    if (CFG_32K_ACTIVE && type == T_4164 && b == 7) continue;


    // Normal address line test - fail if values don't match expected
    if (base_result != 0) error(b, 1);  // Base address corrupted
    if (peer_result != 1) error(b, 1);  // Peer address didn't write correctly
  }

  // ========== COLUMN ADDRESS LINE TEST ==========
  // Use middle row for column testing to avoid edge cases
  uint16_t test_row = max_rows >> 1;

  // Test each column address bit independently
  for (uint8_t b = 0; b < col_bits; b++) {
    // T_4164: skip A7 COLUMN test — CAS-split half-good chips (MSM3732) have a
    // dead/aliased column half that would fail here as "Addressline A23" before
    // the quadrant tracking can classify them. Symmetric to the row A7 skip
    // above (TMS4532, RAS-split); quadrant evaluation covers both splits.
    if (CFG_32K_ACTIVE && type == T_4164 && b == 7) continue;

    uint16_t base_col = 0;          // All address bits = 0
    uint16_t peer_col = (1u << b);  // Only bit 'b' = 1

    // Write 0 to base column, 1 to peer column
    write16Pin(test_row, base_col, 0);
    write16Pin(test_row, peer_col, 1);

    // Read back and verify - column errors offset by 16 for distinction
    if (read16Pin(test_row, base_col) != 0) error(b + 16, 1);
    if (read16Pin(test_row, peer_col) != 1) error(b + 16, 1);
  }
}

//=======================================================================================
// CORE READ/WRITE FUNCTIONS (OPTIMIZED WITH NESTED LOOPS)
//=======================================================================================

/**
 * Perform RAS-only refresh cycle for specified row
 *
 * Executes a RAS-before-CAS (RBC) refresh cycle:
 * 1. Deasserts RAS
 * 2. Sets row address on multiplexed bus
 * 3. Asserts RAS to latch row address
 *
 * This refreshes all columns in the specified row without needing column addresses.
 * Used during retention testing to maintain data between write and verify operations.
 *
 * @param row Row address to refresh (0-255 for 4164, 0-511 for 41256)
 */
void rasHandling_16Pin(uint16_t row) {
  RAS_HIGH16;             // Deassert RAS
  setAddr16_Random(row);  // Set row address on bus
  RAS_LOW16;              // Assert RAS to latch address
}

/**
 * Write test pattern to entire row using optimized page mode
 *
 * This function performs high-speed page mode writes using split lookup tables
 * and nested loops for maximum performance. Marked with __attribute__((hot))
 * for compiler optimization.
 *
 * Only used for the pseudo-random patterns 4-5 (the checkerboard passes 0-1 run
 * through fastPatternTest_16Pin). Writes the whole row, then performs retention
 * testing by reading a row written `delayRows` iterations earlier.
 *
 * Operation Sequence:
 * 1. Asserts RAS with row address
 * 2. Asserts WE for write mode
 * 3. For each column: sets column address + random data, pulses CAS
 * 4. Writes the complete row before any read-back, ensuring proper RAS cycling
 *    between write and read (required by chips like the Toshiba 41257).
 *
 * Optimization Strategy:
 * - Outer loop handles high address bits (A5-A8) in 32-column chunks
 * - Inner loop handles low address bits (A0-A4) for fast sequential access
 * - Pre-calculates base port values to minimize operations in inner loop
 * - Disables interrupts during critical timing sections
 *
 * Data Bit Mapping:
 * - Data is checked against bit 2 (0x04) to match DOUT on PC2
 * - DIN is set on bit 1 (0x02) on PC1
 * - IMPORTANT: base_c is masked with 0xEC to clear DIN bit, allowing clean setting
 *
 * 4532 Detection:
 * All pattern errors are detected by checkRow_16Pin which tracks column
 * position for half-functional chip detection (4532/3732 variants).
 *
 * @param row Row address to write (0-255 for 4164, 0-511 for 41256)
 * @param cols Number of columns in row (256 for 4164, 512 for 41256)
 * @param patNr Pattern number (4 = pseudo-random, 5 = inverted pseudo-random)
 */
// Retention read-back thunk for the shared retentionTail (patterns 4-5)
static void retCheck_16Pin(uint16_t row, uint8_t patNr) {
  checkRow_16Pin(ramTypes[type].columns, row, patNr, 3);
}

void __attribute__((hot)) writeRow_16Pin(uint16_t row, uint16_t cols, uint8_t patNr) {
  // Last row for retention testing
  // For nibble mode, only half the rows are iterated (A8 selects nibble, not row)
  uint16_t total_rows = ramTypes[type].rows;
  if (ramTypes[type].flags & RAM_FLAG_NIBBLE_MODE) total_rows /= 2;
  uint16_t last_row = total_rows - 1;

  // Ensure PC1 (Din) is OUTPUT for writing (required for shared Din/Dout chips)
  DDRC |= 0x02;
  CAS_HIGH16;

  // Calculate number of 32-column chunks
  uint8_t num_chunks = cols >> 5;
  bool is_nibble = (ramTypes[type].flags & RAM_FLAG_NIBBLE_MODE);

  if (!is_nibble) {
    // ====== Non-nibble (4164/4164_2MS/4816/4532/3732/41256): page-mode write with
    // an INLINE ROW-SNAPSHOT tRAS recycle. The NMOS generation specs tRAS max =
    // 10 us with NO page-mode allowance; the old 4/8-col cadence measured
    // 11.8-23.9 us. Restoring the three snapshotted ports + a RAS toggle re-opens
    // the row in ~0.4 us (vs ~2.5 us via setAddr16_Random), so the recycle runs
    // every 2 columns -> window ~6.0-6.6 us. Designed calibration-neutral (cheap
    // recycle x higher frequency ~ the old per-row recycle total). WE stays LOW
    // for the whole row write. ======
    uint16_t row_mix = row + (row >> 4);  // mix8 row part, hoisted per row
    cli();  // BEFORE the row open: an ISR landing between RAS_LOW and a later cli()
            // stretches that RAS window by ~6-7 us (LA: ~15 us spikes, ~2 per ms)
    rasHandling_16Pin(row);
    WE_LOW16;
    // Snapshot AFTER row open + WE low: a port restore reproduces the full row-open
    // state (stale DIN bit is don't-care — the next cell rewrites PORTC before CAS).
    // TWO PORTB variants are needed because mask 0xEA keeps the RAS bit (PB1):
    //  - row_b (RAS LOW, as snapshotted) feeds base_b -> the CELLS keep the row open;
    //  - row_b_hi (RAS forced HIGH) is the RECYCLE restore -> RAS must stay high while
    //    the ports are restored; a low RAS bit would re-latch the row one cycle after
    //    RAS_HIGH16 with PORTC/PORTD still holding COLUMN state (garbage row, ~62 ns
    //    tRP). The trailing RAS_LOW16 is the real, full-address falling edge.
    uint8_t row_b = PORTB, row_c = PORTC, row_d = PORTD;
    uint8_t row_b_hi = row_b | 0x02;
    for (uint8_t chunk = 0; chunk < num_chunks; chunk++) {
      uint8_t hb = pgm_read_byte(&lut_16_high_b[chunk]);
      uint8_t hc = pgm_read_byte(&lut_16_high_c[chunk]);
      uint8_t hd = pgm_read_byte(&lut_16_high_d[chunk]);
      // Bases derived from the SNAPSHOT (the recycle restores the ports to exactly
      // this state) -> loop-invariant per chunk, no PORTx re-reads after recycles.
      uint8_t base_b = (row_b & 0xEA) | hb;
      uint8_t base_c = (row_c & 0xEC) | hc;
      uint8_t base_d = (row_d & 0x3C) | hd;
      // K-hoist: mix8(chunkBase|c, row) == K ^ c for c in 0..31 (c only touches the low
      // 5 bits, which are 0 in chunkBase) -> compute K once per chunk; per cell a single
      // eor replaces the whole mix8 + 16-bit OR. Table indices are IDENTICAL to mix8 ->
      // bit-identical data pattern, no recalibration.
      uint16_t vb = (uint16_t)(chunk << 5) ^ row_mix;
      uint8_t K = (uint8_t)(vb ^ (vb >> 8));

      // Burst loop: descending order (31..0), recycle after every 2 cells (skipped
      // after the last cell of the last chunk — the outer CAS/RAS_HIGH16 ends that).
      uint8_t c = 32;
      do {
        for (uint8_t k = 2; k; k--) {
          c--;
          // One address calc (ldd Z+0/+1/+2) BEFORE the randomTable access clobbers Z.
          // (A running `ll--` pointer was tried: -5 cyc/cell but Z gets pinned -> K
          // spills to the stack and the table access moves to X = +40 B flash for
          // ~0.16 s. Not worth it; the c*3 recompute stays.)
          const struct LutLow16 *ll = &lut_16_low[c];
          uint8_t lb = ll->pb, lc = ll->pc, ld = ll->pd;
          uint8_t val_c = base_c | lc;
          uint8_t data_check = randomTable[(uint8_t)(K ^ c)];
          if (data_check & 0x04) val_c |= 0x02;
          CAS_HIGH16;
          PORTB = base_b | lb;
          PORTD = base_d | ld;
          PORTC = val_c;
          CAS_LOW16;
        }
        if (c > 0 || chunk < num_chunks - 1) {
          CAS_HIGH16;        // end the write strobe (cell left CAS low)
          RAS_HIGH16;        // inline row-snapshot recycle (RAS-only, no CBR)
          PORTB = row_b_hi;  // restore with RAS kept HIGH (~375 ns precharge)
          PORTC = row_c;
          PORTD = row_d;
          RAS_LOW16;         // real falling edge: full row address on all ports
        }
      } while (c);
    }
    CAS_HIGH16;
    WE_HIGH16;
    RAS_HIGH16;  // close the row BEFORE sei(): a pending timer ISR (blocked for the
    sei();       // whole cli row, so almost always pending) fires immediately at sei
                 // and would stretch the still-open last RAS window by ~7 us
  } else {
    // ====== NIBBLE PATH (41257): Single access — one RAS cycle per cell ======
    // No page mode → no tRAS max problem. ~1.2µs/cell, ~315ms per pass.
    WE_LOW16;
    cli();  // keep ISR jitter out of the nibble CAS bursts / RAS-low windows
    for (uint16_t col = 0; col < cols / 2; col++) {
      setAddr16_Random(row);
      RAS_LOW16;
      uint8_t data0 = randomTable[mix8(col, row)];
      uint8_t data1 = data0 >> 1;
      uint8_t data2 = data0 << 1;
      uint8_t data3 = data0 << 2;
      setAddr16_Random(col);
      SET_DIN_16(data0);
      CAS_LOW16;
     // NOP;
      CAS_HIGH16;
      SET_DIN_16(data1);
      CAS_LOW16;
     // NOP;
      CAS_HIGH16;
      SET_DIN_16(data2);
      CAS_LOW16;
    //  NOP;
      CAS_HIGH16;
      SET_DIN_16(data3);
      CAS_LOW16;
    //  NOP;
      CAS_HIGH16;
      RAS_HIGH16;  // reset the nibble counter for the next base
    }
    sei();
    WE_HIGH16;
  }

  // ========== RETENTION TESTING (patterns 4-5; all types incl. 41257 nibble) ==========
  // Shared aging tail (common.cpp retentionTail): delays[5] + delayRows pipeline,
  // last-row drain with simulated writeTime. retCheck_16Pin re-supplies the cols
  // parameter (always ramTypes[type].columns, same value writeRow received).
  retentionTail(row, last_row, patNr, retCheck_16Pin);
}

/**
 * Perform RAS-only refresh on specified row
 *
 * Executes a RAS-only refresh cycle without CAS assertion. This refreshes
 * all columns in the row simultaneously, maintaining data integrity during
 * the gap between write and verify operations in retention testing.
 *
 * @param row Row address to refresh (0-255 for 4164, 0-511 for 41256)
 */
void refreshRow_16Pin(uint16_t row) {
  rasHandling_16Pin(row);  // Assert RAS with row address
  RAS_HIGH16;              // Deassert RAS to complete refresh cycle
}

/**
 * Verify test pattern in row using optimized page mode read
 *
 * This function reads back and verifies previously written test patterns using the same
 * optimized split lookup table approach as writeRow_16Pin. It implements sophisticated
 * 4532 half-functional chip detection logic that allows testing to continue when errors
 * are confined to one half of the address space.
 *
 * Data Bit Mapping:
 * - Data is read from DOUT on PC2 (bit 2 of PINC)
 * - Expected data is XORed with PINC and masked with 0x04 to check bit 2
 * - This matches the data bit handling in writeRow_16Pin where DIN is on PC1
 *
 * Optimization Strategy:
 * - Uses split lookup tables to minimize inner loop overhead
 * - Outer loop handles A5-A8 in 32-column chunks (8 or 16 iterations for 4164/41256)
 * - Inner loop handles A0-A4 as fast sequential access (32 iterations)
 * - Pre-calculates base port values to eliminate redundant bit manipulation
 * - Disables interrupts during entire read sequence for consistent timing
 *
 * Quadrant Error Detection:
 * Errors are tracked by quadrant using row A7 and col A7:
 *   qbit = ((row>=128)?2:0) | ((col>=128)?1:0)
 * If not all 4 quadrants are bad, returns without error.
 * After all patterns, caller evaluates quadrantErrors via LUT.
 *
 * @param cols Number of columns to verify (256 for 4164/4816/4532, 512 for 41256/41257)
 * @param row Row address to verify (0-255 for 4164/4816, 0-511 for 41256)
 * @param patNr Pattern number being verified (4 = random, 5 = random inverted).
 *              Only the random patterns reach this function.
 * @param check Error code to report if verification fails:
 *              2=pattern error (from writeRow), 3=retention error (after refresh delay)
 *
 * @note The expected value is regenerated from randomTable using mix8(col, row).
 *
 * @note Function calls error() and never returns if a real failure is detected.
 *       For 4532 chips, errors in the expected bad half cause early return without error.
 */
void __attribute__((hot)) checkRow_16Pin(uint16_t cols, uint16_t row, uint8_t patNr, uint8_t check) {
  // Only ever called for the random patterns 4-5; the checkerboard passes 0-1
  // verify inline in fastPatternTest_16Pin. Expected data therefore always comes
  // from randomTable via mix8() — there is no constant-pattern path here.

  // Calculate number of 32-column chunks
  uint8_t num_chunks = cols >> 5;
  uint8_t row_errors = 0;  // Bit 0 = col<128 error, bit 1 = col>=128 error
  bool is_nibble = (ramTypes[type].flags & RAM_FLAG_NIBBLE_MODE);

  // Switch PC1 (Din) to INPUT for shared Din/Dout chips (e.g., 41256 in A501)
  PORTC &= ~0x02;
  DDRC &= ~0x02;

  if (!is_nibble) {
    // ====== Non-nibble read (4164/4164_2MS/4816/4532/3732/41256): page mode with an
    // INLINE ROW-SNAPSHOT tRAS recycle every 2 columns (see writeRow_16Pin — NMOS
    // tRAS max 10 us, no page-mode allowance; old 4/8-col cadence was 11.8-23.9 us).
    // 41257 uses the nibble path below. (Since the 5.0.3 revert the 41256 random
    // retention runs HERE, not through fastPatternTest_16Pin.)
    uint16_t row_mix = row + (row >> 4);  // mix8 row part, hoisted per row
    cli();  // BEFORE the row open (see writeRow_16Pin: ISR-stretched RAS windows)
    rasHandling_16Pin(row);
    // Snapshot AFTER row open (WE stays high for the whole read). Two PORTB variants
    // (see writeRow_16Pin): row_b (RAS LOW, as snapshotted) feeds base_b -> the cells
    // keep the row open (mask 0xEA carries the RAS bit into base_b!); row_b_hi (RAS
    // forced HIGH) is the recycle restore -> no premature re-latch on stale columns.
    uint8_t row_b = PORTB, row_c = PORTC, row_d = PORTD;
    uint8_t row_b_hi = row_b | 0x02;
    for (uint8_t chunk = 0; chunk < num_chunks; chunk++) {
      uint8_t hb = pgm_read_byte(&lut_16_high_b[chunk]);
      uint8_t hc = pgm_read_byte(&lut_16_high_c[chunk]);
      uint8_t hd = pgm_read_byte(&lut_16_high_d[chunk]);
      // Bases from the snapshot — loop-invariant (the recycle restores the ports).
      uint8_t base_b = (row_b & 0xEA) | hb;
      uint8_t base_c = (row_c & 0xEC) | hc;
      uint8_t base_d = (row_d & 0x3C) | hd;
      uint8_t half_bit = (chunk >= 4) ? 2 : 1;  // col half: chunks 0-3 < 128, 4+ >= 128
      // K-hoist (see writeRow_16Pin): mix8(chunkBase|c, row) == K ^ c, bit-identical.
      uint16_t vb = (uint16_t)(chunk << 5) ^ row_mix;
      uint8_t K = (uint8_t)(vb ^ (vb >> 8));

      // Burst loop (see writeRow_16Pin): 2-cell bursts, snapshot recycle in between.
      uint8_t c = 32;
      do {
        for (uint8_t k = 2; k; k--) {
          c--;
          SET_ADDR_16_LOW(c);
          CAS_LOW16;
          NOP;         // tCAC margin: without it the sample lands 1 cycle (62.5 ns)
                       // early — 6 ns short of the -20 grade's CAS access time.
          CAS_HIGH16;  // IMPORTANT: sample PINC only AFTER CAS_HIGH16. Looks wrong but
                       // it gives DOUT + the ATmega input pin time to settle (tCAC).
                       // Reading right after CAS_LOW16 samples too early; the nibble
                       // path always read after CAS_HIGH16, which is why 41257 never
                       // showed this. Do NOT move the read before CAS_HIGH16.
          if (((PINC ^ randomTable[(uint8_t)(K ^ c)]) & 0x04) != 0) {
            row_errors |= half_bit;
          }
        }
        // Burst done -> inline row-snapshot recycle (RAS-only; CAS already high after
        // the read strobe). Skipped after the last burst of the last chunk.
        if (c > 0 || chunk < num_chunks - 1) {
          RAS_HIGH16;
          PORTB = row_b_hi;  // restore with RAS kept HIGH (~375 ns precharge)
          PORTC = row_c;
          PORTD = row_d;
          RAS_LOW16;         // real falling edge: full row address on all ports
        }
      } while (c);
    }
    CAS_HIGH16;
    RAS_HIGH16;  // close the row BEFORE sei() — see writeRow_16Pin (pending-ISR stretch)
    sei();

  } else {
    cli();  // keep ISR jitter out of the nibble CAS bursts / RAS-low windows
    for (uint16_t col = 0; col < cols / 2; col++) {
      uint8_t data0 = randomTable[mix8(col, row)];
      uint8_t data1 = data0 >> 1;
      uint8_t data2 = data0 << 1;
      uint8_t data3 = data0 << 2;
      setAddr16_Random(row);
      RAS_LOW16;
      setAddr16_Random(col);
      CAS_LOW16;
      CAS_HIGH16;
      data0 = PINC ^ data0;
      CAS_LOW16;
      CAS_HIGH16;
      data1 = PINC ^ data1;
      CAS_LOW16;
      CAS_HIGH16;
      data2 = PINC ^ data2;
      CAS_LOW16;
      CAS_HIGH16;
      data3 = PINC ^ data3;
      if ((data0 | data1 | data2 | data3) & 0x04) {
        sei();  // interrupt discipline: error()/return paths need interrupts back
        CAS_HIGH16;
        RAS_HIGH16;
        DDRC |= 0x02;
        if (retryActive) {
          retryActive = 2;
          return;
        }
        error(patNr + 1, check);
      }
      RAS_HIGH16;  // reset the nibble counter for the next base
    }
    sei();
  }

  // Switch PC1 (Din) back to OUTPUT for next write operation
  DDRC |= 0x02;

  // --- ERROR HANDLING (non-nibble path only) ---
  if (row_errors) {
    // Quadrant detection only for the full-geometry 4164 (half-good 4532/3732
    // are detected AS T_4164). For 4816/41256/41257 quadrantErrors can never
    // reach 0x0F, so we must NOT defer — fail immediately instead.
    if (CFG_32K_ACTIVE && (type == T_4164 || type == T_4164_2MS)) {
      quadrantErrors |= row_errors << ((row >= 128) ? 2 : 0);
      if (quadrantErrors != 0x0F)
        return;
    }
    if (retryActive) {
      retryActive = 2;
      return;
    }
    error(patNr + 1, check);
  }
}

//=======================================================================================
// REFRESH TIME TEST (41256/41257)
//=======================================================================================

/**
 * Perform a single CAS-before-RAS refresh cycle
 *
 * CAS-before-RAS refresh is a special DRAM refresh mode where asserting CAS before
 * RAS triggers the chip's internal refresh counter to auto-increment and refresh
 * the next row without needing to specify a row address externally.
 *
 * This is more efficient than RAS-only refresh as it doesn't require cycling through
 * all row addresses - the DRAM handles address sequencing internally.
 *
 * Timing: Total cycle ~5-10μs (CAS/RAS setup + hold + deassert)
 */
static void casBeforeRasRefresh_16Pin() {
  RAS_HIGH16;
  CAS_LOW16;  // Assert CAS first
  RAS_LOW16;  // Then assert RAS (triggers auto-refresh)
  NOP;
  NOP;         // Hold time for refresh
  RAS_HIGH16;  // Deassert RAS
  CAS_HIGH16;  // Deassert CAS
}

#if 0  // OBSOLETE (5.0.3): refresh-time test; the ~60 s CBR counter test is now folded
       // into fastPatternTest_16Pin via the runCBR flag (kept here for reference only).
/**
 * Test DRAM refresh timing for 41256/41257 chips
 *
 * This function verifies that the chip can retain data when refreshed at the maximum
 * specified interval. Both 41256 and 41257 specify 4ms refresh time (256 CBR cycles).
 *
 * Algorithm:
 * 1. Write 1 random bit per row using single-cycle access (col 0 only)
 *    - Sequential row traversal provides implicit RAS-only refresh
 *    - No explicit refresh needed during write phase
 * 2. Perform 10 full CAS-before-RAS refresh sequences (10 × 4ms = 40ms)
 *    - Tests internal CBR counter wraps correctly through all rows
 * 3. Read back and verify using single-cycle access
 *    - Sequential row traversal again provides implicit refresh
 *
 * CBR refresh note:
 * Both chips have 512 physical rows but only need 256 CBR cycles.
 * The internal 8-bit refresh counter refreshes TWO rows per cycle:
 *   Cycle 0 → rows 0 and 256, Cycle 1 → rows 1 and 257, etc.
 * 4ms / 256 = 15.625μs per refresh cycle.
 */
static void refreshTimeTest_16Pin() {
  uint16_t rows = ramTypes[type].rows;  // 512 for 41256/41257

  // ===== PHASE 1: WRITE 1 BIT OF RANDOM DATA TO EACH ROW =====
  // Single-cycle write to col 0. The sequential row traversal through all 512 rows
  // acts as implicit RAS-only refresh — each row is touched within the loop time
  // which is well under 4ms for 512 rows (~5µs/row = ~2.6ms total).
  for (uint16_t row = 0; row < rows; row++) {
    write16Pin(row, 0, randomTable[row & 0xFF] & 0x01);
    casBeforeRasRefresh_16Pin();
  }

  // ===== PHASE 2: CBR REFRESH FOR ~60 s =====
  // Run CBR-only at the spec rate (~15.625 µs/cycle) for a full minute, then read
  // back. A broken refresh COUNTER leaves some rows unrefreshed each sweep → after
  // 60 s the accumulated data loss is unmistakable (a ~1 s test was too short to
  // expose a marginal counter). millis() bounds the duration (32-bit).
  uint32_t cbr_t0 = millis();
  do {
    casBeforeRasRefresh_16Pin();
    delayMicroseconds(15);
    NOP;
    NOP;
    NOP;
    NOP;
    NOP;
  } while ((uint32_t)(millis() - cbr_t0) < 60000UL);

  // ===== PHASE 3: READ BACK AND VERIFY =====
  // Single-cycle read from col 0. Same implicit refresh argument as Phase 1.
  for (uint16_t row = 0; row < rows; row++) {
    uint8_t expected = randomTable[row & 0xFF] & 0x01;
    if (read16Pin(row, 0) != expected) {
      error(0, 5);  // Refresh timer test failed
      return;
    }
    casBeforeRasRefresh_16Pin();
  }
}
#endif  // OBSOLETE refreshTimeTest_16Pin