#include "18Pin.h"

//=======================================================================================
// 18-PIN PORT MAPPINGS
//=======================================================================================

// Port-to-DIP-Pin mappings for standard 18-pin DRAMs
const int CPU_18PORTB[] = { 15, 4, 14, 3, EOL, EOL, EOL, EOL };
const int CPU_18PORTC[] = { 1, 2, 16, 17, 5, EOL, EOL, EOL };
const int CPU_18PORTD[] = { 6, 7, 8, 9, NC, 10, 11, 12 };
const int RAS_18PIN = 18;
const int CAS_18PIN = 16;

// Alternative 18-pin constants
const uint8_t RAS_18PIN_ALT = 11;
const uint8_t CAS_18PIN_ALT = A2;

//=======================================================================================
// 18-PIN ALTERNATIVE LOOKUP TABLE GENERATION
//=======================================================================================

// Macros for 18Pin_Alt LUT generation
#define MAP_18PIN_ALT_COMBINED(a) \
  { \
    .portb = ((((a)&0x080) >> 3) | (((a)&0x100) >> 6) | (((a)&0x200) >> 9)), \
    .portd = ((((a)&0x002) >> 1) | (((a)&0x004) >> 1) | (((a)&0x008) >> 1) | (((a)&0x010) << 1) | (((a)&0x020) << 1) | (((a)&0x040) << 1)) \
  }

#define ROW64_18PIN_ALT_COMBINED(base) \
  MAP_18PIN_ALT_COMBINED(base + 0), MAP_18PIN_ALT_COMBINED(base + 1), MAP_18PIN_ALT_COMBINED(base + 2), MAP_18PIN_ALT_COMBINED(base + 3), \
    MAP_18PIN_ALT_COMBINED(base + 4), MAP_18PIN_ALT_COMBINED(base + 5), MAP_18PIN_ALT_COMBINED(base + 6), MAP_18PIN_ALT_COMBINED(base + 7), \
    MAP_18PIN_ALT_COMBINED(base + 8), MAP_18PIN_ALT_COMBINED(base + 9), MAP_18PIN_ALT_COMBINED(base + 10), MAP_18PIN_ALT_COMBINED(base + 11), \
    MAP_18PIN_ALT_COMBINED(base + 12), MAP_18PIN_ALT_COMBINED(base + 13), MAP_18PIN_ALT_COMBINED(base + 14), MAP_18PIN_ALT_COMBINED(base + 15), \
    MAP_18PIN_ALT_COMBINED(base + 16), MAP_18PIN_ALT_COMBINED(base + 17), MAP_18PIN_ALT_COMBINED(base + 18), MAP_18PIN_ALT_COMBINED(base + 19), \
    MAP_18PIN_ALT_COMBINED(base + 20), MAP_18PIN_ALT_COMBINED(base + 21), MAP_18PIN_ALT_COMBINED(base + 22), MAP_18PIN_ALT_COMBINED(base + 23), \
    MAP_18PIN_ALT_COMBINED(base + 24), MAP_18PIN_ALT_COMBINED(base + 25), MAP_18PIN_ALT_COMBINED(base + 26), MAP_18PIN_ALT_COMBINED(base + 27), \
    MAP_18PIN_ALT_COMBINED(base + 28), MAP_18PIN_ALT_COMBINED(base + 29), MAP_18PIN_ALT_COMBINED(base + 30), MAP_18PIN_ALT_COMBINED(base + 31), \
    MAP_18PIN_ALT_COMBINED(base + 32), MAP_18PIN_ALT_COMBINED(base + 33), MAP_18PIN_ALT_COMBINED(base + 34), MAP_18PIN_ALT_COMBINED(base + 35), \
    MAP_18PIN_ALT_COMBINED(base + 36), MAP_18PIN_ALT_COMBINED(base + 37), MAP_18PIN_ALT_COMBINED(base + 38), MAP_18PIN_ALT_COMBINED(base + 39), \
    MAP_18PIN_ALT_COMBINED(base + 40), MAP_18PIN_ALT_COMBINED(base + 41), MAP_18PIN_ALT_COMBINED(base + 42), MAP_18PIN_ALT_COMBINED(base + 43), \
    MAP_18PIN_ALT_COMBINED(base + 44), MAP_18PIN_ALT_COMBINED(base + 45), MAP_18PIN_ALT_COMBINED(base + 46), MAP_18PIN_ALT_COMBINED(base + 47), \
    MAP_18PIN_ALT_COMBINED(base + 48), MAP_18PIN_ALT_COMBINED(base + 49), MAP_18PIN_ALT_COMBINED(base + 50), MAP_18PIN_ALT_COMBINED(base + 51), \
    MAP_18PIN_ALT_COMBINED(base + 52), MAP_18PIN_ALT_COMBINED(base + 53), MAP_18PIN_ALT_COMBINED(base + 54), MAP_18PIN_ALT_COMBINED(base + 55), \
    MAP_18PIN_ALT_COMBINED(base + 56), MAP_18PIN_ALT_COMBINED(base + 57), MAP_18PIN_ALT_COMBINED(base + 58), MAP_18PIN_ALT_COMBINED(base + 59), \
    MAP_18PIN_ALT_COMBINED(base + 60), MAP_18PIN_ALT_COMBINED(base + 61), MAP_18PIN_ALT_COMBINED(base + 62), MAP_18PIN_ALT_COMBINED(base + 63)

#define GEN1024_18PIN_ALT_COMBINED \
  ROW64_18PIN_ALT_COMBINED(0), ROW64_18PIN_ALT_COMBINED(64), ROW64_18PIN_ALT_COMBINED(128), ROW64_18PIN_ALT_COMBINED(192), \
    ROW64_18PIN_ALT_COMBINED(256), ROW64_18PIN_ALT_COMBINED(320), ROW64_18PIN_ALT_COMBINED(384), ROW64_18PIN_ALT_COMBINED(448), \
    ROW64_18PIN_ALT_COMBINED(512), ROW64_18PIN_ALT_COMBINED(576), ROW64_18PIN_ALT_COMBINED(640), ROW64_18PIN_ALT_COMBINED(704), \
    ROW64_18PIN_ALT_COMBINED(768), ROW64_18PIN_ALT_COMBINED(832), ROW64_18PIN_ALT_COMBINED(896), ROW64_18PIN_ALT_COMBINED(960)

// 1024-entry Lookup Table for 18Pin_Alt
const struct port_config_18Pin_Alt_t PROGMEM lut_18Pin_Alt_combined[1024] = {
  GEN1024_18PIN_ALT_COMBINED
};


//=======================================================================================
// MAIN TEST FUNCTION
//=======================================================================================

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
  writeRAMType();
  if (type == T_411000) {
    DDRB = (DDRB & 0xE0) | 0x1D;
    DDRC = (DDRC & 0xE0) | 0x17;
    DDRD = (DDRD & 0x18) | 0xE7;
    checkAddressing_18Pin_Alt();
    for (uint8_t patNr = 0; patNr <= 5; patNr++) {
      if (patNr == 5)
        randomizeData();
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
        randomizeData();
      for (uint16_t row = 0; row < ramTypes[type].rows; row++) {
        writeRow_18Pin(row, patNr, ramTypes[type].columns);
      }
    }
  }

  testOK();
}


//=======================================================================================
// RAM PRESENCE TESTS
//=======================================================================================

bool ram_present_18Pin(void) {
  uint8_t sDDRB = DDRB, sPORTB = PORTB, sDDRC = DDRC, sPORTC = PORTC, sDDRD = DDRD, sPORTD = PORTD;

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

  // Restore original state
  DDRB = sDDRB;
  PORTB = sPORTB;
  DDRC = sDDRC;
  PORTC = sPORTC;
  DDRD = sDDRD;
  PORTD = sPORTD;

  return (result == 0x5);
}

bool ram_present_18Pin_alt(void) {
  uint8_t sDDRB = DDRB, sPORTB = PORTB, sDDRC = DDRC, sPORTC = PORTC, sDDRD = DDRD, sPORTD = PORTD;

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

  // Restore original state
  DDRB = sDDRB;
  PORTB = sPORTB;
  DDRC = sDDRC;
  PORTC = sPORTC;
  DDRD = sDDRD;
  PORTD = sPORTD;

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

  for (uint8_t b = 0; b < rowBits; b++) {
    uint16_t base_row = 0;
    uint16_t peer_row = (1U << b);

    rasHandling_18Pin(base_row);
    col_write(safeCol, 0x5);
    RAS_HIGH18;

    // tiny guard for conservative devices (≈ 125 ns)
    NOP;
    rasHandling_18Pin(peer_row);
    col_write(safeCol, 0xA);
    RAS_HIGH18;
    NOP;
  }

  // Switch to read mode
  WE_HIGH18;
  configDataIn_18Pin();
  OE_LOW18;

  for (uint8_t b = 0; b < rowBits; b++) {
    uint16_t base_row = 0;
    uint16_t peer_row = (1U << b);

    rasHandling_18Pin(base_row);
    if (col_read(safeCol) != 0x5) {
      RAS_HIGH18;
      error(b, 1);
    }
    RAS_HIGH18;

    rasHandling_18Pin(peer_row);
    if (col_read(safeCol) != 0xA) {
      RAS_HIGH18;
      error(b, 1);
    }
    RAS_HIGH18;
  }

  OE_HIGH18;

  // ---------------- Column Address Tests ----------------
  configDataOut_18Pin();
  WE_LOW18;

  for (uint8_t b = 0; b < colBits; b++) {
    uint8_t base_col = 0;
    uint8_t peer_col = (1U << b);

    rasHandling_18Pin(testRow);
    col_write((uint8_t)(base_col << cshift), 0x5);
    // CAS-high time between two column ops
    NOP;
    col_write((uint8_t)(peer_col << cshift), 0xA);
    RAS_HIGH18;

    // tiny guard
    NOP;
  }

  // Readback
  WE_HIGH18;
  configDataIn_18Pin();
  OE_LOW18;

  for (uint8_t b = 0; b < colBits; b++) {
    uint8_t base_col = 0;
    uint8_t peer_col = (1U << b);

    rasHandling_18Pin(testRow);
    if (col_read((uint8_t)(base_col << cshift)) != 0x5) {
      RAS_HIGH18;
      error(b + 16, 1);
    }
    // CAS-high time between two column reads
    NOP;
    if (col_read((uint8_t)(peer_col << cshift)) != 0xA) {
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
void sense4464_18Pin() {
  // RAM check
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

  // Simplified 4416/4464 logic
  // Test only A0 and A7 (most critical differences)

  rasHandling_18Pin(0);
  configDataOut_18Pin();
  WE_LOW18;
  setData18Pin(0x0);
  setAddr18Pin(0x00);
  CAS_LOW18;
  NOP;
  CAS_HIGH18;

  // Test A0 (most important)
  setData18Pin(0xF);
  setAddr18Pin(0x01);  // A0=1
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
  uint8_t a0_test = getData18Pin() & 0xF;
  CAS_HIGH18;
  OE_HIGH18;

  if (a0_test != 0x0) {
    type = T_4416;  // A0 aliased
  } else {
    // Test A7 for confirmation
    configDataOut_18Pin();
    WE_LOW18;
    setData18Pin(0xF);
    setAddr18Pin(0x80);  // A7=1
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
    uint8_t a7_test = getData18Pin() & 0xF;
    CAS_HIGH18;
    OE_HIGH18;

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
      delayMicroseconds(ramTypes[type].writeTime);  // Simulate writing even if it is no longer done for the last rows
      delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows]);
    }
    return;
  }

  if (row >= ramTypes[type].delayRows) {
    rasHandling_18Pin(row - ramTypes[type].delayRows);
    checkRow_18Pin(width, row - ramTypes[type].delayRows, patNr, init_shift, 3);
  }

  if (row < ramTypes[type].delayRows)
    delayMicroseconds(ramTypes[type].delays[row]);
  else
    delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows]);
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
void sense411000_18Pin_Alt() {
  DDRB = (DDRB & 0xE0) | 0x1D;
  DDRC = (DDRC & 0xE0) | 0x17;
  DDRD = (DDRD & 0x18) | 0xE7;

  RAS_HIGH_18PIN_ALT;
  CAS_HIGH_18PIN_ALT;
  WE_HIGH_18PIN_ALT;

  // Double write/read test

  // Write 0, Read 0
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

  // Write 1, Read 1
  setAddr_18Pin_Alt(1);
  RAS_LOW_18PIN_ALT;  // Different address!
  SET_DIN_18PIN_ALT(1);
  WE_LOW_18PIN_ALT;
  CAS_LOW_18PIN_ALT;
  NOP;
  CAS_HIGH_18PIN_ALT;
  WE_HIGH_18PIN_ALT;
  RAS_HIGH_18PIN_ALT;

  setAddr_18Pin_Alt(1);
  RAS_LOW_18PIN_ALT;
  CAS_LOW_18PIN_ALT;
  NOP;
  NOP;
  uint8_t test2 = GET_DOUT_18PIN_ALT();
  CAS_HIGH_18PIN_ALT;
  RAS_HIGH_18PIN_ALT;

  // Cross-check: Address 0 should still be 0
  setAddr_18Pin_Alt(0);
  RAS_LOW_18PIN_ALT;
  CAS_LOW_18PIN_ALT;
  NOP;
  NOP;
  uint8_t test3 = GET_DOUT_18PIN_ALT();
  CAS_HIGH_18PIN_ALT;
  RAS_HIGH_18PIN_ALT;

  // All tests must be correct
  if (test1 == 0 && test2 != 0 && test3 == 0) {
    type = T_411000;
  } else {
    error(0, 0);
  }
}

void checkAddressing_18Pin_Alt() {
  // 1Mx1: rows/cols aus ramTypes; A9 is automatic
  uint16_t rows = ramTypes[type].rows;
  uint16_t cols = ramTypes[type].columns;
  uint8_t rowBits = 0, colBits = 0;
  for (uint16_t t = rows - 1; t; t >>= 1) rowBits++;
  for (uint16_t t = cols - 1; t; t >>= 1) colBits++;

  // ROWS
  for (uint8_t b = 0; b < rowBits; b++) {
    uint16_t base_row = 0, peer_row = (1U << b);

    // base_row = 0
    setAddr_18Pin_Alt(base_row);
    RAS_LOW_18PIN_ALT;
    SET_DIN_18PIN_ALT(0);
    setAddr_18Pin_Alt(0);
    WE_LOW_18PIN_ALT;
    CAS_LOW_18PIN_ALT;
    NOP;
    CAS_HIGH_18PIN_ALT;
    WE_HIGH_18PIN_ALT;
    RAS_HIGH_18PIN_ALT;

    // peer_row = 1
    setAddr_18Pin_Alt(peer_row);
    RAS_LOW_18PIN_ALT;
    SET_DIN_18PIN_ALT(1);
    setAddr_18Pin_Alt(0);
    WE_LOW_18PIN_ALT;
    CAS_LOW_18PIN_ALT;
    NOP;
    CAS_HIGH_18PIN_ALT;
    WE_HIGH_18PIN_ALT;
    RAS_HIGH_18PIN_ALT;

    // verify
    setAddr_18Pin_Alt(base_row);
    RAS_LOW_18PIN_ALT;
    setAddr_18Pin_Alt(0);
    CAS_LOW_18PIN_ALT;
    NOP;
    NOP;
    if (GET_DOUT_18PIN_ALT() != 0) error(b, 1);
    CAS_HIGH_18PIN_ALT;
    RAS_HIGH_18PIN_ALT;

    setAddr_18Pin_Alt(peer_row);
    RAS_LOW_18PIN_ALT;
    setAddr_18Pin_Alt(0);
    CAS_LOW_18PIN_ALT;
    NOP;
    NOP;
    if (GET_DOUT_18PIN_ALT() != 1) error(b, 1);
    CAS_HIGH_18PIN_ALT;
    RAS_HIGH_18PIN_ALT;
  }

  // COLUMNS
  for (uint8_t b = 0; b < colBits; b++) {
    uint16_t base_col = 0, peer_col = (1U << b);

    setAddr_18Pin_Alt(0);
    RAS_LOW_18PIN_ALT;

    SET_DIN_18PIN_ALT(0);
    setAddr_18Pin_Alt(base_col);
    WE_LOW_18PIN_ALT;
    CAS_LOW_18PIN_ALT;
    NOP;
    CAS_HIGH_18PIN_ALT;

    SET_DIN_18PIN_ALT(1);
    setAddr_18Pin_Alt(peer_col);
    WE_LOW_18PIN_ALT;
    CAS_LOW_18PIN_ALT;
    NOP;
    CAS_HIGH_18PIN_ALT;

    WE_HIGH_18PIN_ALT;
    RAS_HIGH_18PIN_ALT;

    // verify
    setAddr_18Pin_Alt(0);
    RAS_LOW_18PIN_ALT;

    setAddr_18Pin_Alt(base_col);
    CAS_LOW_18PIN_ALT;
    NOP;
    NOP;
    if (GET_DOUT_18PIN_ALT() != 0) error(b + 16, 1);
    CAS_HIGH_18PIN_ALT;

    setAddr_18Pin_Alt(peer_col);
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
      delayMicroseconds(ramTypes[type].writeTime);  // Simulate writing even if it is no longer done for the last rows
      delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows]);
    }
    return;
  } else if (row >= ramTypes[type].delayRows) {
    checkRow_18Pin_Alt(row - ramTypes[type].delayRows, patNr, 3);
    delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows]);
    return;
  }
  delayMicroseconds(ramTypes[type].delays[row]);
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
static inline __attribute__((always_inline)) void refreshRow_18Pin_Alt(uint16_t row) {
  rasHandling_18Pin_Alt(row);
  RAS_HIGH_18PIN_ALT;
}
