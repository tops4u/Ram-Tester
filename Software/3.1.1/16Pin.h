
// 16Pin.h - Header for 16-Pin DRAM functions and definitions
//=======================================================================================

#ifndef PIN16_H
#define PIN16_H

#include "common.h"
#include <avr/io.h>       // for SBI/CBI macros
#include <avr/pgmspace.h> // for PROGMEM

//=======================================================================================
// 16-PIN SPECIFIC DEFINES AND MACROS
//=======================================================================================

// Pin mappings for 16-Pin DRAMs (4164, 41256)
extern const int CPU_16PORTB[];
extern const int CPU_16PORTC[];
extern const int CPU_16PORTD[];

// RAS/CAS Pin definitions
extern const int RAS_16PIN;  // Digital Out 9
extern const int CAS_16PIN;  // Analog 3 / Digital 17

// Control signal macros
#define CAS_LOW16  CBI(PORTC, 3)
#define CAS_HIGH16 SBI(PORTC, 3)
#define RAS_LOW16  CBI(PORTB, 1)
#define RAS_HIGH16 SBI(PORTB, 1)
#define WE_LOW16   CBI(PORTB, 3)
#define WE_HIGH16  SBI(PORTB, 3)

// Mapping for 4164 (2ms Refresh Rate) / 41256/257 (4 ms Refresh Rate)
// A0 = PC4   RAS = PB1   t RAS->CAS = 150-200ns -> Max Pulsewidth 10'000ns
// A1 = PD1   CAS = PC3   t CAS->dOut= 75 -100ns -> Max Pulsewidth 10'000ns
// A2 = PD0   WE  = PB3
// A3 = PB2   Din = PC1
// A4 = PB4   Dout= PC2
// A5 = PD7
// A6 = PB0
// A7 = PD6
// A8 = PC0 (only 41256/257 Type, on 4164 this Pin is NC)

// Address setting macro
#define SET_ADDR_PIN16(addr) \
  { \
    PORTB = ((PORTB & 0xEA) | (addr & 0x0010) | ((addr & 0x0008) >> 1) | ((addr & 0x0040) >> 6)); \
    PORTC = ((PORTC & 0xE8) | ((addr & 0x0001) << 4) | ((addr & 0x0100) >> 8)); \
    PORTD = ((addr & 0x0080) >> 1) | ((addr & 0x0020) << 2) | ((addr & 0x0004) >> 2) | (addr & 0x0002); \
  }

//=======================================================================================
// 16-PIN LOOKUP TABLE STRUCTURES
//=======================================================================================

// Packed struct for 24-bit data (3 bytes per entry)
struct __attribute__((packed)) port_config16_t {
  uint8_t portb;
  uint8_t portc;  // Without PC1 (DIN) - set separately
  uint8_t portd;
};

// Lookup table for optimized address setting
extern const struct port_config16_t PROGMEM lut_combined[512];

//=======================================================================================
// 16-PIN FUNCTION PROTOTYPES
//=======================================================================================

// Main test functions
/**
 * test_16Pin — see implementation for details.
 * @return void
 */
void test_16Pin(void);
static void sense41256_16Pin(void);
static void checkAddressing_16Pin(void);
/**
 * ram_present_16Pin — see implementation for details.
 * @return 
 */
static bool ram_present_16Pin(void);

// Address and data handling
/**
 * rasHandling_16Pin — see implementation for details.

 * @param row 
 * @return void
 */
static void rasHandling_16Pin(uint16_t row);

// Write/Read functions
/**
 * writeRow_16Pin — see implementation for details.

 * @param row 
 * @param cols 
 * @param patNr 
 * @return void
 */
static void writeRow_16Pin(uint16_t row, uint16_t cols, uint8_t patNr);
static void refreshRow_16Pin(uint16_t row);
static void checkRow_16Pin(uint16_t cols, uint16_t row, uint8_t patNr, uint8_t check);

// RAM presence tests (static - internal to 16Pin.cpp)
// static bool ram_present_16pin(void); // declared in .cpp file

//=======================================================================================
// 16-PIN INLINE FUNCTIONS
//=======================================================================================

// Optimized version with lookup table + inline assembly
static void __attribute__((hot)) setAddrData(uint16_t addr, uint8_t dataBit) {
  const struct port_config16_t *entry = &lut_combined[addr];

  __asm__ volatile(
    // Sequential PROGMEM-Reads (uses Z+ auto-increment)
    "lpm r18, Z+     \n\t"  // portb (1 cycle)
    "lpm r19, Z+     \n\t"  // portc (1 cycle)
    "lpm r20, Z+     \n\t"  // portd (1 cycle)

    // PORTB update
    "in  r21, %[portb] \n\t"
    "andi r21, 0xEA    \n\t"
    "or  r21, r18      \n\t"
    "out %[portb], r21 \n\t"

    // PORTD update
    "out %[portd], r20 \n\t"

    // PORTC update with optimized DIN-Branch
    "in  r21, %[portc] \n\t"
    "andi r21, 0xE0    \n\t"  // Clear CAS Line
    "or  r21, r19      \n\t"
    "sbrs %[databit], 2 \n\t"  // Skip if bit 2 set
    "rjmp 1f           \n\t"
    "ori r21, 0x02     \n\t"  // Set PC1 (DIN)
    "1: out %[portc], r21 \n\t"

    :
    : [portb] "I"(_SFR_IO_ADDR(PORTB)),
      [portc] "I"(_SFR_IO_ADDR(PORTC)),
      [portd] "I"(_SFR_IO_ADDR(PORTD)),
      "z"(entry),
      [databit] "r"(dataBit)
    : "r18", "r19", "r20", "r21");
}


#endif // PIN16_H