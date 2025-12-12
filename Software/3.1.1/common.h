// common.h - Common definitions and functions for RAM Tester
//=======================================================================================

// ---- COMMENT THE FOLLOWING LINE TO DISABLE DISPLAY SUPPORT AND SPEED UP THE TESTER ------
#define OLED
// -----------------------------------------------------------------------------------------
// ---- UNCOMMENT THE FOLLOWING LINE TO ENABLE DIAG MODE AFTER FIRMWARE UPLOAD -------------
// #define DIAG
// ---- THIS MIGHT BE USEFULL FOR DIY PEOPLE WHO WANT TO CHECK THE SOLDERING ---------------

#ifndef COMMON_H
#define COMMON_H

#include <Arduino.h>
#include <EEPROM.h>
#include <avr/pgmspace.h>

#ifdef OLED
#include <U8x8lib.h>
extern U8X8_SSD1306_128X64_NONAME_SW_I2C display;
#endif
//=======================================================================================
// GENERAL DEFINES AND CONSTANTS
//=======================================================================================

// Pin mode definitions
#define Mode_16Pin 2
#define Mode_18Pin 4
#define Mode_20Pin 5

// Special constants
#define EOL 254
#define NC 255
#define ON HIGH
#define OFF LOW

#define LED_RED_PIN 13    // PB5
#define LED_GREEN_PIN 12  // PB4
// LED color states for bicolor LED
typedef enum {
  LED_OFF,
  LED_RED,
  LED_GREEN,
  LED_ORANGE
} LedColor;

// Timing constants
#define BLINK_ON_MS 500        // Standard blink on duration
#define BLINK_OFF_MS 500       // Standard blink off duration
#define FAST_BLINK_MS 200      // Fast blink for config errors
#define SLOW_BLINK_MS 1000     // Slow blink for no RAM
#define INTER_BLINK_MS 300     // Pause between color sequences
#define PATTERN_PAUSE_MS 2000  // Pause before pattern repeat
#define ERROR_PAUSE_MS 1500    // Pause between error pattern repeats

// EEPROM addresses
#define TESTING 0x00
#define EEPROM_POINTER_ADDR 0x10
#define EEPROM_DATA_START 0x11
#define NUM_CELLS 0xff  // Number of cells for wear leveling

// ADC parameters
#define ADC_VREF 5.0
#define TARGET_VOLTAGE 1.6
#define VOLTAGE_TOLERANCE 0.16
#define ADC_RESOLUTION 1024

//=======================================================================================
// RAM TYPE DEFINITIONS
//=======================================================================================

// RAM Type Numbers
#define T_4164 0
#define T_41256 1
#define T_41257 2
#define T_4416 3
#define T_4464 4
#define T_514256 5
#define T_514258 6
#define T_514400 7
#define T_514402 8
#define T_411000 9
#define T_4116 10
#define T_4816 11
#define T_4027 12

// Test Patterns
extern const uint8_t pattern[6];

// Random Table for pseudo-random testing
extern uint8_t randomTable[256];

//=======================================================================================
// STRUCTURES
//=======================================================================================

// Structure defining characteristics of different RAM types
struct RAM_Definition {
  char name[15];        // Name for the Display
  uint8_t mSRetention;  // This Tester's assumptions of the Retention Time in ms
  uint8_t delayRows;    // How many rows are skipped before reading back and checking retention time
  uint16_t rows;        // How many rows does this type have
  uint16_t columns;     // How many columns does this type have
  uint8_t iOBits;       // How many Bits can this type I/O at the same time
  bool staticColumn;    // Is this a Static Column Type
  bool nibbleMode;      // Is this a nibble Mode Type --> Not yet implemented
  bool smallType;       // Is this the small type for this amount of pins
  uint16_t delays[6];   // List of specific delay times for retention testing
  uint16_t writeTime;   // Write Time to check last rows during retention testing
};

extern struct RAM_Definition ramTypes[];

//=======================================================================================
// GLOBAL VARIABLES
//=======================================================================================

extern int type;
extern uint8_t Mode;
extern uint8_t red;
extern uint8_t green;

//=======================================================================================
// FUNCTION PROTOTYPES - LED and Error Handling
//=======================================================================================

/**
 * setupLED — see implementation for details.
 * @return void
 */
void setupLED(void);
void error(uint8_t code, uint8_t error, int16_t row = -1, int16_t col = -1);
void testOK(void);
/**
 * ConfigFail — see implementation for details.
 * @return void
 */
void ConfigFail(void);
void buildTest(void);
void oledDrawCentered(uint8_t row, const char *s);

//=======================================================================================
// FUNCTION PROTOTYPES - EEPROM and Data Management
//=======================================================================================

/**
 * eeprom_counter — see implementation for details.
 * @return 
 */
uint8_t eeprom_counter(void);
void randomizeData(void);

//=======================================================================================
// FUNCTION PROTOTYPES - ADC Functions
//=======================================================================================

/**
 * adc_init — see implementation for details.
 * @return void
 */
void adc_init(void);
uint16_t adc_read(uint8_t channel);
float adc_to_voltage(uint16_t adc_value);

//=======================================================================================
// FUNCTION PROTOTYPES - Ground Short Check
//=======================================================================================

/**
 * checkGNDShort — see implementation for details.
 * @return void
 */
void checkGNDShort(void);
void checkGNDShort4Port(const int *portb, const int *portc, const int *portd);

// Forward declarations for port mappings (defined in respective pin modules)
extern const int CPU_16PORTB[];
extern const int CPU_16PORTC[];
extern const int CPU_16PORTD[];
extern const int CPU_18PORTB[];
extern const int CPU_18PORTC[];
extern const int CPU_18PORTD[];
extern const int CPU_20PORTB[];
extern const int CPU_20PORTC[];
extern const int CPU_20PORTD[];

//=======================================================================================
// FUNCTION PROTOTYPES - RAM Initialization
//=======================================================================================

/**
 * initRAM — see implementation for details.

 * @param RASPin 
 * @param CASPin 
 * @return void
 */
void initRAM(int RASPin, int CASPin);
void writeRAMType(void);

//=======================================================================================
// INLINE ASSEMBLY MACROS
//=======================================================================================

// Additional delay of 62.5ns may be required for compatibility. (16MHz clock = 1 cycle = 62.5ns).
#define NOP __asm__ __volatile__("nop\n\t")

// SBI - Set Bit in I/O Register (inline assembly for fast bit manipulation)
#define SBI(port, bit) __asm__ __volatile__("sbi %0, %1" ::"I"(_SFR_IO_ADDR(port)), "I"(bit))

// CBI - Clear Bit in I/O Register (inline assembly for fast bit manipulation)
#define CBI(port, bit) __asm__ __volatile__("cbi %0, %1" ::"I"(_SFR_IO_ADDR(port)), "I"(bit))

//=======================================================================================
// UTILITY INLINE FUNCTIONS
//=======================================================================================

// Fast 8-bit left rotation using inline assembly
static inline uint8_t __attribute__((always_inline, hot)) rotate_left(uint8_t val) {
  __asm__ __volatile__(
    "lsl %0 \n\t"           // Logical Shift Left (Bit 7 → Carry)
    "adc %0, __zero_reg__"  // Add with Carry (Carry → Bit 0)
    : "=r"(val)             // Output: val is modified
    : "0"(val)              // Input: val as input value
  );
  return val;
}

// Randomize the access to the pseudo random table by using col & row
static inline __attribute__((always_inline)) uint8_t mix8(uint16_t col, uint16_t row) {
  uint16_t v = col ^ (row + (row >> 4));  
  return (uint8_t)(v ^ (v >> 8));
}

#endif  // COMMON_H