// 18Pin.cpp - Implementation of 18-Pin DRAM testing functions
//=======================================================================================
//
// This file contains all test logic for 18-pin DRAM packages. It implements:
// - Standard 18-pin DRAM testing (4416, 4464)
// - Alternative 18-pin configuration for 411000 (1Mx1)
// - Optimized lookup tables for fast address setting
// - Checkerboard coupling test (passes 0-1) + pseudo-random patterns (4-5)
// - Page mode read/write operations with minimal timing overhead
//
// Supported chips:
// - 4416 (16Kx4, 256 rows x 64 cols, 4ms refresh)
// - 4464 (64Kx4, 256 rows x 256 cols, 4ms refresh)
// - 411000 (1Mx1, 1024 rows x 1024 cols, 8ms refresh, alternative pinout)
// - 2114 (1Kx4 SRAM)
//=======================================================================================

#include "18Pin.h"

//=======================================================================================
// 18-PIN PORT MAPPINGS
//=======================================================================================

// Port-to-DIP-Pin mappings for standard 18-pin DRAMs
const uint8_t CPU_18PORTB[] = { 15, 4, 14, 3, EOL, EOL, EOL, EOL };
const uint8_t CPU_18PORTC[] = { 1, 2, 16, 17, 5, EOL, EOL, EOL };
const uint8_t CPU_18PORTD[] = { 6, 7, 8, 9, NC, 10, 11, 12 };
const uint8_t RAS_18PIN = 18;
const uint8_t CAS_18PIN = 16;

// Alternative 18-pin constants
const uint8_t RAS_18PIN_ALT = 11;
const uint8_t CAS_18PIN_ALT = A2;

// tRAS-max guard (4416/4464): standard page mode would keep RAS active for the
// whole row (up to 256 CAS), far beyond the 10 µs RAS-active spec, so RAS is
// recycled (precharge + re-assert the same row, which also refreshes it) during
// every page burst. Current cadence (5.0.4):
//   - 4464: hidden CBR refresh after EVERY column in the checkerboard pass;
//     RAS-only recycle every 8 columns in the random/retention path.
//   - 4416 (no CBR capability): RAS-only recycle every 2 columns in all paths
//     (per-CAS ~2.5-3.5 µs -> 2 columns keep RAS-active ~5-7 µs, under spec;
//     the former 8-column burst was ~20-28 µs = over spec).
// Verify burst lengths on a logic analyser after timing-relevant changes.

//=======================================================================================
// 18-PIN ALTERNATIVE LOOKUP TABLE GENERATION
//=======================================================================================

// Macro for PORT B (Starting from A7)
#define CALC_PORTB(a) \
  ((((a)&0x080) >> 3) | (((a)&0x100) >> 6) | (((a)&0x200) >> 9))

// PORTD low bits (A1-A6) are computed arithmetically in setAddr_18Pin_Alt
// (formerly a 128-byte PROGMEM LUT). The mapping is just two shifts:
//   A1-A3 (bits 1-3) >> 1  +  A4-A6 (bits 4-6) << 1  ==  (a&0x0E)>>1 | (a&0x70)<<1

// 2. Tabelle für die oberen Bits (PORTB). Größe: 8 Bytes.
// Deckt die High-Bits für 0, 128, 256 ... 896 ab.
const uint8_t lut_18Pin_High[8] PROGMEM = {
  CALC_PORTB(0), CALC_PORTB(128), CALC_PORTB(256), CALC_PORTB(384),
  CALC_PORTB(512), CALC_PORTB(640), CALC_PORTB(768), CALC_PORTB(896)
};

// PORTB address parts for fast iteration: maps index 0-7 → PB0(A9), PB2(A8), PB4(A7)
static const uint8_t pb_addr_18Alt[8] PROGMEM = {
  0x00, 0x01, 0x04, 0x05, 0x10, 0x11, 0x14, 0x15
};

/**
 * Address setter (split LUT). The hot per-cell version setAddr18Alt_inl() is FORCE-inlined
 * (always_inline) and used ONLY in the random-path inner loops (writeRow/checkRow_18Pin_Alt,
 * ~4 M calls/run) to drop the CALL/RET overhead there. The plain setAddr_18Pin_Alt() wraps
 * it once for the ~21 cold call sites (detection, address-walk, RAS recycles) — so only the
 * 2 hot loops pay the inline-duplication flash cost.
 */
static inline __attribute__((always_inline)) void setAddr18Alt_inl(uint16_t a) {
  if (a & 1) SBI(PORTC, 4);
  else CBI(PORTC, 4);
  // PORTD low bits: A1-A3 (bits 1-3) >>1, A4-A6 (bits 4-6) <<1 (was a 128-byte LUT)
  PORTD = (PORTD & 0b00011000) | ((a & 0x0E) >> 1) | ((a & 0x70) << 1);
  PORTB = (PORTB & 0b11101010) | pgm_read_byte(&lut_18Pin_High[a >> 7]);
}
void setAddr_18Pin_Alt(uint16_t a) { setAddr18Alt_inl(a); }  // callable wrapper (cold sites)

//=======================================================================================
// 2114 Basic Functions
//=======================================================================================

// NOTE: the 2114 is inserted UPSIDE-DOWN (180° rotated in the socket) because its
// Vcc/GND are swapped relative to the tester's 18-pin power rails. The rotation maps
// every chip pin p to socket pin ((p+8) % 18) + 1, so each 2114 signal is driven on
// the port bit that the diametrically opposite pin used before. Resulting mapping:
//   A0=PB2 A1=PB0 A2=PC2 A3=PB4 A4=PD7 A5=PD6 A6=PD5 A7=PD2 A8=PD1 A9=PD0
//   IO1=PC4 IO2=PB1 IO3=PB3 IO4=PC1   _CS=PC3  _WE=PC0
#define ADDR_MASK_B 0x15  // PB0(A1) PB2(A0) PB4(A3)
#define ADDR_MASK_C 0x04  // PC2(A2)
#define ADDR_MASK_D 0xE7  // PD0(A9) PD1(A8) PD2(A7) PD5(A6) PD6(A5) PD7(A4)
#define IO_MASK_B 0x0A    // PB1 PB3
#define IO_MASK_C 0x12    // PC1 PC4
#define CS_2114 (1 << PC3)
#define WE_2114 (1 << PC0)

// Strobe timing for the 2114 data/address access (cycles @ 16 MHz, 1 cyc = 62.5 ns).
// This is a FUNCTIONAL tester, not a speed-grade sorter — so the timing is sized for the
// SLOWEST datasheet class (std F2114, 450 ns: tCO 100 ns, tWP 200 ns, tACC 450 ns) PLUS
// generous vintage margin, so any working 2114 (incl. old GDR/DDR-era clones that don't
// quite meet datasheet) passes. tACC is covered with huge margin by the ~1.5 us address
// setup (drive_addr_2114) that runs before _CS, so the read settle only needs to cover tCO.
//   RD_SETTLE_NOPS = 4  -> 250 ns _CS-to-data settle (>= tCO 100 ns, 2.5x margin)
//   WR_PULSE_NOPS  = 6  -> 375 ns _WE-low write pulse (>= tWP 200 ns, ~1.9x margin)
#define RD_SETTLE_NOPS 4
#define WR_PULSE_NOPS 6

// --- 2114 speed grade (5.0.5: tACC probe) ----------------------------------------------
// The former per-class strobe sweep could not discriminate: its class delays sat on top
// of ~250 ns of instruction overhead (>= the SLOWEST grade's tWP/tCO), and the headline
// spec — tACC 200/300/450 ns — was never exercised at all (drive_addr_2114 settles the
// address ~1.5 us before _CS). Every working chip therefore graded "-20".
// Now a dedicated tACC probe (speedProbe_2114, defined after check_2114) runs ONCE at
// the END of the test, when the chip is already known-good; ALL functional tests use
// the fixed proven slow strobes. g_speedSuffix feeds printTestOK ("-20"/"-30"/"-45").
const __FlashStringHelper *g_speedSuffix = NULL;
static const char sfx20_2114[] PROGMEM = "-20";
static const char sfx30_2114[] PROGMEM = "-30";
static const char sfx45_2114[] PROGMEM = "-45";
static const __FlashStringHelper *speedProbe_2114(void);

/**
 * Initialize port directions for 2114 SRAM access
 *
 * Configures address, data and control pins as outputs. Control lines _CS (PC3)
 * and _WE (PC0) are driven HIGH (idle / inactive). Must be called once before
 * any sram2114_read/write operation.
 */
void sram2114_init() {
  DDRB |= ADDR_MASK_B | IO_MASK_B;
  DDRC |= ADDR_MASK_C | IO_MASK_C | CS_2114 | WE_2114;
  DDRD |= ADDR_MASK_D;
  PORTC |= CS_2114 | WE_2114;  // deassert _CS, _WE
}

/**
 * Drive 10-bit 2114 address bus across PORTB/PORTC/PORTD
 *
 * Address bits are mapped to non-sequential port pins (see inline comments).
 * Implemented as non-inline static so the port-shuffle code exists only once
 * and is shared by sram2114_write, sram2114_read and check_CSWE_2114.
 *
 * @param a 10-bit address (A0..A9, upper bits ignored)
 */
static void drive_addr_2114(uint16_t a) {
  PORTB = (PORTB & ~ADDR_MASK_B)
          | ((a >> 1) & 0x01)   // A1 → PB0
          | ((a << 2) & 0x04)   // A0 → PB2
          | ((a << 1) & 0x10);  // A3 → PB4
  PORTC = (PORTC & ~ADDR_MASK_C)
          | (a & 0x04);  // A2 → PC2
  PORTD = (PORTD & ~ADDR_MASK_D)
          | ((a >> 9) & 0x01)   // A9 → PD0
          | ((a >> 7) & 0x02)   // A8 → PD1
          | ((a >> 5) & 0x04)   // A7 → PD2
          | ((a >> 1) & 0x20)   // A6 → PD5
          | ((a << 1) & 0x40)   // A5 → PD6
          | ((a << 3) & 0x80);  // A4 → PD7
}

/**
 * Drive 4-bit 2114 data bus (IO1..IO4) across PORTB and PORTC
 *
 * Mirrors the inverse mapping of sram2114_read so that a write/read round-trip
 * preserves the nibble value. DDR for IO_MASK_B / IO_MASK_C must already be
 * set to outputs by the caller (sram2114_write does this). The exact D-bit to
 * IO-line permutation is irrelevant as long as write and read stay consistent.
 *
 * @param d 4-bit data nibble to drive (upper 4 bits ignored)
 */
static void drive_data_2114(uint8_t d) {
  PORTB = (PORTB & ~IO_MASK_B)
          | (d & 0x02)          // D1 → PB1
          | ((d << 1) & 0x08);  // D2 → PB3
  PORTC = (PORTC & ~IO_MASK_C)
          | ((d >> 2) & 0x02)   // D3 → PC1
          | ((d << 4) & 0x10);  // D0 → PC4
}

/**
 * Write one 4-bit nibble to a 2114 SRAM cell
 *
 * Drives the address bus, switches the data bus to outputs, asserts _CS LOW
 * and pulses _WE LOW for WR_PULSE_NOPS cycles (6 = 375 ns @ 16 MHz), which
 * covers the slow 2114's tWP of 200 ns with ~1.9x margin. _CS and _WE return
 * HIGH at end.
 *
 * @param a 10-bit cell address
 * @param d 4-bit data nibble to write
 */
void sram2114_write(uint16_t a, uint8_t d) {
  drive_addr_2114(a);
  // Data bus to outputs and drive
  DDRB |= IO_MASK_B;
  DDRC |= IO_MASK_C;
  drive_data_2114(d);
  // Assert _CS, pulse _WE for >= tWP (see WR_PULSE_NOPS)
  PORTC &= ~CS_2114;  // _CS low
  PORTC &= ~WE_2114;  // _WE low
  __builtin_avr_delay_cycles(WR_PULSE_NOPS);
  PORTC |= WE_2114;  // _WE high
  PORTC |= CS_2114;  // _CS high
}

/**
 * Read one 4-bit nibble from a 2114 SRAM cell
 *
 * Switches the data bus to inputs WITH pull-ups (a present 2114 drives IO hard
 * enough that the weak pull-up is harmless; an empty socket reads 0xF so presence
 * detection cannot false-positive), drives the address bus, asserts _CS LOW, waits
 * RD_SETTLE_NOPS cycles (4 = 250 ns @ 16 MHz, covers the slow 2114's tCO of
 * 100 ns with 2.5x margin) and samples PINB/PINC. _CS returns HIGH before exit.
 *
 * @param a 10-bit cell address
 * @return 4-bit nibble in lower nibble of return value; upper nibble = 0
 */
uint8_t sram2114_read(uint16_t a) {
  // Data bus to inputs WITH pull-ups. A present 2114 drives IO hard during a read
  // (_CS low, _WE high), so the weak (~20-50 kOhm) pull-up cannot corrupt the sampled
  // value. On an EMPTY socket the floating IO lines are instead pulled to 0xF, which
  // is what makes ram_present_2114 robust: it writes 0 and expects 0 back, but a bare
  // socket now returns 0xF != 0 so presence detection correctly fails (no false A0).
  DDRB &= ~IO_MASK_B;
  PORTB |= IO_MASK_B;
  DDRC &= ~IO_MASK_C;
  PORTC |= IO_MASK_C;
  drive_addr_2114(a);
  // Assert _CS, wait tCO before sampling (see RD_SETTLE_NOPS)
  PORTC &= ~CS_2114;
  __builtin_avr_delay_cycles(RD_SETTLE_NOPS);
  uint8_t pb = PINB;
  uint8_t pc = PINC;
  PORTC |= CS_2114;  // deassert _CS

  return ((pc >> 4) & 0x01)     // PC4 → D0
         | (pb & 0x02)          // PB1 → D1
         | ((pb >> 1) & 0x04)   // PB3 → D2
         | ((pc << 2) & 0x08);  // PC1 → D3
}

//=======================================================================================
// MAIN TEST FUNCTION
//=======================================================================================

// Forward declarations
static inline void casBeforeRasRefresh_18Pin();      // CBR (defined below; used by pass_18Pin)
static inline void casBeforeRasRefresh_18Pin_Alt();  // CBR (defined below; used by pass_18Alt)
static void pass_18Alt(uint8_t patNr, bool runCBR);  // 411000 pass 0-1; runCBR = 60s CBR test
static void pass_18Pin(uint8_t patNr, bool runCBR);  // 4464 pass 0-1; runCBR = 60s CBR test
static void pass_4416_LAG(uint8_t patNr);            // 4416 pass 0-1 (LAG-2, no CBR)

/**
 * Main test function for 18-pin DRAMs (4416, 4464, and KM41C1000 alternative types)
 * Configures I/O ports, detects RAM type, and routes to appropriate test functions
 */
void test_18Pin() {
  // Configure I/O for Standard 18-Pin
  DDRB = 0b00111111;
  PORTB = 0b00100010;
  DDRC = 0b00011111;
  PORTC = 0b00010101;
  DDRD = 0b11100111;

  type = -1;
  // First try standard 18-Pin (with OE test)
  if (ram_present_18Pin()) {
    sense4464_18Pin();
  }

  if (type == -1) {
    // Standard failed, try 411000
    if (ram_present_18Pin_alt()) {
      sense411000_18Pin_Alt();
    }
  }

  if (type == -1) {
    if (ram_present_2114()) {
      type = T_2114;
    }
  }

  if (type == -1) {
    error(0, 0);  // Definitely no RAM
    return;
  }
  writeRAMType((const __FlashStringHelper *)ramTypes[type].name);
  do {
    if (type == T_2114) {
      checkAddressing_2114();
      check_CSWE_2114();
      check_2114();  // March C- at the fixed proven slow strobes (never returns on fail)
      // Speed grade LAST — the chip is functionally good at this point; the tACC probe
      // only decides the displayed "-20"/"-30"/"-45" postfix (see speedProbe_2114).
      g_speedSuffix = speedProbe_2114();
    } else if (type == T_411000) {
      DDRB = (DDRB & 0xE0) | 0x1D;
      DDRC = (DDRC & 0xE0) | 0x17;
      DDRD = (DDRD & 0x18) | 0xE7;
      checkAddressing_18Pin_Alt();
      // Patterns 0-1: checkerboard coupling (write-all -> read-all + hidden refresh).
      pass_18Alt(0, false);
      pass_18Alt(1, false);
      // Patterns 4-5: random WITH retention testing (REAL retention: write row ->
      // controlled aging -> read back, aging row not refreshed).
      for (uint8_t patNr = 4; patNr <= 5; patNr++) {
        if (patNr == 5) invertRandomTable();
        for (uint16_t row = 0; row < 1024; row++)
          writeRow_18Pin_Alt(row, patNr);
      }
      // CBR refresh-counter test AFTER the full regular test (so the success screen is not
      // shown prematurely), loop mode only, every 10th run. pass_18Alt(0,true): re-write +
      // ~60 s CBR (own "CBR:<sec>" screen + LED off) + verify. 411000 is CBR-capable.
      if (CFG_LOOP_ACTIVE && (s_runCount % 10 == 1)) {
        cbrScreenPrep();  // render BEFORE the write phase (25-60 ms zero-refresh)
        pass_18Alt(0, true);
      }
    } else {
      DDRB = 0b00111111;
      PORTB = 0b00100010;
      checkAddressing_18Pin();
      // Patterns 0-1: checkerboard coupling. 4464 = write-all -> read-all with hidden
      // refresh (pass_18Pin); 4416 (no CBR) = LAG-2 pipeline (pass_4416_LAG, 5.0.5 —
      // the write-all form let non-current rows age ~190 ms vs the 4 ms spec).
      if (type == T_4416) {
        pass_4416_LAG(0);
        pass_4416_LAG(1);
      } else {
        pass_18Pin(0, false);
        pass_18Pin(1, false);
      }
      // Patterns 4-5: random WITH retention testing (REAL retention: write row ->
      // controlled aging -> read back, aging row not refreshed).
      uint16_t total_rows = ramTypes[type].rows;
      for (uint8_t patNr = 4; patNr <= 5; patNr++) {
        if (patNr == 5) invertRandomTable();
        for (uint16_t row = 0; row < total_rows; row++)
          writeRow_18Pin(row, patNr);
      }
      // CBR refresh-counter test AFTER the full regular test, loop mode only, every 10th
      // run, 4464 ONLY (4416 has no CBR). pass_18Pin(0,true): re-write + ~60 s CBR + verify.
      if (CFG_LOOP_ACTIVE && (s_runCount % 10 == 1) && (type == T_4464)) {
        cbrScreenPrep();  // render BEFORE the write phase (25-60 ms zero-refresh)
        pass_18Pin(0, true);
      }
    }

    testOK();
  } while (CFG_LOOP_ACTIVE);
}


//=======================================================================================
// RAM PRESENCE TESTS
//=======================================================================================

bool ram_present_18Pin(void) {
  // Configure I/O for standard 18-Pin
  DDRB = 0b00111111;
  PORTB = 0b00100010;
  DDRC = 0b00011111;
  PORTC = 0b00010101;
  DDRD = 0b11100111;

  // Basic write/read test
  rasHandling_18Pin(0);
  configDataOut_18Pin();
  WE_LOW18;
  setData18Pin(0x5);
  setAddr18Pin(0x00);
  CAS_LOW18;
  NOP;
  CAS_HIGH18;
  WE_HIGH18;
  RAS_HIGH18;

  configDataIn_18Pin();
  OE_LOW18;
  rasHandling_18Pin(0);
  setAddr18Pin(0x00);
  CAS_LOW18;
  NOP;
  NOP;
  uint8_t result = (getData18Pin() & 0xF);
  CAS_HIGH18;
  OE_HIGH18;
  RAS_HIGH18;

  // Ports will be reconfigured by caller
  return (result == 0x5);
}

bool ram_present_18Pin_alt(void) {
  DDRB = (DDRB & 0xE0) | 0x1D;
  DDRC = (DDRC & 0xE0) | 0x17;
  DDRD = (DDRD & 0x18) | 0xE7;
  PORTC |= 0x08;  // DOUT (PC3) pull-up: an empty socket then reads 1, not a
                  // floating 0 (write-0/read-0 would otherwise false-positive
                  // and run checkAddressing_18Pin_Alt -> bogus address error).

  RAS_HIGH_18PIN_ALT;
  CAS_HIGH_18PIN_ALT;
  WE_HIGH_18PIN_ALT;

  // Double write/read test
  setAddr_18Pin_Alt(0);
  RAS_LOW_18PIN_ALT;
  SET_DIN_18PIN_ALT(0);
  WE_LOW_18PIN_ALT;
  CAS_LOW_18PIN_ALT;
  NOP;
  CAS_HIGH_18PIN_ALT;
  WE_HIGH_18PIN_ALT;
  RAS_HIGH_18PIN_ALT;

  setAddr_18Pin_Alt(0);
  RAS_LOW_18PIN_ALT;
  CAS_LOW_18PIN_ALT;
  NOP;
  NOP;
  uint8_t test1 = GET_DOUT_18PIN_ALT();
  CAS_HIGH_18PIN_ALT;
  RAS_HIGH_18PIN_ALT;

  // Ports will be reconfigured by caller
  return (test1 == 0);
}

/**
 * Detect presence of a 2114 SRAM in the 18-pin socket
 *
 * Performs a minimal write/read round-trip at address 0 with values 0x0 and
 * 0xF. Both values must read back correctly to confirm presence. Only the
 * IO lines, _CS and _WE are exercised — full address verification is left
 * to checkAddressing_2114.
 *
 * @return true if a 2114 responds correctly, false otherwise
 */
bool ram_present_2114(void) {
  sram2114_init();
  sram2114_write(0, 0);
  if (sram2114_read(0) == 0) {
    sram2114_write(0, 0xF);
    if (sram2114_read(0) == 0xF)
      return true;
  }
  return false;
}

//=======================================================================================
// STANDARD 18-PIN DRAM FUNCTIONS (4416/4464)
//=======================================================================================

// Small hot helpers keep code compact and readable
// Real (noinline) functions since 5.0.5: the data/address bit-scatter body exists ONCE
// and the ~5 call sites (random rows, detection, 4416 LAG pass) share it — always_inline
// overflowed flash when pass_4416_LAG was added. Cost: ~0.5 us call overhead per cell,
// folded into the 4416/4464 retune.
static void __attribute__((noinline)) col_write(uint8_t col, uint8_t data) {
  setData18Pin(data);
  setAddr18Pin(col);
  CAS_LOW18;
  NOP;  // tCL guard — 3 NOPs give a DETERMINISTIC ~312 ns CAS-low width (Mi-4): the
  NOP;  // old single NOP guaranteed only ~187 ns and the measured 437 ns came from
  NOP;  // compiler scheduling luck; the slowest 4416 grade needs tCAS >= 200 ns.
  CAS_HIGH18;
}

static uint8_t __attribute__((noinline)) col_read(uint8_t col) {
  setAddr18Pin(col);
  CAS_LOW18;
  NOP;
  NOP;  // tCAC / data valid
  uint8_t d = getData18Pin() & 0x0F;
  CAS_HIGH18;
  return d;
}

/**
 * Write 4-bit nibble to a specific row/column address
 *
 * @param row Row address to write to
 * @param col Column address to write to
 * @param data 4-bit data nibble to write (0x0-0xF)
 */
static inline void write18Pin(uint16_t row, uint8_t col, uint8_t data) {
  rasHandling_18Pin(row);
  col_write(col, data);
  RAS_HIGH18;
}

/**
 * Read 4-bit nibble from a specific row/column address
 *
 * @param row Row address to read from
 * @param col Column address to read from
 * @return 4-bit data nibble read (0x0-0xF)
 */
static inline uint8_t read18Pin(uint16_t row, uint8_t col) {
  rasHandling_18Pin(row);
  uint8_t result = col_read(col);
  RAS_HIGH18;
  return result;
}

void checkAddressing_18Pin() {
  // PORTD mask 0xE7 = the socket's actual PORTD pins (PD0-2, PD5-7; same set the 2114
  // ADDR_MASK_D and the Alt DDRD masks use). The former 0xFC drove PD3 — the 18-pin
  // DIP-switch input, railed to 5 V in this mode -> ~65 mA through the pin (abs-max
  // 40 mA) — plus PD4 (NC), while leaving PD0/PD1 untested.
  checkAddressShorts(0x14, 0x00, 0xE7);

  // Restore I/O direction: checkAddressShorts leaves address pins as inputs with pull-ups
  DDRB = 0b00111111;
  PORTB = 0b00100010;  // RAS=HIGH
  DDRD = 0b11100111;
  PORTD = 0x00;

  // Derive bit counts (values mirror ramTypes[] but inlined to avoid the costly
  // ramTypes[type] array-of-struct resolve in this no-delays function)
  uint16_t rows = 256;                          // 4416 & 4464 both 256 rows
  uint16_t cols = (type == T_4416) ? 64 : 256;  // 4416=64, 4464=256
  uint8_t rowBits = countBits(rows - 1);
  uint8_t colBits = countBits(cols - 1);

  const uint8_t is4416 = (type == T_4416);
  const uint8_t cshift = is4416 ? 1 : 0;         // 4416: columns on A1..A6
  const uint8_t safeCol = is4416 ? 0x02 : 0x00;  // fix column used for row-tests
  const uint16_t testRow = rows >> 1;            // mid row for column-tests

  // ---------------- Row Address Tests ----------------
  configDataOut_18Pin();
  WE_LOW18;

  // Write test pattern to all row address bits
  for (uint8_t b = 0; b < rowBits; b++) {
    write18Pin(0, safeCol, 0x5);          // Base row: all bits = 0
    NOP;                                  // tiny guard for conservative devices (≈ 125 ns)
    write18Pin((1U << b), safeCol, 0xA);  // Peer row: only bit 'b' = 1
    NOP;                                  // tiny guard
  }

  // Switch to read mode
  WE_HIGH18;
  configDataIn_18Pin();
  OE_LOW18;

  // Verify all row address bits
  for (uint8_t b = 0; b < rowBits; b++) {
    if (read18Pin(0, safeCol) != 0x5) error(b, 1);          // Base row
    if (read18Pin((1U << b), safeCol) != 0xA) error(b, 1);  // Peer row
  }

  OE_HIGH18;

  // ---------------- Column Address Tests ----------------
  configDataOut_18Pin();
  WE_LOW18;

  // Write test pattern to all column address bits
  for (uint8_t b = 0; b < colBits; b++) {
    rasHandling_18Pin(testRow);
    col_write(0 << cshift, 0x5);          // Base column: all bits = 0
    NOP;                                  // CAS-high time between two column ops
    col_write((1U << b) << cshift, 0xA);  // Peer column: only bit 'b' = 1
    RAS_HIGH18;
    NOP;  // tiny guard
  }

  // Switch to read mode
  WE_HIGH18;
  configDataIn_18Pin();
  OE_LOW18;

  // Verify all column address bits
  for (uint8_t b = 0; b < colBits; b++) {
    rasHandling_18Pin(testRow);
    if (col_read(0 << cshift) != 0x5) {
      RAS_HIGH18;
      error(b + 16, 1);
    }
    NOP;  // CAS-high time between two column reads
    if (col_read((1U << b) << cshift) != 0xA) {
      RAS_HIGH18;
      error(b + 16, 1);
    }
    RAS_HIGH18;
  }

  OE_HIGH18;
}


/**
 * Optimized 18-Pin DRAM Detection (streamlined)
 */
/**
 * Write and read test pattern for chip sensing
 * @param row Row address
 * @param addr Column address for write
 * @param data Data value to write
 * @return Data value read back from address 0
 */
static inline uint8_t sense_write_read_18Pin(uint16_t row, uint8_t addr, uint8_t data) {
  rasHandling_18Pin(row);
  configDataOut_18Pin();
  WE_LOW18;
  setData18Pin(data);
  setAddr18Pin(addr);
  CAS_LOW18;
  NOP;
  CAS_HIGH18;
  setAddr18Pin(0x00);
  WE_HIGH18;
  configDataIn_18Pin();
  OE_LOW18;
  CAS_LOW18;
  NOP;
  NOP;
  uint8_t result = getData18Pin() & 0xF;
  CAS_HIGH18;
  OE_HIGH18;
  return result;
}

void sense4464_18Pin() {
  // RAM check - initial presence test
  rasHandling_18Pin(0);
  configDataOut_18Pin();
  WE_LOW18;
  setData18Pin(0x5);
  setAddr18Pin(0x00);
  CAS_LOW18;
  NOP;
  CAS_HIGH18;
  WE_HIGH18;
  RAS_HIGH18;

  configDataIn_18Pin();
  OE_LOW18;
  rasHandling_18Pin(0);
  setAddr18Pin(0x00);
  CAS_LOW18;
  NOP;
  NOP;
  if ((getData18Pin() & 0xF) == 0xF) {
    type = -1;
    return;
  }
  CAS_HIGH18;
  OE_HIGH18;

  // Simplified 4416/4464 logic - Test A0 and A7
  rasHandling_18Pin(0);
  configDataOut_18Pin();
  WE_LOW18;
  setData18Pin(0x0);
  setAddr18Pin(0x00);
  CAS_LOW18;
  NOP;
  CAS_HIGH18;
  WE_HIGH18;
  RAS_HIGH18;

  // Test A0 (most important) - write 0xF to address 0x01, read from 0x00
  uint8_t a0_test = sense_write_read_18Pin(0, 0x01, 0xF);

  if (a0_test != 0x0) {
    type = T_4416;  // A0 aliased
  } else {
    // Test A7 for confirmation - write 0xF to address 0x80, read from 0x00
    uint8_t a7_test = sense_write_read_18Pin(0, 0x80, 0xF);
    type = (a7_test != 0x0) ? T_4416 : T_4464;
  }

  RAS_HIGH18;
}

/**
 * Configure data lines as outputs for write operations
 */
void configDataOut_18Pin() {
  DDRB |= 0x09;  // Configure D1 & D2 as Outputs
  DDRC |= 0x0a;  // Configure D0 & D3 as Outputs
}

/**
 * Configure data lines as inputs for read operations
 */
void configDataIn_18Pin() {
  DDRB &= 0xf6;  // Config Data Lines for input
  DDRC &= 0xf5;
  PORTB |= 0x09;  // Activate the Pullups, otherwise static can keep the lines
  PORTC |= 0x0A;  // Causing false positives. However this is the limit of the 4416 Output driver
}

/**
 * Set row address and activate RAS signal for standard 18-pin DRAMs
 * @param row Row address to access
 */
void rasHandling_18Pin(uint8_t row) {
  RAS_HIGH18;
  setAddr18Pin(row);
  RAS_LOW18;
}

// RAS recycle WITH a hidden refresh, std 18-pin (4416/4464). Entry: CAS LOW.
// Toggles RAS while CAS stays low (CBR runs "hidden", no extra CAS edge), then
// re-opens the current row. The WRITE phase enters CAS-low (real hidden refresh);
// the READ phase samples after CAS rises, so it lowers CAS first (ordinary CBR).
static void __attribute__((noinline)) rasHandlingHidden_18Pin(uint8_t row) {
  RAS_HIGH18;  // precharge the active row (refresh it); CAS stays LOW
  NOP;
  RAS_LOW18;  // RAS down with CAS low -> CBR (hidden) refresh
  NOP;
  NOP;
  RAS_HIGH18;         // end the refresh
  CAS_HIGH18;         // finish the cycle
  setAddr18Pin(row);  // re-address the current row
  RAS_LOW18;          // re-open it
}

// Checkerboard pass for the 4464 ONLY (4-bit, CBR-capable): write-all -> read-all so
// inter-row coupling is caught; a hidden refresh (write) / CBR (read) after EVERY
// column keeps the counter sweep inside the refresh budget and bounds tRAS. The 4416
// (no CBR) runs the LAG pipeline in pass_4416_LAG below instead (5.0.5). No quadrant.
//   patNr 0/1 = checkerboard nibble ((row^col)&1)^inv ? 0x5 : 0xA
static void __attribute__((hot)) pass_18Pin(uint8_t patNr, bool runCBR) {
  uint8_t inv = patNr & 1;

  // ---------------- WRITE ALL ROWS ----------------
  configDataOut_18Pin();
  cli();
  for (uint16_t row = 0; row < 256; row++) {
    rasHandling_18Pin(row);
    WE_LOW18;
    for (uint16_t col = 0; col < 256; col++) {
      uint8_t d = (((row ^ col) & 1) ^ inv) ? 0x05 : 0x0A;
      CAS_HIGH18;
      setData18Pin(d);
      setAddr18Pin(col);
      CAS_LOW18;
      // CAS low -> hidden refresh (WE high so the CBR cannot write)
      WE_HIGH18;
      rasHandlingHidden_18Pin(row);
      WE_LOW18;
    }
    WE_HIGH18;
    RAS_HIGH18;
  }

  // CBR refresh-COUNTER test (~60 s, loop mode, 4464 only — 4416 never sets runCBR):
  // refresh the just-written array via CAS-before-RAS only, then the read-all below
  // verifies it survived. Shared helper: millis-bounded 60 s + LED off + countdown.
  if (runCBR) cbrRefreshPhase(&casBeforeRasRefresh_18Pin);

  // ---------------- READ ALL ROWS ----------------
  configDataIn_18Pin();
  for (uint16_t row = 0; row < 256; row++) {
    rasHandling_18Pin(row);
    OE_LOW18;
    for (uint16_t col = 0; col < 256; col++) {
      uint8_t d = (((row ^ col) & 1) ^ inv) ? 0x05 : 0x0A;
      setAddr18Pin(col);
      CAS_LOW18;
      NOP;
      CAS_HIGH18;
      if (getData18Pin() != d) {
        sei();
        OE_HIGH18;
        RAS_HIGH18;
        error(patNr, runCBR ? 5 : 2);  // 5 = "CBR Timer fault" (CBR run), 2 = checkerboard
      }
      // read burst ends CAS-high -> lower CAS, then CBR
      CAS_LOW18;
      rasHandlingHidden_18Pin(row);
    }
    OE_HIGH18;
    RAS_HIGH18;
  }
  sei();
}

// 4416 checkerboard (patterns 0-1) via the 2-row write/read LAG pipeline — mirrors
// pass_4116 / checkerQuadrant_16Pin (both HW-validated). The 4416 has NO CBR, so the
// former write-all -> read-all pass let non-current rows age ~190 ms vs the 4 ms spec
// (room-temperature reliance). With LAG=2 the victim row is read right after BOTH
// vertical neighbours were written -> inter-row coupling kept, write->read window
// ~2 row times (~1.3 ms) < 4 ms -> in-spec WITHOUT extra refresh. Data identical to
// the old pass; cells via col_write/col_read + RAS-only recycle every column (tRAS
// 10 us). cli per row half (millis keeps ticking); rows close BEFORE sei.
static void pass_4416_LAG(uint8_t patNr) {
  uint8_t inv = patNr & 1;
  bool ascending = (patNr == 0);  // pass 0 up, pass 1 down (order-dependent coupling)
  const uint8_t LAG = 2;
  for (uint16_t i = 0; i < 256 + LAG; i++) {
    // ---------- WRITE row_w (while rows remain) ----------
    if (i < 256) {
      uint8_t row_w = ascending ? (uint8_t)i : (uint8_t)(255 - i);
      configDataOut_18Pin();
      rasHandling_18Pin(row_w);
      WE_LOW18;
      cli();
      for (uint8_t col = 0; col < 64; col++) {
        uint8_t d = (((row_w ^ col) & 1) ^ inv) ? 0x05 : 0x0A;
        col_write((uint8_t)(col << 1), d);  // 4416 columns sit on A1..A6
        rasHandling_18Pin(row_w);           // RAS-only recycle every col (no CBR)
      }
      WE_HIGH18;
      RAS_HIGH18;  // close the row BEFORE sei (pending-ISR discipline)
      sei();
    }
    // ---------- READ row_r (trails write by LAG) ----------
    if (i >= LAG) {
      uint8_t jr = (uint8_t)(i - LAG);
      uint8_t row_r = ascending ? jr : (uint8_t)(255 - jr);
      configDataIn_18Pin();
      OE_LOW18;
      rasHandling_18Pin(row_r);
      cli();
      for (uint8_t col = 0; col < 64; col++) {
        uint8_t d = (((row_r ^ col) & 1) ^ inv) ? 0x05 : 0x0A;
        if (col_read((uint8_t)(col << 1)) != d) {
          OE_HIGH18;
          RAS_HIGH18;
          sei();
          error(patNr, 2);
        }
        rasHandling_18Pin(row_r);           // RAS-only recycle every col (no CBR)
      }
      OE_HIGH18;
      RAS_HIGH18;  // close the row BEFORE sei (pending-ISR discipline)
      sei();
    }
  }
}

// ---- std 18-pin (4416/4464) random + retention pipeline (patterns 4-5) ----
// The checkerboard coupling (0-1) runs through pass_18Pin; THESE provide the real
// retention test: write a row, then read it back delayRows rows later (after the
// controlled aging delay) WITHOUT refreshing it in between. tRAS guard: BOTH types
// recycle RAS every column (NMOS tRAS max = 10,000 ns, no page-mode allowance) —
// the 4416 via the full rasHandling_18Pin (calibrated), the 4464 via the cheap
// inline row-port-snapshot restore (window ~7.6 us). Functionally as in pre-5.0.

void refreshRow_18Pin(uint16_t row) {
  rasHandling_18Pin(row);  // entry CAS HIGH -> RAS-only refresh of this row (not CBR)
  NOP;
  RAS_HIGH18;
}

void checkRow_18Pin(uint16_t row, uint8_t patNr) {
  uint8_t init_shift = (type == T_4416) ? 1 : 0;
  uint16_t width = (type == T_4416) ? 64 : 256;
  uint8_t is4416 = (type == T_4416);
  configDataIn_18Pin();
  OE_LOW18;
  rasHandling_18Pin(row);
  // Row-port snapshot for the 4464 inline tRAS recycle (see writeRow_18Pin): the
  // KM41464A specs tRAS max = 10,000 ns with NO page-mode allowance (NMOS gen), so
  // RAS must recycle EVERY column; restoring the snapshot (~0.4 us) instead of the
  // full setAddr18Pin keeps the RAS-low window at ~7.6 us (was 51.6/52.1 us @ 8 cols).
  uint8_t row_b = PORTB, row_d = PORTD;
  cli();
  for (uint16_t col = 0; col < width; col++) {
    if (col_read((uint8_t)(col << init_shift)) != randomTable[mix8(col, row)]) {
      sei();
      OE_HIGH18;
      RAS_HIGH18;
      error(patNr, 3);
    }
    if (is4416) {
      rasHandling_18Pin(row);  // 4416: unchanged full recycle (calibration preserved)
    } else {
      RAS_HIGH18;    // 4464: inline row-snapshot recycle (RAS-only; CAS high after read)
      PORTB = row_b; // ~250 ns precharge while the row address ports are restored
      PORTD = row_d;
      RAS_LOW18;
    }
  }
  OE_HIGH18;
  RAS_HIGH18;  // close the row BEFORE sei(): a pending timer ISR (blocked across the
  sei();       // cli row) fires right at sei and would stretch the open RAS window ~7 us
}

void writeRow_18Pin(uint16_t row, uint8_t patNr) {
  uint8_t init_shift = (type == T_4416) ? 1 : 0;
  uint16_t width = (type == T_4416) ? 64 : 256;
  uint8_t is4416 = (type == T_4416);
  const RAM_Definition *rt = &ramTypes[type];
  uint16_t last_row = rt->rows - 1;

  rasHandling_18Pin(row);
  WE_LOW18;
  configDataOut_18Pin();
  // Row-port snapshot for the 4464 inline tRAS recycle: taken AFTER WE low + data-out
  // config so a restore reproduces the full row-open state (stale data bits on PB0/PB3
  // are don't-care — CAS is high during the recycle). KM41464A: tRAS max 10,000 ns, no
  // page-mode allowance -> recycle EVERY column; snapshot restore keeps the window at
  // ~7.6 us (was 51.6 us @ 8 cols = 5.2x over spec). 4416 keeps its calibrated path.
  uint8_t row_b = PORTB, row_d = PORTD;
  cli();
  for (uint16_t col = 0; col < width; col++) {
    col_write((uint8_t)(col << init_shift), randomTable[mix8(col, row)]);
    if (is4416) {
      rasHandling_18Pin(row);  // 4416: unchanged full recycle (calibration preserved)
    } else {
      RAS_HIGH18;    // 4464: inline row-snapshot recycle (RAS-only; CAS high, WE low)
      PORTB = row_b; // ~250 ns precharge while the row address ports are restored
      PORTD = row_d;
      RAS_LOW18;
    }
  }
  WE_HIGH18;
  RAS_HIGH18;  // close the row BEFORE sei() (pending-ISR stretch — see checkRow_18Pin)
  sei();

  // Retention aging + pipelined read-back via the shared tail (common.cpp).
  refreshRow_18Pin(row);
  retentionTail(row, last_row, patNr, checkRow_18Pin);
}

//=======================================================================================
// 18-PIN ALTERNATIVE DRAM FUNCTIONS (KM41C1000 type)
//=======================================================================================

/**
 * Optimized 18-Pin Alternative Detection (KM41C1000)
 */
/**
 * Write and read helper for Alt pin configuration
 * @param addr Address to write to and read from
 * @param data Data value to write
 * @return Data value read back
 */
static inline uint8_t write_read_alt_18Pin(uint8_t addr, uint8_t data) {
  setAddr_18Pin_Alt(addr);
  RAS_LOW_18PIN_ALT;
  SET_DIN_18PIN_ALT(data);
  WE_LOW_18PIN_ALT;
  CAS_LOW_18PIN_ALT;
  NOP;
  CAS_HIGH_18PIN_ALT;
  WE_HIGH_18PIN_ALT;
  RAS_HIGH_18PIN_ALT;

  setAddr_18Pin_Alt(addr);
  RAS_LOW_18PIN_ALT;
  CAS_LOW_18PIN_ALT;
  NOP;
  NOP;
  uint8_t result = GET_DOUT_18PIN_ALT();
  CAS_HIGH_18PIN_ALT;
  RAS_HIGH_18PIN_ALT;
  return result;
}

/**
 * Read-only helper for Alt pin configuration
 * @param addr Address to read from
 * @return Data value read
 */
static inline uint8_t read_alt_18Pin(uint8_t addr) {
  setAddr_18Pin_Alt(addr);
  RAS_LOW_18PIN_ALT;
  CAS_LOW_18PIN_ALT;
  NOP;
  NOP;
  uint8_t result = GET_DOUT_18PIN_ALT();
  CAS_HIGH_18PIN_ALT;
  RAS_HIGH_18PIN_ALT;
  return result;
}

void sense411000_18Pin_Alt() {
  DDRB = (DDRB & 0xE0) | 0x1D;
  DDRC = (DDRC & 0xE0) | 0x17;
  DDRD = (DDRD & 0x18) | 0xE7;

  RAS_HIGH_18PIN_ALT;
  CAS_HIGH_18PIN_ALT;
  WE_HIGH_18PIN_ALT;

  // Write 0 to address 0, read it back
  uint8_t test1 = write_read_alt_18Pin(0, 0);

  // Write 1 to address 1, read it back
  uint8_t test2 = write_read_alt_18Pin(1, 1);

  // Cross-check: Address 0 should still be 0
  uint8_t test3 = read_alt_18Pin(0);

  // All tests must be correct
  if (test1 == 0 && test2 != 0 && test3 == 0) {
    type = T_411000;
  }
  // Note: Don't call error() here - let test_18Pin() handle it if type remains -1
}

/**
 * Write a single bit to a specific row/column address (Alt configuration)
 *
 * @param row Row address to write to
 * @param col Column address to write to
 * @param data Data bit to write (0 or 1)
 */
static inline void write_alt_18Pin(uint16_t row, uint16_t col, uint8_t data) {
  setAddr_18Pin_Alt(row);
  RAS_LOW_18PIN_ALT;
  SET_DIN_18PIN_ALT(data);
  setAddr_18Pin_Alt(col);
  WE_LOW_18PIN_ALT;
  CAS_LOW_18PIN_ALT;
  NOP;
  CAS_HIGH_18PIN_ALT;
  WE_HIGH_18PIN_ALT;
  RAS_HIGH_18PIN_ALT;
}

/**
 * Read a single bit from a specific row/column address (Alt configuration)
 *
 * @param row Row address to read from
 * @param col Column address to read from
 * @return Data bit read (0 or 1)
 */
static inline uint8_t read_addr_alt_18Pin(uint16_t row, uint16_t col) {
  setAddr_18Pin_Alt(row);
  RAS_LOW_18PIN_ALT;
  setAddr_18Pin_Alt(col);
  CAS_LOW_18PIN_ALT;
  NOP;
  NOP;
  uint8_t result = GET_DOUT_18PIN_ALT();
  CAS_HIGH_18PIN_ALT;
  RAS_HIGH_18PIN_ALT;
  return result;
}

void checkAddressing_18Pin_Alt() {
  // PORTD mask 0xE7 (socket pins PD0-2, PD5-7 — matches the DDRD restore below). The
  // former 0x7F drove PD3 (18-pin DIP input, railed 5 V -> ~65 mA over abs-max) and
  // PD4 (NC), and left PD7 (alt A6) untested.
  checkAddressShorts(0x15, 0x00, 0xE7);

  // Restore I/O direction: checkAddressShorts leaves address pins as inputs with pull-ups
  DDRB = (DDRB & 0xE0) | 0x1D;
  DDRC = (DDRC & 0xE0) | 0x17;
  DDRD = (DDRD & 0x18) | 0xE7;
  PORTD = 0x00;

  uint16_t rows = 1024;  // 411000: 1Mx1 = 1024 rows × 1024 cols (mirrors ramTypes[])
  uint16_t cols = 1024;
  uint8_t rowBits = countBits(rows - 1);
  uint8_t colBits = countBits(cols - 1);

  // Row address test
  for (uint8_t b = 0; b < rowBits; b++) {
    write_alt_18Pin(0, 0, 0);          // Base row: all bits = 0
    write_alt_18Pin((1U << b), 0, 1);  // Peer row: only bit 'b' = 1

    if (read_addr_alt_18Pin(0, 0) != 0) error(b, 1);
    if (read_addr_alt_18Pin((1U << b), 0) != 1) error(b, 1);
  }

  // Column address test
  for (uint8_t b = 0; b < colBits; b++) {
    setAddr_18Pin_Alt(0);
    RAS_LOW_18PIN_ALT;

    SET_DIN_18PIN_ALT(0);
    setAddr_18Pin_Alt(0);
    WE_LOW_18PIN_ALT;
    CAS_LOW_18PIN_ALT;
    NOP;
    CAS_HIGH_18PIN_ALT;

    SET_DIN_18PIN_ALT(1);
    setAddr_18Pin_Alt((1U << b));
    WE_LOW_18PIN_ALT;
    CAS_LOW_18PIN_ALT;
    NOP;
    CAS_HIGH_18PIN_ALT;

    WE_HIGH_18PIN_ALT;
    RAS_HIGH_18PIN_ALT;

    // Verify
    setAddr_18Pin_Alt(0);
    RAS_LOW_18PIN_ALT;

    setAddr_18Pin_Alt(0);
    CAS_LOW_18PIN_ALT;
    NOP;
    NOP;
    if (GET_DOUT_18PIN_ALT() != 0) error(b + 16, 1);
    CAS_HIGH_18PIN_ALT;

    setAddr_18Pin_Alt((1U << b));
    CAS_LOW_18PIN_ALT;
    NOP;
    NOP;
    if (GET_DOUT_18PIN_ALT() != 1) error(b + 16, 1);
    CAS_HIGH_18PIN_ALT;

    RAS_HIGH_18PIN_ALT;
  }
}

/**
 * Build PORTD lookup table for fast column iteration (A1-A6)
 * Marked noinline to emit only one copy (called from 3 functions).
 */
static void __attribute__((noinline)) buildPdLut_18Alt(uint8_t *pd_lut) {
  uint8_t pd_base = PORTD & 0x18;  // Preserve PD3, PD4
  for (uint8_t i = 0; i < 64; i++) {
    pd_lut[i] = pd_base | (i & 0x07) | ((i & 0x38) << 2);
  }
}

//=======================================================================================
// CHECKERBOARD COUPLING TEST for 411000 (replaces former constant patterns 0-3)
//=======================================================================================
//
// The previous fast path wrote the ENTIRE 1Mx1 array, then read it all back, so
// the first-written row stayed unrefreshed for the whole write phase (~0.5 s) —
// ~60x over the 8 ms refresh spec, a latent source of FALSE failures on weak
// cells or at temperature. This version bounds the write->read window with a
// 1-row pipeline lag (write row N, read row N∓1) and NO retention delays, while
// adding spatial coupling coverage via a checkerboard background.
//
// Background per cell:  bit = ((row ^ col) & 1).
//   For 411000 the column LSB A0 == the outer PORTC loop index (pc_i), so the
//   bit depends only on (row & 1) ^ pc_i — no per-inner-column computation.
//   Two passes (normal + inverted) give every cell both 0 and 1; adjacent cells
//   always hold opposite values (row/column coupling).
//
// Two passes for row-order coverage:
//   Pass 0: ascending  write, read lag N-1, normal background.
//   Pass 1: descending write, read lag N+1, inverted background.
//
// Port-ordered iteration preserved (Outer PORTC/A0 x2, Middle PORTB/A7-9 x8,
// Inner PORTD/A1-6 x64 = 1024 columns) — one port write per CAS in the hot loop.
//
// KNOWN LIMITATION (out of scope): pure transition / idempotent coupling faults
// need per-cell read-modify-write, too costly in page mode.
//=======================================================================================

// tRAS-max guard for 411000 (page mode allows ~100 us RAS-low):
//  - Random/retention path: ~5 us/CAS MEASURED (the A1-A9 bit-scatter in
//    setAddr_18Pin_Alt dominates) -> RAS-only recycle every RAS_BURST_ALT_RND = 16
//    cols (~82 us). Power of two (recycle uses a `col & (BURST-1)` mask).
//  - Checkerboard (pass_18Alt): port-ordered cells (~1 us) -> full 64-col bursts
//    (~55-70 us) with an 8-CBR catch-up batch after each burst (cbrCatchup_18Alt).
#define RAS_BURST_ALT_RND 16  // random/retention (writeRow/checkRow_18Pin_Alt, FPM RAS-only)

// Catch-up CBR burst for the 411000 checkerboard. Entry: CAS LOW (hidden). Precharges
// the row, then runs EIGHT back-to-back CBR cycles while CAS stays low ("hidden", no
// extra CAS edges), then re-opens the current row and restores the column PORTC
// (A0/control/WE/DIN) + PORTB (A7-A9). Refresh average stays 1 CBR per 8 cols
// (15.625 us budget, 8 ms/512), but the expensive row-reopen (setAddr call) is paid
// once per 64-col burst instead of 8x. The 64-col burst (~55-70 us RAS-low) stays
// under the ~100 us page-mode RAS limit. noinline so the body exists once.
static void __attribute__((noinline)) cbrCatchup_18Alt(uint16_t row, uint8_t pc, uint8_t pb) {
  RAS_HIGH_18PIN_ALT;  // precharge active row (refresh it); CAS stays LOW
  for (uint8_t i = 8; i; i--) {
    NOP;                 // tRP
    RAS_LOW_18PIN_ALT;   // RAS down with CAS low -> CBR (hidden) refresh
    NOP;
    NOP;
    RAS_HIGH_18PIN_ALT;  // end this CBR (precharge)
  }
  CAS_HIGH_18PIN_ALT;      // finish the cycle
  setAddr_18Pin_Alt(row);  // re-open the current row
  RAS_LOW_18PIN_ALT;
  PORTC = pc;  // restore column PORTC (A0/control/WE/DIN)
  PORTB = pb;  // restore A7-A9
}

// Unified test pass for the 411000 (18-pin Alt, 1Mx1, 1-bit). Replaces the former
// checkerWriteRow/checkerReadRow/checkerboardTest_18Alt + writeRow/checkRow_18Pin_Alt.
// write-all -> read-all (inter-row coupling); 64-col bursts with an 8-CBR catch-up
// batch after each burst — the CBR counter sweep keeps every row alive across the
// sweeps (1 CBR per 8 cols average = 15.625 us budget) and bounds tRAS. No quadrant.
//   patNr 0/1 = checkerboard ((row^inv)^A0) ; 4/5 = pseudo-random (5 = inverted)
// Cell layout per row: A0 (pc_i) x A7-A9 (pb_i, pb_col table) x A1-A6 (pd_lut).
static void __attribute__((hot)) pass_18Alt(uint8_t patNr, bool runCBR) {
  uint8_t pd_lut[64];
  buildPdLut_18Alt(pd_lut);
  uint8_t inv = patNr & 1;
  uint8_t pc_upper = PORTC & 0xE0;

  // ---------------- WRITE ALL ROWS ----------------
  uint8_t pc_w0 = pc_upper | 0x04;         // A0=0, CAS=HIGH, WE=LOW
  uint8_t pc_w1 = pc_upper | 0x04 | 0x10;  // A0=1
  cli();
  for (uint16_t row = 0; row < 1024; row++) {
    uint8_t rp = (uint8_t)(row ^ inv) & 1;
    RAS_HIGH_18PIN_ALT;
    setAddr_18Pin_Alt(row);
    RAS_LOW_18PIN_ALT;
    WE_LOW_18PIN_ALT;
    uint8_t pb_base = PORTB & 0xEA;
    for (uint8_t pc_i = 0; pc_i < 2; pc_i++) {
      uint8_t pc_val = pc_i ? pc_w1 : pc_w0;
      uint8_t check_din = rp ^ pc_i;  // checkerboard DIN bit for this A0
      for (uint8_t pb_i = 0; pb_i < 8; pb_i++) {
        uint8_t pb_val = pb_base | pgm_read_byte(&pb_addr_18Alt[pb_i]);
        PORTB = pb_val;
        // Full 64-col burst (~55-60 us RAS-low < ~100 us page limit), then ONE batch
        // of 8 catch-up CBRs: refresh average unchanged (1 CBR per 8 cols), but the
        // row-reopen is paid once per burst instead of 8x.
        for (uint8_t pd_i = 0; pd_i < 64; pd_i++) {
          uint8_t din = check_din;
          CAS_HIGH_18PIN_ALT;
          PORTD = pd_lut[pd_i];
          PORTC = din ? (pc_val | 0x01) : pc_val;  // DIN on PC0
          CAS_LOW_18PIN_ALT;
        }
        // CAS low -> hidden refresh batch (WE high during the CBRs to avoid WCBR)
        PORTC |= 0x02;
        cbrCatchup_18Alt(row, pc_val, pb_val);
      }
    }
    WE_HIGH_18PIN_ALT;
    RAS_HIGH_18PIN_ALT;
  }

  // CBR refresh-COUNTER test (~60 s, loop mode): refresh the just-written array via
  // CAS-before-RAS only, then the read-all below verifies it survived. Shared helper:
  // millis-bounded 60 s + LED off + "R:<sec>" countdown.
  if (runCBR) cbrRefreshPhase(&casBeforeRasRefresh_18Pin_Alt);

  // ---------------- READ ALL ROWS ----------------
  uint8_t pc_r0 = pc_upper | 0x04 | 0x02;         // A0=0, WE=HIGH
  uint8_t pc_r1 = pc_upper | 0x04 | 0x02 | 0x10;  // A0=1
  for (uint16_t row = 0; row < 1024; row++) {
    uint8_t rp = (uint8_t)(row ^ inv) & 1;
    RAS_HIGH_18PIN_ALT;
    setAddr_18Pin_Alt(row);
    RAS_LOW_18PIN_ALT;
    uint8_t pb_base = PORTB & 0xEA;
    for (uint8_t pc_i = 0; pc_i < 2; pc_i++) {
      uint8_t pc_val = pc_i ? pc_r1 : pc_r0;
      uint8_t check_exp = (rp ^ pc_i) ? 0x08 : 0x00;
      for (uint8_t pb_i = 0; pb_i < 8; pb_i++) {
        uint8_t pb_val = pb_base | pgm_read_byte(&pb_addr_18Alt[pb_i]);
        PORTB = pb_val;
        PORTC = pc_val;
        // Full 64-col burst (~64-70 us RAS-low < ~100 us page limit) + ONE batch of
        // 8 catch-up CBRs (see write phase).
        for (uint8_t pd_i = 0; pd_i < 64; pd_i++) {
          uint8_t exp = check_exp;
          PORTD = pd_lut[pd_i];
          CAS_LOW_18PIN_ALT;
          NOP;
          CAS_HIGH_18PIN_ALT;
          if (((PINC ^ exp) & 0x08) != 0) {
            sei();
            RAS_HIGH_18PIN_ALT;
            error(patNr, runCBR ? 5 : 2);  // 5 = "CBR Timer fault" (CBR run), 2 = checkerboard
            return;
          }
        }
        // read burst ends CAS-high -> lower CAS, then the hidden CBR batch
        CAS_LOW_18PIN_ALT;
        cbrCatchup_18Alt(row, pc_val, pb_val);
      }
    }
    RAS_HIGH_18PIN_ALT;
  }
  sei();
}

/**
 * Set row address and activate RAS signal for 18Pin_Alt
 * @param row Row address to access
 */
void rasHandling_18Pin_Alt(uint16_t row) {
  RAS_HIGH_18PIN_ALT;
  setAddr_18Pin_Alt(row);
  RAS_LOW_18PIN_ALT;
}

// ---- 411000 (18-pin Alt, 1Mx1) random + retention pipeline (patterns 4-5) ----
// Checkerboard coupling (0-1) runs through pass_18Alt; THESE provide the real
// retention test: write a row, read it back delayRows rows later (after aging) without
// refreshing it. Streamlined vs pass_18Alt: plain setAddr_18Pin_Alt() per column (no
// port-order LUTs) + a RAS-only recycle every RAS_BURST_ALT cols to bound tRAS.
// Data bit = bit 3 of randomTable (DIN=PC0 on write, DOUT=PC3 on read).

void refreshRow_18Pin_Alt(uint16_t row) {
  rasHandling_18Pin_Alt(row);  // entry CAS HIGH -> RAS-only refresh (not CBR)
  NOP;
  RAS_HIGH_18PIN_ALT;
}

void checkRow_18Pin_Alt(uint16_t row, uint8_t patNr) {
  uint16_t cols = ramTypes[type].columns;  // 1024
  uint16_t row_mix = row + (row >> 4);
  // K-hoist + 256-col block split (see writeRow_18Pin_Alt): indices bit-identical.
  uint8_t Krow = (uint8_t)(row_mix ^ (row_mix >> 8));
  WE_HIGH_18PIN_ALT;
  rasHandling_18Pin_Alt(row);
  cli();
  for (uint8_t blk = 0; blk < (uint8_t)(cols >> 8); blk++) {  // 1024 cols = 4 x 256
    uint8_t Kb = Krow ^ blk;
    uint16_t colBase = (uint16_t)blk << 8;
    uint8_t c8 = 0;
    do {
      uint8_t exp = (randomTable[(uint8_t)(Kb ^ c8)] >> 3) & 1;
      setAddr18Alt_inl(colBase | c8);  // force-inlined (hot per-cell path)
      CAS_LOW_18PIN_ALT;
      NOP;
      CAS_HIGH_18PIN_ALT;
      if (GET_DOUT_18PIN_ALT() != exp) {
        sei();
        RAS_HIGH_18PIN_ALT;
        error(patNr, 3);
      }
      if ((c8 & (RAS_BURST_ALT_RND - 1)) == (RAS_BURST_ALT_RND - 1))
        rasHandling_18Pin_Alt(row);  // RAS-only recycle (CAS high), re-latch row (~82us @ burst 16)
    } while (++c8 != 0);
  }
  RAS_HIGH_18PIN_ALT;  // close the row BEFORE sei() (pending-ISR stretch)
  sei();
}

void writeRow_18Pin_Alt(uint16_t row, uint8_t patNr) {
  const RAM_Definition *rt = &ramTypes[type];  // 411000: rows=1024, cols=1024
  uint16_t cols = rt->columns;
  uint16_t last_row = rt->rows - 1;
  uint16_t row_mix = row + (row >> 4);

  // K-hoist: for v = col ^ row_mix with col = (blk<<8)|c8 the table index
  // (uint8_t)(v ^ (v>>8)) folds to (Krow ^ blk) ^ c8 -> hoist Krow per row and
  // Kb per 256-col block; per cell a single eor replaces the 16-bit v math, and
  // the 8-bit c8 do-while replaces the 16-bit col loop. Indices bit-identical.
  uint8_t Krow = (uint8_t)(row_mix ^ (row_mix >> 8));
  rasHandling_18Pin_Alt(row);
  WE_LOW_18PIN_ALT;
  cli();
  for (uint8_t blk = 0; blk < (uint8_t)(cols >> 8); blk++) {  // 1024 cols = 4 x 256
    uint8_t Kb = Krow ^ blk;
    uint16_t colBase = (uint16_t)blk << 8;
    uint8_t c8 = 0;
    do {
      uint8_t din = (randomTable[(uint8_t)(Kb ^ c8)] >> 3) & 1;
      setAddr18Alt_inl(colBase | c8);  // force-inlined (hot per-cell path)
      SET_DIN_18PIN_ALT(din);
      CAS_LOW_18PIN_ALT;
      NOP;  // Mi-4: deterministic CAS-low width (~187 ns min) — the bool below is not a
            // reliable spacer (the compiler may schedule it outside the CAS window)
      bool RAS = (c8 & (RAS_BURST_ALT_RND - 1)) == (RAS_BURST_ALT_RND - 1);
      CAS_HIGH_18PIN_ALT;
      if (RAS)
        rasHandling_18Pin_Alt(row);  // RAS-only recycle (CAS high), re-latch row (~82us @ burst 16)
    } while (++c8 != 0);
  }
  WE_HIGH_18PIN_ALT;
  RAS_HIGH_18PIN_ALT;  // close the row BEFORE sei() (pending-ISR stretch)
  sei();

  // Retention aging + pipelined read-back via the shared tail (common.cpp).
  refreshRow_18Pin_Alt(row);
  retentionTail(row, last_row, patNr, checkRow_18Pin_Alt);
}

//=======================================================================================
// REFRESH TIME TEST (18-PIN: 4464 AND 411000)
//=======================================================================================

/**
 * CAS-before-RAS refresh for standard 18-pin (4464)
 */
static inline void casBeforeRasRefresh_18Pin() {
  RAS_HIGH18;
  CAS_LOW18;
  RAS_LOW18;
  NOP;
  NOP;
  RAS_HIGH18;
  CAS_HIGH18;
}

/**
 * CAS-before-RAS refresh for 411000 (alternative pinout)
 */
static inline void casBeforeRasRefresh_18Pin_Alt() {
  RAS_HIGH_18PIN_ALT;
  CAS_LOW_18PIN_ALT;
  RAS_LOW_18PIN_ALT;
  NOP;
  NOP;
  RAS_HIGH_18PIN_ALT;
  CAS_HIGH_18PIN_ALT;
}

#if 0   // OBSOLETE (5.0.3): refresh-time tests; the ~60 s CBR counter test is now folded \
        // into pass_18Pin / pass_18Alt via the runCBR flag (kept here for reference only).
/**
 * Refresh time test for 4464 (64Kx4)
 * - 4ms refresh time, 256 physical rows
 * - 8-bit refresh counter (A0-A7) requires 256 CBR cycles
 * - Write/read 2 columns (4-bit nibble, test 2 bits)
 *
 * 4464: 4ms / 256 cycles = 15.625μs per refresh cycle
 * Target: 125 full sequences × 4ms = 500ms retention test
 * Total: 125 × 256 = 32,000 individual refresh cycles
 */
static void refreshTimeTest_18Pin() {
  uint16_t rows = ramTypes[type].rows;  // 256 for 4464
  configDataOut_18Pin();

  // ===== PHASE 1: WRITE 2 NIBBLES TO EACH ROW =====
  CAS_HIGH18;

  for (uint16_t row = 0; row < rows; row++) {
    uint8_t dataNibble = randomTable[row & 0xFF] & 0x0F;

    rasHandling_18Pin(row);
    WE_LOW18;

    // Write 2 columns (col 0, col 1)
    for (uint8_t col = 0; col < 2; col++) {
      uint8_t nibble = (dataNibble >> (col * 2)) & 0x03;
      setData18Pin(nibble);
      setAddr18Pin(col);
      CAS_LOW18;
      NOP;
      CAS_HIGH18;
    }
    WE_HIGH18;
    casBeforeRasRefresh_18Pin();
  }

  WE_HIGH18;

  // ===== PHASE 2: CBR REFRESH FOR ~60 s =====
  // Run CBR-only at the spec rate (~15.625 µs/cycle) for a full minute, then verify.
  // A broken refresh COUNTER leaves rows unrefreshed each sweep -> after 60 s the
  // data loss is unmistakable. millis() bounds the duration (32-bit).
  uint32_t cbr_t0 = millis();
  do {
    casBeforeRasRefresh_18Pin();
    delayMicroseconds(14);
    NOP;
    NOP;
    NOP;
  } while ((uint32_t)(millis() - cbr_t0) < 60000UL);
  configDataIn_18Pin();
  // ===== PHASE 3: READ AND VERIFY =====
  for (uint16_t row = 0; row < rows; row++) {
    uint8_t expectedNibble = randomTable[row & 0xFF] & 0x0F;

    rasHandling_18Pin(row);
    OE_LOW18;

    for (uint8_t col = 0; col < 2; col++) {
      setAddr18Pin(col);
      CAS_LOW18;
      NOP;
      NOP;

      uint8_t actual = getData18Pin() & 0x03;
      uint8_t expected = (expectedNibble >> (col * 2)) & 0x03;

      CAS_HIGH18;

      if (actual != expected) {
        RAS_HIGH18;
        OE_HIGH18;
        error(0, 5);
        return;
      }
    }

    OE_HIGH18;
    casBeforeRasRefresh_18Pin();
  }
}

/**
 * Refresh time test for 411000 (1Mx1)
 * - 8ms refresh time, 1024 physical rows
 * - IMPORTANT: Only needs 512 CBR refresh cycles (9-bit counter, A0-A8)
 * - Each CBR cycle refreshes 2 rows (similar to 41256)
 * - Write/read 8 bits (1Mx1 is single bit, test 8 columns)
 *
 * 411000: 8ms / 512 cycles = 15.625μs per refresh cycle
 * Target: 125 full sequences × 8ms = 1000ms retention test
 * Total: 125 × 512 = 64,000 individual refresh cycles
 */
static void refreshTimeTest_18Pin_Alt() {
  uint16_t rows = 1024;

  // ===== PHASE 1: WRITE 8 BITS TO EACH ROW =====
  CAS_HIGH_18PIN_ALT;

  for (uint16_t row = 0; row < rows; row++) {
    uint8_t dataByte = randomTable[row & 0xFF];

    setAddr_18Pin_Alt(row);
    RAS_LOW_18PIN_ALT;
    WE_LOW_18PIN_ALT;

    for (uint8_t col = 0; col < 8; col++) {
      uint8_t bit = (dataByte >> col) & 0x01;
      SET_DIN_18PIN_ALT(bit);
      setAddr_18Pin_Alt(col);
      CAS_LOW_18PIN_ALT;
      NOP;
      CAS_HIGH_18PIN_ALT;
    }

    WE_HIGH_18PIN_ALT;
    casBeforeRasRefresh_18Pin_Alt();
  }

  WE_HIGH_18PIN_ALT;

  // ===== PHASE 2: CBR REFRESH FOR ~60 s =====
  // Run CBR-only at the spec rate (~15.625 µs/cycle) for a full minute, then verify.
  // A broken refresh COUNTER leaves rows unrefreshed each sweep -> after 60 s the
  // data loss is unmistakable. millis() bounds the duration (32-bit).
  uint32_t cbr_t0 = millis();
  do {
    casBeforeRasRefresh_18Pin_Alt();
    delayMicroseconds(15);
    NOP;
    NOP;
    NOP;
    NOP;
    NOP;
    NOP;
  } while ((uint32_t)(millis() - cbr_t0) < 60000UL);

  // ===== PHASE 3: READ AND VERIFY =====
  for (uint16_t row = 0; row < rows; row++) {
    uint8_t expectedByte = randomTable[row & 0xFF];

    setAddr_18Pin_Alt(row);
    RAS_LOW_18PIN_ALT;
    for (uint8_t col = 0; col < 8; col++) {
      setAddr_18Pin_Alt(col);
      CAS_LOW_18PIN_ALT;
      NOP;
      NOP;

      uint8_t actual = GET_DOUT_18PIN_ALT();
      uint8_t expected = (expectedByte >> col) & 0x01;

      CAS_HIGH_18PIN_ALT;

      if (actual != expected) {
        RAS_HIGH_18PIN_ALT;
        error(0, 5);
        return;
      }
    }

    casBeforeRasRefresh_18Pin_Alt();
  }
}
#endif  // OBSOLETE refresh-time tests (18Pin std + Alt)

//=======================================================================================
// 2114 SRAM ADDRESSING TEST
// 1) Physical pin-level short test on all 10 address lines (chip idle, _CS high).
// 2) Walking-bit functional test with UNIQUE nibble per test address.
//    Unique values are essential to catch address-line shorts:
//    if A_i and A_j are shorted, writes to (1<<i) and (1<<j) hit the same cell
//    and overwrite each other -> mismatch on read.
//=======================================================================================
/**
 * Verify 2114 address bus integrity
 *
 * Two-stage check:
 *   1. checkAddressShorts() probes every address pin against the others while
 *      _CS is HIGH (chip tristate), detecting shorts and stuck-at faults on
 *      the bus itself.
 *   2. Walking-bit test writes a unique nibble at address 0 (0x0F) and at each
 *      power-of-two address 1<<b with value b+1 (b=0..9). Reading back must
 *      return the same unique values. Any swapped or broken address line
 *      produces a mismatch.
 *
 * Calls error() and never returns if a fault is detected.
 */
void checkAddressing_2114(void) {
  // Pin-level short test (uses pull-ups, expects chip tristate via _CS=high)
  checkAddressShorts(ADDR_MASK_B, ADDR_MASK_C, ADDR_MASK_D);
  // Restore DDR/PORT (checkAddressShorts left address pins as inputs)
  sram2114_init();

  // Write unique nibble values
  sram2114_write(0, 0x0F);
  for (uint8_t b = 0; b < 10; b++) {
    sram2114_write((uint16_t)1 << b, (uint8_t)(b + 1));
  }
  // Read back and verify
  if (sram2114_read(0) != 0x0F) error(0, 1);
  for (uint8_t b = 0; b < 10; b++) {
    if (sram2114_read((uint16_t)1 << b) != (uint8_t)(b + 1)) error(b, 1);
  }
}

/**
 * Verify 2114 _CS and _WE control-signal behaviour
 *
 * Combined functional check of the two control inputs:
 *   (a) With _CS HIGH the chip must tristate its IO drivers. The IO lines are
 *       reconfigured as inputs with pull-ups; a working chip lets the pull-ups
 *       win and reads back as 0xF.
 *   (b) While _CS is still HIGH, _WE is pulsed LOW. A correct 2114 ignores
 *       _WE unless _CS is also LOW, so the reference value (0x05 at TADR)
 *       must survive the pulse. This is verified by reading TADR back.
 *
 * Calls error() and never returns on failure (errType 7 = "SRAM Error", the
 * chip IS present so this is distinct from errType 0 "no RAM"):
 *   - error(1, 7): tristate failed (read != 0xF)            -> "SRAM Error 1"
 *   - error(2, 7): _WE was not blocked by _CS (ref changed) -> "SRAM Error 2"
 */
void check_CSWE_2114(void) {
  const uint16_t TADR = 0x123;

  // Reference value at TADR — must survive the _WE pulse while _CS is HIGH
  sram2114_write(TADR, 0x05);

  // Combined check:
  //   - _CS HIGH, IO pins = inputs with pull-ups -> chip must tristate (read 0xF)
  //   - while in that state, address bus stays at TADR and we pulse _WE.
  //     With _CS HIGH a real 2114 ignores _WE -> reference value preserved.
  drive_addr_2114(TADR);
  DDRB &= ~IO_MASK_B;
  DDRC &= ~IO_MASK_C;
  PORTB |= IO_MASK_B;            // IO pull-ups on
  PORTC |= IO_MASK_C | CS_2114;  // IO pull-ups + _CS HIGH
  delayMicroseconds(5);
  uint8_t pb = PINB;
  uint8_t pc = PINC;
  // Pulse _WE while _CS still HIGH -> must NOT write
  PORTC &= ~WE_2114;
  NOP;
  NOP;
  PORTC |= WE_2114;
  PORTB &= ~IO_MASK_B;  // pull-ups off
  PORTC &= ~IO_MASK_C;
  uint8_t tri = ((pc >> 4) & 0x01)     // PC4 → D0
                | (pb & 0x02)          // PB1 → D1
                | ((pb >> 1) & 0x04)   // PB3 → D2
                | ((pc << 2) & 0x08);  // PC1 → D3
  if (tri != 0x0F) error(1, 7);  // "SRAM Error 1": _CS-high tristate failed
  // _WE-block verification: reference value must still be 0x05
  if (sram2114_read(TADR) != 0x05) error(2, 7);  // "SRAM Error 2": _WE not blocked by _CS
}

/**
 * Full 2114 SRAM test — March C- algorithm (industry-standard SRAM march test)
 *
 * March C- has 6 elements and detects the standard SRAM fault classes:
 *   SAF (stuck-at), TF (transition), AF (address decoder),
 *   CFin/CFid/CFst (coupling faults) — far stronger coverage than the old
 *   write-all/read-all constant + pseudo-random passes, which cannot reliably
 *   catch inversion/idempotent coupling or transition faults.
 *
 *   M0:  ⇕ (w0)            write background to all cells (either direction)
 *   M1:  ⇑ (r0, w1)        ascending : read bg, write inverse
 *   M2:  ⇑ (r1, w0)        ascending : read inverse, write bg
 *   M3:  ⇓ (r0, w1)        descending: read bg, write inverse
 *   M4:  ⇓ (r1, w0)        descending: read inverse, write bg
 *   M5:  ⇕ (r0)            read background from all cells
 *
 * Run TWICE with two complementary data backgrounds:
 *   bg = 0x0 -> 0x0 / 0xF  (solid)       : SAF, TF, inter-cell coupling
 *   bg = 0x5 -> 0x5 / 0xA  (checkerboard): intra-word IO coupling (x4 lines)
 * Together every IO line sees both 0 and 1 with both neighbour polarities.
 * All compares mask & 0x0F (2114 has only 4 IO lines).
 *
 * Error sub-codes — error(N, 7) -> OLED "SRAM Error N", N = March element:
 *   3 = M1 (r0)   4 = M2 (r1)   5 = M3 (r0)   6 = M4 (r1)   7 = M5 (r0)
 * (errType 7 = "SRAM Error"; sub-codes 1/2 are used by check_CSWE_2114.)
 * Never returns on the first mismatch.
 */
void check_2114(void) {
  for (uint8_t k = 0; k < 2; k++) {
    const uint8_t v0 = (k == 0) ? 0x00 : 0x05;  // background; was a 2-byte SRAM array
    const uint8_t v1 = (uint8_t)(~v0) & 0x0F;
    uint16_t a;
    // M0: (w0) — either direction
    for (a = 0; a < 1024; a++) sram2114_write(a, v0);
    // M1: (r0,w1) ascending
    for (a = 0; a < 1024; a++) {
      if (sram2114_read(a) != v0) error(3, 7);  // "SRAM Error 3"
      sram2114_write(a, v1);
    }
    // M2: (r1,w0) ascending
    for (a = 0; a < 1024; a++) {
      if (sram2114_read(a) != v1) error(4, 7);  // "SRAM Error 4"
      sram2114_write(a, v0);
    }
    // M3: (r0,w1) descending
    for (a = 1024; a-- > 0;) {
      if (sram2114_read(a) != v0) error(5, 7);  // "SRAM Error 5"
      sram2114_write(a, v1);
    }
    // M4: (r1,w0) descending
    for (a = 1024; a-- > 0;) {
      if (sram2114_read(a) != v1) error(6, 7);  // "SRAM Error 6"
      sram2114_write(a, v0);
    }
    // M5: (r0) — either direction
    for (a = 0; a < 1024; a++)
      if (sram2114_read(a) != v0) error(7, 7);  // "SRAM Error 7"
  }
}

// tACC speed probe (5.0.5). Exploits that the 2114 is a STATIC RAM: with _CS held LOW,
// Dout follows the ADDRESS after tACC — the headline spec the grades are named after
// (200/300/450 ns), which the strobe-based sweep never exercised. Clean measurement:
//   - the two probe addresses differ ONLY in A4 (PD7)  -> ONE atomic PORTD write
//   - their data differ ONLY in D0 (IO1 = PC4)         -> ONE atomic PINC read
//   - the class window is the delay between those two instructions
// Windows (address-valid -> sample, incl. the in-latch cycle): ~250 / ~375 / ~560 ns.
// STARTING VALUES — calibrate against graded parts (shift by ±1 cycle). Expect room-
// temperature optimism of ~one grade: silicon is binned at 70 degC worst case, so a
// true -45 typically reads "-30". A part failing even the slow window still grades
// "-45" (it already passed the functional March test at the proven strobes).
#define TACC_PROBE_LOOP(W, okvar) \
  do { \
    okvar = true; \
    for (uint8_t r = 0; r < 16; r++) { \
      PORTD = pdB; \
      __builtin_avr_delay_cycles(W); \
      if (!(PINC & (1 << PC4))) okvar = false; \
      PORTD = pdA; \
      __builtin_avr_delay_cycles(W); \
      if (PINC & (1 << PC4)) okvar = false; \
    } \
  } while (0)

static const __FlashStringHelper *speedProbe_2114(void) {
  // Reference cells via the proven slow writes: D0=0 at addr A, D0=1 at addr B (=A4 set)
  sram2114_write(0x000, 0x0);
  sram2114_write(0x010, 0x1);
  // Data bus to inputs with pull-ups (as in sram2114_read), park the address at A,
  // then hold _CS LOW for the whole probe (static read mode, _WE stays high)
  DDRB &= ~IO_MASK_B;
  PORTB |= IO_MASK_B;
  DDRC &= ~IO_MASK_C;
  PORTC |= IO_MASK_C;
  drive_addr_2114(0x000);
  uint8_t pdA = PORTD;         // probe address A on PORTD
  uint8_t pdB = pdA | 0x80;    // probe address B: A4 (PD7) set
  PORTC &= ~CS_2114;
  __builtin_avr_delay_cycles(8);  // first access settles via the normal tCO path
  uint8_t cls = 0;
  bool ok;
  cli();                       // ISR jitter would stretch the sample windows
  TACC_PROBE_LOOP(3, ok);      // "-20"
  if (!ok) {
    cls = 1;
    TACC_PROBE_LOOP(5, ok);    // "-30"
    if (!ok) cls = 2;          // "-45" (informational; March already passed)
  }
  sei();
  PORTC |= CS_2114;            // deassert _CS
  return (const __FlashStringHelper *)(cls == 0   ? sfx20_2114
                                       : cls == 1 ? sfx30_2114
                                                  : sfx45_2114);
}