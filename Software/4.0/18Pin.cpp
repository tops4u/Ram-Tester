// 18Pin.cpp - Implementation of 18-Pin DRAM testing functions
//=======================================================================================
//
// This file contains all test logic for 18-pin DRAM packages. It implements:
// - Standard 18-pin DRAM testing (4416, 4464)
// - Alternative 18-pin configuration for 411000 (1Mx1)
// - Optimized lookup tables for fast address setting
// - Comprehensive test patterns (stuck-at, alternating, pseudo-random)
// - Page mode read/write operations with minimal timing overhead
//
// Supported chips:
// - 4416 (16Kx4, 256 rows x 64 cols, 4ms refresh)
// - 4464 (64Kx4, 256 rows x 256 cols, 4ms refresh)
// - 411000 (1Mx1, 1024 rows x 1024 cols, 8ms refresh, alternative pinout)
//
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

//=======================================================================================
// 18-PIN ALTERNATIVE LOOKUP TABLE GENERATION
//=======================================================================================

// Macro for PORT D (Must cover A1 through A6)
// Note: Starts at 0x002 (A1), not 0x001!
#define CALC_PORTD(a) \
  ((((a)&0x002) >> 1) | (((a)&0x004) >> 1) | (((a)&0x008) >> 1) | (((a)&0x010) << 1) | (((a)&0x020) << 1) | (((a)&0x040) << 1))

// Macro for PORT B (Starting from A7)
#define CALC_PORTB(a) \
  ((((a)&0x080) >> 3) | (((a)&0x100) >> 6) | (((a)&0x200) >> 9))

// Helper macros for generating lookup table arrays
#define ROW8_D(b) CALC_PORTD(b + 0), CALC_PORTD(b + 1), CALC_PORTD(b + 2), CALC_PORTD(b + 3), CALC_PORTD(b + 4), CALC_PORTD(b + 5), CALC_PORTD(b + 6), CALC_PORTD(b + 7)
#define ROW64_D(b) ROW8_D(b), ROW8_D(b + 8), ROW8_D(b + 16), ROW8_D(b + 24), ROW8_D(b + 32), ROW8_D(b + 40), ROW8_D(b + 48), ROW8_D(b + 56)

// 1. Tabelle für die unteren Bits (PORTD). Größe: 128 Bytes.
// Deckt Adressen 0 bis 127 ab. Das Muster wiederholt sich danach.
const uint8_t lut_18Pin_Low[128] PROGMEM = {
  ROW64_D(0), ROW64_D(64)
};

// 2. Tabelle für die oberen Bits (PORTB). Größe: 8 Bytes.
// Deckt die High-Bits für 0, 128, 256 ... 896 ab.
const uint8_t lut_18Pin_High[8] PROGMEM = {
  CALC_PORTB(0), CALC_PORTB(128), CALC_PORTB(256), CALC_PORTB(384),
  CALC_PORTB(512), CALC_PORTB(640), CALC_PORTB(768), CALC_PORTB(896)
};

/**
 * High-Performance Address Setter using Split LUTs
 * Replaces the 2048 Byte struct array with two small byte arrays.
 */
inline __attribute__((always_inline)) void setAddr_18Pin_Alt(uint16_t a) {
  // 1. PORT C: Handle A0 (IMPORTANT! This was missing previously)
  if (a & 1) SBI(PORTC, 4);
  else CBI(PORTC, 4);

  // 2. PORT D: A1 through A6
  // Note: If your LUT only has 64 entries (0..63), it's not sufficient for A6!
  // You need either a 128-byte LUT or must shift the index.
  // Am einfachsten: Index ist 'a', LUT muss Adressen bis bit 6 (127) abdecken.
  uint8_t val_d = pgm_read_byte(&lut_18Pin_Low[a & 0x7F]);  // Maske 0x7F um A6 mitzunehmen!
  PORTD = (PORTD & 0b00011000) | val_d;

  // 3. PORT B: A7 bis A9
  // Hier ab Bit 7 schauen.
  uint8_t val_b = pgm_read_byte(&lut_18Pin_High[a >> 7]);
  PORTB = (PORTB & 0b11101010) | val_b;
}

//=======================================================================================
// MAIN TEST FUNCTION
//=======================================================================================

// Forward declarations for refresh time tests
static void refreshTimeTest_18Pin();
static void refreshTimeTest_18Pin_Alt();

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

  // First try standard 18-Pin (with OE test)
  if (ram_present_18Pin()) {
    sense4464_18Pin();
  } else {
    type = -1;
  }

  if (type == -1) {
    // Standard failed, try 411000
    if (ram_present_18Pin_alt()) {
      sense411000_18Pin_Alt();
    }
  }

  if (type == -1) {
    error(0, 0);  // Definitely no RAM
    return;
  }
  writeRAMType((const __FlashStringHelper*)ramTypes[type].name);
  if (type == T_411000) {
    DDRB = (DDRB & 0xE0) | 0x1D;
    DDRC = (DDRC & 0xE0) | 0x17;
    DDRD = (DDRD & 0x18) | 0xE7;
    checkAddressing_18Pin_Alt();
    for (uint8_t patNr = 0; patNr <= 5; patNr++) {
      if (patNr == 5)
        invertRandomTable();
      for (uint16_t row = 0; row < 1024; row++) {
        writeRow_18Pin_Alt(row, patNr);
      }
    }
  } else {
    DDRB = 0b00111111;
    PORTB = 0b00100010;
    checkAddressing_18Pin();
    for (uint8_t patNr = 0; patNr <= 5; patNr++) {
      if (patNr == 5)
        invertRandomTable();
      for (uint16_t row = 0; row < ramTypes[type].rows; row++) {
        writeRow_18Pin(row, patNr, ramTypes[type].columns);
      }
    }
  }

  // ===== REFRESH TIME TEST =====
  // Test if chip can retain data with maximum refresh interval
  if (type == T_4464 || type == T_411000) {
    if (type == T_411000) {
      refreshTimeTest_18Pin_Alt();
    } else {
      refreshTimeTest_18Pin();
    }
  }

  testOK();
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

//=======================================================================================
// STANDARD 18-PIN DRAM FUNCTIONS (4416/4464)
//=======================================================================================

// Small hot helpers keep code compact and readable
static inline __attribute__((always_inline, hot)) void col_write(uint8_t col, uint8_t data) {
  setData18Pin(data);
  setAddr18Pin(col);
  CAS_LOW18;
  NOP;  // tCL guard
  CAS_HIGH18;
}

static inline __attribute__((always_inline, hot)) uint8_t col_read(uint8_t col) {
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
  // Derive bit counts (rows/cols assumed power-of-two from ramTypes)
  uint16_t rows = ramTypes[type].rows;
  uint16_t cols = ramTypes[type].columns;
  uint8_t rowBits = 0, colBits = 0;
  for (uint16_t t = rows - 1; t; t >>= 1) rowBits++;
  for (uint16_t t = cols - 1; t; t >>= 1) colBits++;

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

/**
 * Write and verify pattern data to a complete row for standard 18-pin DRAMs
 * @param row Row address to write/test
 * @param patNr Pattern number (0-4)
 * @param width Number of columns in this RAM type
 */
void writeRow_18Pin(uint8_t row, uint8_t patNr, uint16_t width) {
  uint16_t colAddr;  // Prepared Column Address to save Init Time. This is needed when A0 & A8 are not used for Col addressing.
  uint8_t init_shift = type == T_4416 ? 1 : 0;

  // Prepare Write Cycle
  rasHandling_18Pin(row);
  WE_LOW18;
  configDataOut_18Pin();
  setData18Pin(pattern[patNr]);

  cli();
  if (patNr < 4)
    for (uint16_t col = 0; col < width; col++) {
      CAS_HIGH18;
      colAddr = (col << init_shift);
      setAddr18Pin(colAddr);
      CAS_LOW18;
    }
  else
    for (uint16_t col = 0; col < width; col++) {
      CAS_HIGH18;
      setData18Pin(randomTable[mix8(col, row)]);
      colAddr = (col << init_shift);
      setAddr18Pin(colAddr);
      CAS_LOW18;
    }
  sei();

  WE_HIGH18;
  CAS_HIGH18;

  // If we check 255 Columns the time for Write & Read(Check) exceeds the Refresh time. We need to add a Refresh in the Middle
  if (patNr < 4) {
    checkRow_18Pin(width, row, patNr, init_shift, 2);
    return;
  }

  refreshRow_18Pin(row);
  if (row == ramTypes[type].rows - 1) {  // Last Row written, we have to check the last n Rows as well.
    // Retention testing the last rows, they will no longer be written only read back. Simulate the write time to get a correct retention time test.
    for (int8_t x = ramTypes[type].delayRows; x >= 0; x--) {
      rasHandling_18Pin(row - x);
      checkRow_18Pin(width, row - x, patNr, init_shift, 3);
      delayMicroseconds(ramTypes[type].writeTime * 20);  // Simulate writing even if it is no longer done for the last rows
      delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows] * 20);
    }
    return;
  }

  if (row >= ramTypes[type].delayRows) {
    rasHandling_18Pin(row - ramTypes[type].delayRows);
    checkRow_18Pin(width, row - ramTypes[type].delayRows, patNr, init_shift, 3);
  }

  if (row < ramTypes[type].delayRows)
    delayMicroseconds(ramTypes[type].delays[row] * 20);
  else
    delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows] * 20);
}

/**
 * Read and verify data from a complete row for standard 18-pin DRAMs
 * @param width Number of columns to check
 * @param row Row address being tested  
 * @param patNr Pattern number (0-4)
 * @param init_shift Address shift for 4416 compatibility (1 for 4416, 0 for 4464)
 * @param errorNr Error code to report if check fails
 */
void checkRow_18Pin(uint16_t width, uint8_t row, uint8_t patNr, uint8_t init_shift, uint8_t errorNr) {
  configDataIn_18Pin();
  uint8_t pat = pattern[patNr] & 0x0f;
  OE_LOW18;

  cli();
  if (patNr < 4)
    for (uint16_t col = 0; col < width; col++) {
      setAddr18Pin(col << init_shift);
      CAS_LOW18;
      NOP;
      CAS_HIGH18;
      if ((getData18Pin()) != pat) {
        error(patNr, errorNr, row, col);
      }
    }
  else
    for (uint16_t col = 0; col < width; col++) {
      setAddr18Pin(col << init_shift);
      CAS_LOW18;
      CAS_HIGH18;
      if (getData18Pin() != randomTable[mix8(col, row)]) {
        error(patNr, errorNr, row, col);
      }
    }
  sei();

  OE_HIGH18;
  RAS_HIGH18;
}

/**
 * Refresh a specific row by performing RAS-only cycle
 * @param row Row address to refresh
 */
void refreshRow_18Pin(uint8_t row) {
  rasHandling_18Pin(row);
  NOP;
  RAS_HIGH18;
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
  uint16_t rows = ramTypes[type].rows;
  uint16_t cols = ramTypes[type].columns;
  uint8_t rowBits = 0, colBits = 0;
  for (uint16_t t = rows - 1; t; t >>= 1) rowBits++;
  for (uint16_t t = cols - 1; t; t >>= 1) colBits++;

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
 * Set row address and activate RAS signal for 18Pin_Alt
 * @param row Row address to access
 */
void rasHandling_18Pin_Alt(uint16_t row) {
  RAS_HIGH_18PIN_ALT;
  setAddr_18Pin_Alt(row);
  RAS_LOW_18PIN_ALT;
}

/**
 * OPTIMIZED: Row Write for 18Pin_Alt with Pattern-Rotation
 * Writes test patterns to a complete row and performs inline verification for stuck-at patterns
 * @param row Row address to write/test
 * @param patNr Pattern number (0-4)
 */
void __attribute__((hot)) writeRow_18Pin_Alt(uint16_t row, uint8_t patNr) {
  RAS_HIGH_18PIN_ALT;
  setAddr_18Pin_Alt(row);
  RAS_LOW_18PIN_ALT;
  WE_LOW_18PIN_ALT;

  uint8_t pat = pattern[patNr];

  cli();
  if (patNr < 2) {
    // Either it is 0 or 1 for the first 2 patterns, so we speed up here.
    SET_DIN_18PIN_ALT(pat & 0x08);
    // Regular patterns with Bit-Rotation
    for (uint16_t col = 0; col < 1024; col++) {
      setAddr_18Pin_Alt(col);
      CAS_LOW_18PIN_ALT;
      CAS_HIGH_18PIN_ALT;
      // Pattern 0 + 1 checks for stuck bits, we can check inline
      WE_HIGH_18PIN_ALT;
      CAS_LOW_18PIN_ALT;
      CAS_HIGH_18PIN_ALT;
      if (((PINC ^ pat) & 0x08) != 0) {
        error(patNr, 2, row, col);
      }
      WE_LOW_18PIN_ALT;
    }
    return;
  } else if (patNr < 4) {
    // Regular patterns with Bit-Rotation
    for (uint16_t col = 0; col < 1024; col++) {
      SET_DIN_18PIN_ALT(pat & 0x08);
      setAddr_18Pin_Alt(col);
      CAS_LOW_18PIN_ALT;
      pat = rotate_left(pat);  // 1-Bit Rotation for 1Mx1
      CAS_HIGH_18PIN_ALT;
    }
  } else {
    // Random pattern
    for (uint16_t col = 0; col < 1024; col++) {
      SET_DIN_18PIN_ALT(randomTable[mix8(col, row)] & 0x08);
      setAddr_18Pin_Alt(col);
      CAS_LOW_18PIN_ALT;
      CAS_HIGH_18PIN_ALT;
    }
  }
  sei();

  WE_HIGH_18PIN_ALT;
  RAS_HIGH_18PIN_ALT;

  // Read back and check
  if (patNr < 4) {
    checkRow_18Pin_Alt(row, patNr, 2);
    return;
  }

  refreshRow_18Pin_Alt(row);
  // Retention testing (analog to other RAMs)
  if (row == ramTypes[type].rows - 1) {                       // Last row
    for (int8_t x = ramTypes[type].delayRows; x >= 0; x--) {  // Check last 5 rows
      checkRow_18Pin_Alt(row - x, patNr, 3);
      delayMicroseconds(ramTypes[type].writeTime * 20);  // Simulate writing even if it is no longer done for the last rows
      delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows] * 20);
    }
    return;
  } else if (row >= ramTypes[type].delayRows) {
    checkRow_18Pin_Alt(row - ramTypes[type].delayRows, patNr, 3);
    delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows] * 20);
    return;
  }
  delayMicroseconds(ramTypes[type].delays[row] * 20);
}

/**
 * Row Check for 18Pin_Alt
 * Reads and verifies data from a complete row
 * @param row Row address being tested
 * @param patNr Pattern number (0-4)
 * @param errorNr Error code to report if check fails
 */
void __attribute__((hot)) checkRow_18Pin_Alt(uint16_t row, uint8_t patNr, uint8_t errorNr) {
  uint8_t pat = pattern[patNr];
  rasHandling_18Pin_Alt(row);

  cli();
  if (patNr < 4) {
    for (uint16_t col = 0; col < 1024; col++) {
      setAddr_18Pin_Alt(col);
      CAS_LOW_18PIN_ALT;
      CAS_HIGH_18PIN_ALT;
      if (((PINC ^ pat) & 0x08) != 0) {
        error(patNr + 1, errorNr, row, col);
      }
      pat = rotate_left(pat);
    }
  } else {
    for (uint16_t col = 0; col < 1024; col++) {
      setAddr_18Pin_Alt(col);
      CAS_LOW_18PIN_ALT;
      uint8_t expected_bit = randomTable[mix8(col, row)];
      CAS_HIGH_18PIN_ALT;
      if (((PINC ^ expected_bit) & 0x08) != 0) {
        error(patNr + 1, errorNr, row, col);
      }
    }
  }
  sei();

  RAS_HIGH_18PIN_ALT;
}

/**
 * Refresh for 18Pin_Alt
 * Performs RAS-only cycle to refresh specified row
 * @param row Row address to refresh
 */
inline __attribute__((always_inline)) void refreshRow_18Pin_Alt(uint16_t row) {
  rasHandling_18Pin_Alt(row);
  RAS_HIGH_18PIN_ALT;
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

  // ===== PHASE 2: CBR REFRESH CYCLES (40ms) =====
  // Test CBR refresh counter: 10 full sequences through all 256 addresses
  for (uint8_t cycle = 0; cycle < 10; cycle++) {
    // Perform 256 CAS-before-RAS refresh cycles
    for (uint16_t refresh_count = 0; refresh_count < 256; refresh_count++) {
      casBeforeRasRefresh_18Pin();

      // Delay to achieve ~15.625μs per cycle (4ms / 256)
      // CBR refresh takes ~1-2μs, so delay ~14μs
      delayMicroseconds(15);
      NOP;
      NOP;
      NOP;
      NOP;
      NOP;
      NOP;
      NOP;
      NOP;
      NOP;
      NOP;
    }
  }
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

  // ===== PHASE 2: CBR REFRESH CYCLES (80ms) =====
  // Test CBR refresh counter: 10 full sequences through all 512 addresses
  for (uint8_t cycle = 0; cycle < 10; cycle++) {
    // Perform 512 CAS-before-RAS refresh cycles (covers all 1024 rows)
    for (uint16_t refresh_count = 0; refresh_count < 512; refresh_count++) {
      casBeforeRasRefresh_18Pin_Alt();

      // Delay to achieve ~15.625μs per cycle (8ms / 512)
      // CBR refresh takes ~1-2μs, so delay ~14μs
      delayMicroseconds(15);
      NOP;
      NOP;
      NOP;
      NOP;
      NOP;
      NOP;
    }
  }

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
