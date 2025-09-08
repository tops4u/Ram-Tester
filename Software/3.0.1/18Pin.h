// 18Pin.h - Header for 18-Pin DRAM functions and definitions
//=======================================================================================

#ifndef PIN18_H
#define PIN18_H

#include "common.h"
#include <avr/io.h>       // for SBI/CBI macros
#include <avr/pgmspace.h> // for PROGMEM

//=======================================================================================
// Pin mappings for 4416 / 4464 DRAM (18-Pin DIP candidates)
//=======================================================================================

// Port mapping arrays (defined in 18Pin.cpp)
extern const int CPU_18PORTB[];
extern const int CPU_18PORTC[];
extern const int CPU_18PORTD[];
extern const int RAS_18PIN;
extern const int CAS_18PIN;

//=======================================================================================
// Control signal macros for standard 18-pin DRAMs (4416/4464)
//=======================================================================================

#define CAS_LOW18   PORTC &= 0xfb
#define CAS_HIGH18  PORTC |= 0x04
#define RAS_LOW18   PORTC &= 0xef
#define RAS_HIGH18  PORTC |= 0x10
#define OE_LOW18    PORTC &= 0xfe
#define OE_HIGH18   PORTC |= 0x01
#define WE_LOW18    PORTB &= 0xfd
#define WE_HIGH18   PORTB |= 0x02

//=======================================================================================
// Pin mapping for KM41C1000 (alternative 18-pin socket)
//=======================================================================================

extern const uint8_t RAS_18PIN_ALT;
extern const uint8_t CAS_18PIN_ALT;

// Control signal macros for 18Pin_Alt
#define RAS_LOW_18PIN_ALT   CBI(PORTB, 3)
#define RAS_HIGH_18PIN_ALT  SBI(PORTB, 3)
#define CAS_LOW_18PIN_ALT   CBI(PORTC, 2)
#define CAS_HIGH_18PIN_ALT  SBI(PORTC, 2)
#define WE_LOW_18PIN_ALT    CBI(PORTC, 1)
#define WE_HIGH_18PIN_ALT   SBI(PORTC, 1)

// Data macros for 1-bit DRAMs (18Pin_Alt)
#define SET_DIN_18PIN_ALT(data) \
  do { if (data) SBI(PORTC, 0); else CBI(PORTC, 0); } while (0)
#define GET_DOUT_18PIN_ALT() ((PINC & 0x08) >> 3)

//=======================================================================================
// 18-PIN LOOKUP TABLE STRUCTURES
//=======================================================================================

// Compact port configuration structure for 18Pin_Alt
struct __attribute__((packed)) port_config_18Pin_Alt_t {
  uint8_t portb;
  uint8_t portd;
};

// Lookup table for 18Pin_Alt (defined in 18Pin.cpp)
extern const struct port_config_18Pin_Alt_t PROGMEM lut_18Pin_Alt_combined[1024];

//=======================================================================================
// 18-PIN FUNCTION PROTOTYPES
//=======================================================================================

// Main test functions
/**
 * test_18Pin — see implementation for details.
 * @return void
 */
void test_18Pin(void);
bool ram_present_18Pin(void);
bool ram_present_18Pin_alt(void);

// Standard 18-pin DRAM functions (4416/4464)
/**
 * sense4464_18Pin — see implementation for details.
 * @return void
 */
void sense4464_18Pin(void);
void checkAddressing_18Pin(void);
void rasHandling_18Pin(uint8_t row);
/**
 * writeRow_18Pin — see implementation for details.

 * @param row 
 * @param patNr 
 * @param width 
 * @return void
 */
void writeRow_18Pin(uint8_t row, uint8_t patNr, uint16_t width);
void refreshRow_18Pin(uint8_t row);
void checkRow_18Pin(uint16_t width, uint8_t row, uint8_t patNr, uint8_t init_shift, uint8_t errorNr);
/**
 * configDataOut_18Pin — see implementation for details.
 * @return void
 */
void configDataOut_18Pin(void);
void configDataIn_18Pin(void);

// Alternative 18-pin DRAM functions (KM41C1000 type)
/**
 * sense411000_18Pin_Alt — see implementation for details.
 * @return void
 */
void sense411000_18Pin_Alt(void);
void checkAddressing_18Pin_Alt(void);
void rasHandling_18Pin_Alt(uint16_t row);
/**
 * writeRow_18Pin_Alt — see implementation for details.

 * @param row 
 * @param patNr 
 * @return void
 */
void writeRow_18Pin_Alt(uint16_t row, uint8_t patNr);
void checkRow_18Pin_Alt(uint16_t row, uint8_t patNr, uint8_t check);
static void refreshRow_18Pin_Alt(uint16_t row);

// RAM presence tests (static - internal to 18Pin.cpp)
// static bool ram_present_18pin(void); // declared in .cpp file
// static bool ram_present_18pin_alt(void); // declared in .cpp file

//=======================================================================================
// 18-PIN INLINE FUNCTIONS
//=======================================================================================

// Inline assembly functions for standard 18-pin address and data handling
static inline __attribute__((always_inline, hot)) void setAddr18Pin(uint8_t addr) {
  __asm__ volatile(
    "in   r16, %[portb]     \n\t"
    "andi r16, 0xEB         \n\t"
    "mov  r17, %[addr]      \n\t"
    "andi r17, 0x01         \n\t"
    "lsl  r17               \n\t"
    "lsl  r17               \n\t"
    "or   r16, r17          \n\t"
    "mov  r17, %[addr]      \n\t"
    "andi r17, 0x02         \n\t"
    "lsl  r17               \n\t"
    "lsl  r17               \n\t"
    "lsl  r17               \n\t"
    "or   r16, r17          \n\t"
    "out  %[portb], r16     \n\t"

    "clr  r16               \n\t"

    "mov  r17, %[addr]      \n\t"
    "andi r17, 0x04         \n\t"
    "swap r17               \n\t"
    "lsl  r17               \n\t"
    "or   r16, r17          \n\t"

    "mov  r17, %[addr]      \n\t"
    "andi r17, 0x08         \n\t"
    "swap r17               \n\t"
    "lsr  r17               \n\t"
    "or   r16, r17          \n\t"

    "mov  r17, %[addr]      \n\t"
    "andi r17, 0x80         \n\t"
    "lsr  r17               \n\t"
    "lsr  r17               \n\t"
    "or   r16, r17          \n\t"

    "mov  r17, %[addr]      \n\t"
    "andi r17, 0x20         \n\t"
    "swap r17               \n\t"
    "andi r17, 0x0F         \n\t"
    "or   r16, r17          \n\t"

    "mov  r17, %[addr]      \n\t"
    "andi r17, 0x40         \n\t"
    "swap r17               \n\t"
    "lsr  r17               \n\t"
    "lsr  r17               \n\t"
    "or   r16, r17          \n\t"

    "mov  r17, %[addr]      \n\t"
    "andi r17, 0x10         \n\t"
    "lsr  r17               \n\t"
    "lsr  r17               \n\t"
    "or   r16, r17          \n\t"

    "out  %[portd], r16     \n\t"
    :
    : [portb] "I"(_SFR_IO_ADDR(PORTB)),
      [portd] "I"(_SFR_IO_ADDR(PORTD)),
      [addr] "r"(addr)
    : "r16", "r17");
}

static inline __attribute__((always_inline, hot)) void setData18Pin(uint8_t data) {
  __asm__ volatile(
    "in   r16, %[portb]     \n\t"
    "andi r16, 0xf6         \n\t"
    "mov  r17, %[data]      \n\t"
    "andi r17, 0x02         \n\t"
    "lsl  r17               \n\t"
    "lsl  r17               \n\t"
    "or   r16, r17          \n\t"
    "mov  r17, %[data]      \n\t"
    "andi r17, 0x04         \n\t"
    "lsr  r17               \n\t"
    "lsr  r17               \n\t"
    "or   r16, r17          \n\t"
    "out  %[portb], r16     \n\t"

    "in   r20, %[portc]     \n\t"
    "andi r20, 0xf5         \n\t"
    "mov  r17, %[data]      \n\t"
    "andi r17, 0x01         \n\t"
    "lsl  r17               \n\t"
    "or   r20, r17          \n\t"
    "mov  r17, %[data]      \n\t"
    "andi r17, 0x08         \n\t"
    "or   r20, r17          \n\t"
    "out  %[portc], r20     \n\t"
    :
    : [portb] "I"(_SFR_IO_ADDR(PORTB)),
      [portc] "I"(_SFR_IO_ADDR(PORTC)),
      [data] "r"(data)
    : "r16", "r17", "r20");
}

static inline __attribute__((always_inline, hot)) uint8_t getData18Pin(void) {
  uint8_t result;
  __asm__ volatile(
    "in   r16, %[pinc]      \n\t"
    "in   r17, %[pinb]      \n\t"

    "clr  r18               \n\t"

    "mov  r19, r16          \n\t"
    "andi r19, 0x02         \n\t"
    "lsr  r19               \n\t"
    "add  r18, r19          \n\t"

    "mov  r19, r17          \n\t"
    "andi r19, 0x08         \n\t"
    "lsr  r19               \n\t"
    "lsr  r19               \n\t"
    "add  r18, r19          \n\t"

    "mov  r19, r17          \n\t"
    "andi r19, 0x01         \n\t"
    "lsl  r19               \n\t"
    "lsl  r19               \n\t"
    "add  r18, r19          \n\t"

    "andi r16, 0x08         \n\t"
    "add  r18, r16          \n\t"

    "mov  %0, r18           \n\t"
    : "=r"(result)
    : [pinc] "I"(_SFR_IO_ADDR(PINC)),
      [pinb] "I"(_SFR_IO_ADDR(PINB))
    : "r16", "r17", "r18", "r19");
  return result;
}

// Fast address setting using LUT for 18Pin_Alt
static inline __attribute__((always_inline)) void setAddr_18Pin_Alt(uint16_t addr) {
  const struct port_config_18Pin_Alt_t *entry = &lut_18Pin_Alt_combined[addr];
  __asm__ volatile(
    "lpm r18, Z       \n\t"
    "adiw r30, 1      \n\t"
    "lpm r20, Z       \n\t"
    "sbiw r30, 1      \n\t"

    "in  r21, %[portb] \n\t"
    "andi r21, 0xEA    \n\t"
    "or  r21, r18      \n\t"
    "out %[portb], r21 \n\t"

    "in  r19, %[portc] \n\t"
    "andi r19, 0xEF    \n\t"
    "bst %A[addr], 0   \n\t"
    "bld r19, 4        \n\t"
    "out %[portc], r19 \n\t"

    "in  r21, %[portd] \n\t"
    "andi r21, 0x18    \n\t"
    "or  r21, r20      \n\t"
    "out %[portd], r21 \n\t"

    :
    : [portb] "I"(_SFR_IO_ADDR(PORTB)),
      [portc] "I"(_SFR_IO_ADDR(PORTC)),
      [portd] "I"(_SFR_IO_ADDR(PORTD)),
      [addr] "r"(addr),
      "z"(entry)
    : "r18", "r19", "r20", "r21");
}

#endif // PIN18_H