#include "16Pin.h"

//=======================================================================================
// 16-PIN PORT MAPPINGS
//=======================================================================================

// Port-to-DIP-Pin mappings for 16-pin DRAMs
const int CPU_16PORTB[] = { 13, 4, 12, 3, EOL, EOL, EOL, EOL };
const int CPU_16PORTC[] = { 1, 2, 14, 15, 5, EOL, EOL, EOL };
const int CPU_16PORTD[] = { 6, 7, 8, NC, NC, NC, 9, 10 };

// RAS/CAS Pin definitions
const int RAS_16PIN = 9;   // Digital Out 9
const int CAS_16PIN = 17;  // Analog 3 / Digital 17

//=======================================================================================
// 16-PIN LOOKUP TABLE GENERATION
//=======================================================================================

// Macros for LUT generation
#define MAP_COMBINED(a) \
  { \
    .portb = ((((a)&0x0010)) | (((a)&0x0008) >> 1) | (((a)&0x0040) >> 6)), \
    .portc = ((((a)&0x0001) << 4) | (((a)&0x0100) >> 8)), \
    .portd = ((((a)&0x0080) >> 1) | (((a)&0x0020) << 2) | (((a)&0x0004) >> 2) | ((a)&0x0002)) \
  }

#define ROW16_COMBINED(base) \
  MAP_COMBINED(base + 0), MAP_COMBINED(base + 1), MAP_COMBINED(base + 2), MAP_COMBINED(base + 3), \
    MAP_COMBINED(base + 4), MAP_COMBINED(base + 5), MAP_COMBINED(base + 6), MAP_COMBINED(base + 7), \
    MAP_COMBINED(base + 8), MAP_COMBINED(base + 9), MAP_COMBINED(base + 10), MAP_COMBINED(base + 11), \
    MAP_COMBINED(base + 12), MAP_COMBINED(base + 13), MAP_COMBINED(base + 14), MAP_COMBINED(base + 15)

#define ROW256_COMBINED(base) \
  ROW16_COMBINED(base + 0), ROW16_COMBINED(base + 16), \
    ROW16_COMBINED(base + 32), ROW16_COMBINED(base + 48), \
    ROW16_COMBINED(base + 64), ROW16_COMBINED(base + 80), \
    ROW16_COMBINED(base + 96), ROW16_COMBINED(base + 112), \
    ROW16_COMBINED(base + 128), ROW16_COMBINED(base + 144), \
    ROW16_COMBINED(base + 160), ROW16_COMBINED(base + 176), \
    ROW16_COMBINED(base + 192), ROW16_COMBINED(base + 208), \
    ROW16_COMBINED(base + 224), ROW16_COMBINED(base + 240)

#define GEN512_COMBINED ROW256_COMBINED(0), ROW256_COMBINED(256)

// 512-entry Lookup Table (1536 Bytes in PROGMEM)
const struct port_config16_t PROGMEM lut_combined[512] = {
  GEN512_COMBINED
};

static bool ram_present_16Pin(void) {
  uint8_t sDDRB = DDRB, sPORTB = PORTB, sDDRC = DDRC, sPORTC = PORTC, sDDRD = DDRD, sPORTD = PORTD;

  // Write 0 to address 0
  CAS_HIGH16;
  rasHandling_16Pin(0);
  WE_LOW16;
  PORTC &= ~0x02;  // DIN = LOW
  CAS_LOW16;
  NOP;
  CAS_HIGH16;
  WE_HIGH16;
  RAS_HIGH16;

  // Read back address 0 -> should be 0
  rasHandling_16Pin(0);
  CAS_LOW16;
  NOP;
  NOP;
  uint8_t r0 = (PINC & 0x04) >> 2;
  CAS_HIGH16;
  RAS_HIGH16;

  // Write 1 to address 1
  rasHandling_16Pin(1);
  WE_LOW16;
  PORTC |= 0x02;  // DIN = HIGH
  CAS_LOW16;
  NOP;
  CAS_HIGH16;
  WE_HIGH16;
  RAS_HIGH16;

  // Read back address 1 -> should be 1
  rasHandling_16Pin(1);
  CAS_LOW16;
  NOP;
  NOP;
  uint8_t r1 = (PINC & 0x04) >> 2;
  CAS_HIGH16;
  RAS_HIGH16;

  // Verify address 0 is still 0 (no aliasing)
  rasHandling_16Pin(0);
  CAS_LOW16;
  NOP;
  NOP;
  uint8_t r2 = (PINC & 0x04) >> 2;
  CAS_HIGH16;
  RAS_HIGH16;

  // Restore original state
  DDRB = sDDRB;
  PORTB = sPORTB;
  DDRC = sDDRC;
  PORTC = sPORTC;
  DDRD = sDDRD;
  PORTD = sPORTD;

  return (r0 == 0 && r1 == 1 && r2 == 0);
}

//=======================================================================================
// ERWEITERTE DRAM DETECTION MIT T_4816 SUPPORT
//=======================================================================================

/**
 * Extended Chip-Detection für 16-Pin DRAMs
 * Unterscheidet zwischen 4164 (64Kx1), 41256 (256Kx1) und 4816 (16Kx1, no A7)
 */
void sense41256_16Pin() {
  CAS_HIGH16;

  // --- SCHRITT 1: A8 Test (4164 vs 41256/4816) ---
  // Write 0 in Row 0
  rasHandling_16Pin(0);
  PORTC &= ~0x02;  // DIN = LOW
  WE_LOW16;
  CAS_LOW16;
  NOP;
  CAS_HIGH16;
  WE_HIGH16;
  RAS_HIGH16;

  // Write 1 in Row 256 (A7=1)
  rasHandling_16Pin(256);
  PORTC |= 0x02;  // DIN = HIGH
  WE_LOW16;
  CAS_LOW16;
  NOP;
  CAS_HIGH16;
  WE_HIGH16;
  RAS_HIGH16;

  // Read Row 0 back
  rasHandling_16Pin(0);
  CAS_LOW16;
  NOP;
  NOP;
  uint8_t a7_test = (PINC & 0x04) >> 2;
  CAS_HIGH16;
  RAS_HIGH16;

  if (a7_test == 1) {
    // A7 ist aliased -> könnte 4164 oder 4816 sein
    // Weitere Tests notwendig

    // --- SCHRITT 2: A6 Test (4164 vs 4816) ---
    // Write 0 in Row 0
    rasHandling_16Pin(0);
    PORTC &= ~0x02;  // DIN = LOW
    WE_LOW16;
    CAS_LOW16;
    NOP;
    CAS_HIGH16;
    WE_HIGH16;
    RAS_HIGH16;

    // Write 1 in Row 128 (A7=1)
    rasHandling_16Pin(128);
    PORTC |= 0x02;  // DIN = HIGH
    WE_LOW16;
    CAS_LOW16;
    NOP;
    CAS_HIGH16;
    WE_HIGH16;
    RAS_HIGH16;

    // Read Row 0 back
    rasHandling_16Pin(0);
    CAS_LOW16;
    NOP;
    NOP;
    uint8_t a6_test = (PINC & 0x04) >> 2;
    CAS_HIGH16;
    RAS_HIGH16;

    if (a6_test == 1) {
      // A6 ist auch aliased -> 4816 (no A6 Pin)
      type = T_4816;
    } else {
      // A6 funktional, aber A7 aliased -> 4164
      type = T_4164;
    }
  } else {
    // A7 funktional -> 41256
    type = T_41256;
  }
}

//=======================================================================================
// ANGEPASSTE ADDRESS TESTING MIT T_4816 SUPPORT
//=======================================================================================

void checkAddressing_16Pin(void) {
  uint16_t max_rows = ramTypes[type].rows;
  uint16_t max_cols = ramTypes[type].columns;

  // Berechne Anzahl der Adressbits
  uint8_t row_bits = 0, col_bits = 0;
  for (uint16_t t = max_rows - 1; t; t >>= 1) row_bits++;
  for (uint16_t t = max_cols - 1; t; t >>= 1) col_bits++;

  // Configure I/O
  DDRB = 0b00111111;
  PORTB = 0b00101010;
  DDRC = 0b00011011;
  PORTC = 0b00001000;
  DDRD = 0b11000011;
  PORTD = 0x00;

  CAS_HIGH16;
  RAS_HIGH16;
  WE_LOW16;

  // -------- ROW ADDRESS DECODER TEST --------
  for (uint8_t b = 0; b < row_bits; b++) {
    uint16_t base_row = 0;
    uint16_t peer_row = (1u << b);
    const uint16_t test_col = 0;

    // Write base_row = 0
    SET_ADDR_PIN16(base_row);
    RAS_LOW16;
    PORTC &= ~0x02;  // DIN = 0
    WE_LOW16;
    setAddrData(test_col, 0);
    CAS_LOW16;
    NOP;
    CAS_HIGH16;
    WE_HIGH16;
    RAS_HIGH16;

    // Write peer_row = 1
    SET_ADDR_PIN16(peer_row);
    RAS_LOW16;
    PORTC |= 0x02;  // DIN = 1
    WE_LOW16;
    setAddrData(test_col, 0x04);
    CAS_LOW16;
    NOP;
    CAS_HIGH16;
    WE_HIGH16;
    RAS_HIGH16;

    // Verifikation base_row -> 0
    SET_ADDR_PIN16(base_row);
    RAS_LOW16;
    setAddrData(test_col, 0);
    CAS_LOW16;
    NOP;
    NOP;
    if (((PINC & 0x04) >> 2) != 0) {
      error(b, 1);
    }
    CAS_HIGH16;
    RAS_HIGH16;

    // Verifikation peer_row -> 1
    SET_ADDR_PIN16(peer_row);
    RAS_LOW16;
    setAddrData(test_col, 0);
    CAS_LOW16;
    NOP;
    NOP;
    if (((PINC & 0x04) >> 2) != 1) {
#ifdef DEBUG_ADDRESSING
      Serial.print("ROW bit ");
      Serial.print(b);
      Serial.println(" test failed!");
#endif
      error(b, 1);
    }
    CAS_HIGH16;
    RAS_HIGH16;
  }

  // -------- COLUMN ADDRESS DECODER TEST --------
  uint16_t test_row = max_rows >> 1;
  SET_ADDR_PIN16(test_row);
  RAS_LOW16;
  WE_LOW16;

  // Write Phase
  for (uint8_t b = 0; b < col_bits; b++) {
    uint16_t base_col = 0;
    uint16_t peer_col = (1u << b);

    setAddrData(base_col, 0);
    CAS_LOW16;
    NOP;
    CAS_HIGH16;

    setAddrData(peer_col, 0x04);
    CAS_LOW16;
    NOP;
    CAS_HIGH16;
  }
  WE_HIGH16;
  RAS_HIGH16;

  // Verify Phase
  SET_ADDR_PIN16(test_row);
  RAS_LOW16;
  for (uint8_t b = 0; b < col_bits; b++) {
    uint16_t base_col = 0;
    uint16_t peer_col = (1u << b);

    setAddrData(base_col, 0);
    CAS_LOW16;
    NOP;
    NOP;
    if (((PINC & 0x04) >> 2) != 0) {
#ifdef DEBUG_ADDRESSING
      Serial.print("COL bit ");
      Serial.print(b);
      Serial.println(" test failed!");
#endif
      error(b + 16, 1);
    }
    CAS_HIGH16;

    setAddrData(peer_col, 0);
    CAS_LOW16;
    NOP;
    NOP;
    if (((PINC & 0x04) >> 2) != 1) {
#ifdef DEBUG_ADDRESSING
      Serial.print("COL bit ");
      Serial.print(b);
      Serial.println(" test failed!");
#endif
      error(b + 16, 1);
    }
    CAS_HIGH16;
  }
  RAS_HIGH16;

#ifdef DEBUG_ADDRESSING
  Serial.println("Address testing completed successfully");
#endif
}

//=======================================================================================
// MAIN TEST FUNCTION
//=======================================================================================

/**
 * Main test function for 16-pin DRAMs (4164, 41256, 4816)
 * Configures I/O ports, detects RAM type, performs address testing and pattern tests
 */
void test_16Pin() {
  // Configure I/O for this Chip Type
  DDRB = 0b00111111;
  PORTB = 0b00101010;
  DDRC = 0b00011011;
  PORTC = 0b00001000;
  DDRD = 0b11000011;
  PORTD = 0x00;

  if (!ram_present_16Pin())
    error(0, 0);

  sense41256_16Pin();  // Erweiterte Erkennung mit T_4816 Support

  writeRAMType();
  // Redo because otherwise u8x8 interferes with the Test
  DDRB = 0b00111111;
  PORTB = 0b00101010;

  checkAddressing_16Pin();

  for (uint8_t patNr = 0; patNr <= 5; patNr++) {
    if (patNr == 5)
      randomizeData();
    for (uint16_t row = 0; row < ramTypes[type].rows; row++) {
      writeRow_16Pin(row, ramTypes[type].columns, patNr);
    }
  }
  testOK();
}

/**
 * Prepare and execute ROW Access for 16 Pin Types
 * Sets row address and activates RAS signal
 * @param row Row address to access (0 to max_rows-1)
 */
void rasHandling_16Pin(uint16_t row) {
  RAS_HIGH16;
  SET_ADDR_PIN16(row);
  RAS_LOW16;
}

/**
 * Write and verify pattern data to a complete row
 * Handles different test patterns including stuck-at, walking patterns, and pseudo-random data
 * @param row Row address to write/test
 * @param cols Number of columns in this RAM type
 * @param patNr Pattern number (0-4): 0=all zeros, 1=all ones, 2,3=walking patterns, 4=pseudo-random
 */
void writeRow_16Pin(uint16_t row, uint16_t cols, uint8_t patNr) {
  // Prepare Write Cycle
  CAS_HIGH16;
  rasHandling_16Pin(row);
  WE_LOW16;
  uint8_t pat = pattern[patNr];
  cli();
  uint16_t col = cols;

  if (patNr < 2) {
    while (col > 0) {
      setAddrData(--col, pat);
      CAS_HIGH16;
      WE_HIGH16;
      CAS_LOW16;
      CAS_HIGH16;
      if (((PINC ^ pat) & 0x04) != 0) {
        sei();
        error(patNr + 1, 2);
        return;
      }
      WE_LOW16;
    }
    sei();
    return;
  } else if (patNr < 4) {
    do {
      setAddrData(--col, pat);
      pat = rotate_left(pat);
      CAS_HIGH16;
    } while (col != 0);
  } else {
    do {
      CAS_HIGH16;
      col--;
      setAddrData(col, randomTable[mix8(col, row)]);
    } while (col != 0);
  }

  CAS_HIGH16;
  sei();
  WE_HIGH16;

  if (patNr < 4) {
    checkRow_16Pin(cols, row, patNr, 2);
    return;
  }

  refreshRow_16Pin(row);
  if (row == ramTypes[type].rows - 1) {
    for (int8_t x = ramTypes[type].delayRows; x >= 0; x--) {
      rasHandling_16Pin(row - x);
      checkRow_16Pin(cols, row - x, patNr, 3);
      delayMicroseconds(ramTypes[type].writeTime);
      delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows]);
    }
    return;
  } else if (row >= ramTypes[type].delayRows) {
    rasHandling_16Pin(row - ramTypes[type].delayRows);
    checkRow_16Pin(cols, row - ramTypes[type].delayRows, patNr, 3);
    delayMicroseconds(ramTypes[type].delays[ramTypes[type].delayRows]);
    return;
  } else {
    delayMicroseconds(ramTypes[type].delays[row]);
  }
}

/**
 * Refresh a specific row by performing RAS-only cycle
 * @param row Row address to refresh
 */
void refreshRow_16Pin(uint16_t row) {
  rasHandling_16Pin(row);
  RAS_HIGH16;
}

/**
 * Read and verify data from a complete row
 * Supports different pattern types and optimized loop unrolling for performance
 * @param cols Number of columns to check
 * @param row Row address being tested
 * @param patNr Pattern number (0-4)
 * @param check Error code to report if check fails (2=write error, 3=retention error)
 */
void checkRow_16Pin(uint16_t cols, uint16_t row, uint8_t patNr, uint8_t check) {
  register uint8_t pat = pattern[patNr];
  cli();

  if (patNr < 4) {
    register uint16_t col = cols;
    register uint16_t col4 = col & ~3;

    while (col > col4) {
      // Unrolled read iterations
      setAddrData(--col, 0);
      CAS_HIGH16;
      if (((PINC ^ pat) & 0x04) != 0) {
        sei();
        error(patNr + 1, check);
        return;
      }
      pat = rotate_left(pat);

      setAddrData(--col, 0);
      CAS_HIGH16;
      if (((PINC ^ pat) & 0x04) != 0) {
        sei();
        error(patNr + 1, check);
        return;
      }
      pat = rotate_left(pat);

      setAddrData(--col, 0);
      CAS_HIGH16;
      if (((PINC ^ pat) & 0x04) != 0) {
        sei();
        error(patNr + 1, check);
        return;
      }
      pat = rotate_left(pat);

      setAddrData(--col, 0);
      CAS_HIGH16;
      if (((PINC ^ pat) & 0x04) != 0) {
        sei();
        error(patNr + 1, check);
        return;
      }
      pat = rotate_left(pat);
    }

    while (col > 0) {
      setAddrData(--col, 0);
      CAS_HIGH16;
      if (((PINC ^ pat) & 0x04) != 0) {
        sei();
        error(patNr + 1, check);
        return;
      }
      pat = rotate_left(pat);
    }
  } else {
    register uint16_t col = cols;
    do {
      setAddrData(--col, 0);
      CAS_HIGH16;
      if (((PINC ^ randomTable[mix8(col, row)]) & 0x04) != 0) {
        sei();
        error(patNr + 1, check);
        return;
      }
    } while (col != 0);
  }

  sei();
  RAS_HIGH16;
}