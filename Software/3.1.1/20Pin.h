// 20Pin.h - Header for 20-Pin DRAM functions and definitions
//=======================================================================================

#ifndef PIN20_H
#define PIN20_H

#include "common.h"
#include <avr/io.h>       // for SBI/CBI macros
#include <avr/pgmspace.h> // for PROGMEM

//=======================================================================================
// Pin mappings for 514256 / 514400 DRAM (20-Pin DIP candidates)
//=======================================================================================

// Pin assignment:
// A0 = PD0   RAS = PB1
// A1 = PD1   CAS = PB0
// A2 = PD2   WE  = PB3
// A3 = PD3   OE  = PB2
// A4 = PD4   IO0 = PC0
// A5 = PD5   IO1 = PC1
// A6 = PD6   IO2 = PC2
// A7 = PD7   IO3 = PC3
// A8 = PB4
// A9 = PC4 (only for 1Mx4 RAM; on 256x4 this pin is NC or missing)

// Port mapping arrays (defined in 20Pin.cpp)
extern const int CPU_20PORTB[];
extern const int CPU_20PORTC[];
extern const int CPU_20PORTD[];
extern const int RAS_20PIN;
extern const int CAS_20PIN;

//=======================================================================================
// Control signal macros for 20-pin DRAMs
//=======================================================================================

#define CAS_LOW20     CBI(PORTB, 0)
#define CAS_HIGH20    SBI(PORTB, 0)
#define RAS_LOW20     CBI(PORTB, 1)
#define RAS_HIGH20    SBI(PORTB, 1)
#define OE_LOW20      CBI(PORTB, 2)
#define OE_HIGH20     SBI(PORTB, 2)
#define WE_LOW20      CBI(PORTB, 3)
#define WE_HIGH20     SBI(PORTB, 3)

//=======================================================================================
// High address bit control for larger DRAMs
//=======================================================================================

#define SET_A8_LOW20   CBI(PORTB, 4)
#define SET_A8_HIGH20  SBI(PORTB, 4)
#define SET_A9_LOW20   CBI(PORTC, 4)
#define SET_A9_HIGH20  SBI(PORTC, 4)

//=======================================================================================
// 20-PIN FUNCTION PROTOTYPES
//=======================================================================================

// Main test functions
/**
 * test_20Pin — see implementation for details.
 * @return void
 */
void test_20Pin(void);
void configureIO_20Pin(void);
bool ram_present_20Pin(void);

// RAM detection and sensing
/**
 * senseRAM_20Pin — see implementation for details.
 * @return void
 */
void senseRAM_20Pin(void);
void senseSCRAM_20Pin(void);

// Address testing
/**
 * checkAddressing_20Pin — see implementation for details.
 * @return void
 */
void checkAddressing_20Pin(void);

// Address and timing handling
/**
 * rasHandling_20Pin — see implementation for details.

 * @param row 
 * @return void
 */
void rasHandling_20Pin(uint16_t row);
void casHandling_20Pin(uint16_t row, uint8_t patNr, boolean is_static);
static void msbHandling_20Pin(uint8_t address);

// Write/Read functions
/**
 * writeRow_20Pin — see implementation for details.

 * @param row 
 * @param pattern 
 * @param is_static 
 * @return void
 */
void writeRow_20Pin(uint16_t row, uint8_t pattern, boolean is_static);
void refreshRow_20Pin(uint16_t row);
void checkRow_20Pin(uint8_t patNr, uint16_t row, uint8_t errNr, boolean is_static);

// RAM presence tests (static - internal to 20Pin.cpp)
// static bool ram_present_20pin(void); // declared in .cpp file

//=======================================================================================
// 4116 FUNCTION PROTOTYPES (16K DRAM with adapter - uses 20-pin socket)
//=======================================================================================

/**
 * test_4116 — see implementation for details.
 * @return 
 */
bool test_4116(void);
void test_4116_logic(void);
void checkAddressing_4116(void);
/**
 * rasHandling_4116 — see implementation for details.

 * @param row 
 * @return void
 */
void rasHandling_4116(uint8_t row);
void writeRow_4116(uint8_t row, uint8_t patNr);
void checkRow_4116(uint8_t row, uint8_t patNr, uint8_t errorNr);
/**
 * refreshRow_4116 — see implementation for details.

 * @param row 
 * @return void
 */
void refreshRow_4116(uint8_t row);

//=======================================================================================
// 4116 SPECIFIC DEFINES AND MACROS (16K DRAM with adapter)
//=======================================================================================

// 4116 uses same pin mapping as 20-pin DRAMs but different timing
#define SET_DIN_4116(data) \
  do { if (data) PORTC |= _BV(PC1); else PORTC &= ~_BV(PC1); } while (0)

#define GET_DOUT_4116() (PINC & _BV(PC0))   // read RAM DOUT on PC0

// Basic delays for 4116 (conservative values for best compatibility)
#define DELAY_4116_RAS_TO_CAS() \
  do { \
    NOP; \
    NOP; \
    NOP; \
  } while (0)  // ~187ns (3 cycles)

#define DELAY_4116_CAS_LOW() \
  do { \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
  } while (0)  // ~250ns (4 cycles)

#define DELAY_4116_DATA_SETUP() \
  do { \
    NOP; \
    NOP; \
  } while (0)  // ~125ns (2 cycles)

#define DELAY_4116_PRECHARGE() \
  do { \
    NOP; \
    NOP; \
  } while (0)  // ~125ns (2 cycles)

// Extended delays for slower variants (250ns)
#define DELAY_4116_CAS_LOW_SLOW() \
  do { \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
  } while (0)  // ~375ns (6 cycles)

#define DELAY_4116_PRECHARGE_SLOW() \
  do { \
    NOP; \
    NOP; \
    NOP; \
  } while (0)  // ~187ns (3 cycles)

//=======================================================================================
// 20-PIN INLINE FUNCTIONS
//=======================================================================================

/**
 * Fast MSB (high address bits A8/A9) handling for 20-pin DRAMs
 * Uses inline assembly for optimal performance
 * @param address Upper address bits (bit 0 = A8, bit 1 = A9)
 */
static inline __attribute__((always_inline)) void msbHandling_20Pin(uint8_t address) {
  __asm__ volatile(
    "in  r16, %[portb] \n\t"
    "andi r16, 0xEF    \n\t"  // Clear A8
    "in  r17, %[portc] \n\t"
    "andi r17, 0xEF    \n\t"  // Clear A9
    "sbrc %[addr], 0   \n\t"  // Skip if bit 0 clear
    "ori r16, 0x10     \n\t"  // Set A8
    "sbrc %[addr], 1   \n\t"  // Skip if bit 1 clear
    "ori r17, 0x10     \n\t"  // Set A9
    "out %[portb], r16 \n\t"
    "out %[portc], r17 \n\t"
    :
    : [portb] "I"(_SFR_IO_ADDR(PORTB)),
      [portc] "I"(_SFR_IO_ADDR(PORTC)),
      [addr] "r"(address)
    : "r16", "r17");
}

// Helper to extract one test bit for chips with only one data line (used by 4116)
static inline __attribute__((always_inline)) uint8_t get_test_bit(uint16_t col, uint16_t row) {
  uint16_t bit_index = (col + (row << 4)) & 0x3FF;  // % 1024 via AND-mask (2^10-1)

  // Load nibble from table (~2-3 cycles)
  uint8_t nibble_val = randomTable[bit_index >> 2];  // /4 for nibble index

  // Extract bit with switch (avoids expensive variable shifts) (~1-2 cycles)
  switch (bit_index & 3) {                    // %4 for bit position in nibble
    case 0: return nibble_val & 0x04;         // Bit 2
    case 1: return (nibble_val >> 1) & 0x04;  // Bit 3
    case 2: return (nibble_val << 1) & 0x04;  // Bit 1
    case 3: return (nibble_val << 2) & 0x04;  // Bit 0
    default: return 0;                        // Should never be reached
  }
  // Total: ~11-15 cycles
}

/**
 * Set address for 4116 - uses same pin mapping as 20-pin DRAMs
 * @param addr 7-bit address (A0-A6)
 */
static inline __attribute__((always_inline)) void setAddr_4116(uint8_t addr) {
  PORTD = addr & 0x7F;  // A0-A6 on PD0-PD6 (bit 7 unused)
}

#endif // PIN20_H