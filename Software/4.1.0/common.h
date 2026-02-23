// common.h - Common definitions and functions for RAM Tester
//=======================================================================================

// ---- COMMENT THE FOLLOWING LINE TO DISABLE DISPLAY SUPPORT AND SPEED UP THE TESTER ------
#define OLED
// -----------------------------------------------------------------------------------------
// ---- UNCOMMENT FOR 2-PAGE BUFFER: FASTER DISPLAY (+128 BYTES RAM) -----------------------
// ---- REQUIRES PATCHED U8g2 LIBRARY (see u8g2_i2c_speedfix.patch) ------------------------
#define OLED_2PAGE
// -----------------------------------------------------------------------------------------

#ifndef COMMON_H
#define COMMON_H

#include <Arduino.h>
#include <avr/pgmspace.h>

// Version String
const String version="4.1.0";

#ifdef OLED
#include <U8g2lib.h>
#ifdef OLED_2PAGE
extern U8G2_SSD1306_128X64_NONAME_2_SW_I2C display;
#else
extern U8G2_SSD1306_128X64_NONAME_1_SW_I2C display;
#endif

const unsigned char github_QR[] U8X8_PROGMEM = {0xFF, 0x3F, 0x30, 0xFC, 0xFF, 0xF3, 0xFF, 0x03, 0xFF, 0x3F, 0x30, 0xFC,
                                                0xFF, 0xF3, 0xFF, 0x03, 0x03, 0x30, 0xCF, 0x03, 0xF0, 0x33, 0x00, 0x03,
                                                0x03, 0x30, 0xCF, 0x03, 0xF0, 0x33, 0x00, 0x03, 0xF3, 0x33, 0xCC, 0xF0,
                                                0xCC, 0x30, 0x3F, 0x03, 0xF3, 0x33, 0xCC, 0xF0, 0xCC, 0x30, 0x3F, 0x03,
                                                0xF3, 0x33, 0xC3, 0xF0, 0xC3, 0x33, 0x3F, 0x03, 0xF3, 0x33, 0xC3, 0xF0,
                                                0xC3, 0x33, 0x3F, 0x03, 0xF3, 0x33, 0xCF, 0xCF, 0x30, 0x30, 0x3F, 0x03,
                                                0xF3, 0x33, 0xCF, 0xCF, 0x30, 0x30, 0x3F, 0x03, 0x03, 0x30, 0xF3, 0x30,
                                                0x30, 0x30, 0x00, 0x03, 0x03, 0x30, 0xF3, 0x30, 0x30, 0x30, 0x00, 0x03,
                                                0xFF, 0x3F, 0x33, 0x33, 0x33, 0xF3, 0xFF, 0x03, 0xFF, 0x3F, 0x33, 0x33,
                                                0x33, 0xF3, 0xFF, 0x03, 0x00, 0x00, 0xC0, 0x0C, 0xFC, 0x03, 0x00, 0x00,
                                                0x00, 0x00, 0xC0, 0x0C, 0xFC, 0x03, 0x00, 0x00, 0xCC, 0xF3, 0xFF, 0x33,
                                                0xC0, 0xCC, 0xCF, 0x00, 0xCC, 0xF3, 0xFF, 0x33, 0xC0, 0xCC, 0xCF, 0x00,
                                                0x3F, 0x00, 0xCC, 0xCC, 0x03, 0x30, 0x0F, 0x00, 0x3F, 0x00, 0xCC, 0xCC,
                                                0x03, 0x30, 0x0F, 0x00, 0x0F, 0xF3, 0xC0, 0xFF, 0xC0, 0xC3, 0x03, 0x03,
                                                0x0F, 0xF3, 0xC0, 0xFF, 0xC0, 0xC3, 0x03, 0x03, 0xF0, 0xC3, 0x0F, 0x33,
                                                0xFC, 0xC0, 0xC3, 0x00, 0xF0, 0xC3, 0x0F, 0x33, 0xFC, 0xC0, 0xC3, 0x00,
                                                0x03, 0x3C, 0xF3, 0x33, 0xCF, 0x0C, 0xC0, 0x00, 0x03, 0x3C, 0xF3, 0x33,
                                                0xCF, 0x0C, 0xC0, 0x00, 0x30, 0xCF, 0x00, 0xFF, 0xF3, 0x0C, 0x3C, 0x03,
                                                0x30, 0xCF, 0x00, 0xFF, 0xF3, 0x0C, 0x3C, 0x03, 0x3F, 0xFC, 0x3F, 0x0C,
                                                0x30, 0xFF, 0x3F, 0x00, 0x3F, 0xFC, 0x3F, 0x0C, 0x30, 0xFF, 0x3F, 0x00,
                                                0xFC, 0xC3, 0xFC, 0x00, 0x30, 0xFF, 0xC0, 0x00, 0xFC, 0xC3, 0xFC, 0x00,
                                                0x30, 0xFF, 0xC0, 0x00, 0xF3, 0x30, 0x0C, 0x3F, 0x0F, 0x00, 0x3F, 0x00,
                                                0xF3, 0x30, 0x0C, 0x3F, 0x0F, 0x00, 0x3F, 0x00, 0xCC, 0x0F, 0xCC, 0xFC,
                                                0x0F, 0xF3, 0x33, 0x00, 0xCC, 0x0F, 0xCC, 0xFC, 0x0F, 0xF3, 0x33, 0x00,
                                                0x0F, 0x3F, 0xF3, 0xC0, 0xFC, 0x3C, 0xF3, 0x03, 0x0F, 0x3F, 0xF3, 0xC0,
                                                0xFC, 0x3C, 0xF3, 0x03, 0xC3, 0xC3, 0x03, 0x33, 0x00, 0xF0, 0x03, 0x00,
                                                0xC3, 0xC3, 0x03, 0x33, 0x00, 0xF0, 0x03, 0x00, 0xFC, 0x30, 0x0F, 0x0C,
                                                0x3F, 0xFF, 0xF3, 0x03, 0xFC, 0x30, 0x0F, 0x0C, 0x3F, 0xFF, 0xF3, 0x03,
                                                0x00, 0x00, 0x3F, 0xF3, 0xC3, 0x03, 0xF3, 0x00, 0x00, 0x00, 0x3F, 0xF3,
                                                0xC3, 0x03, 0xF3, 0x00, 0xFF, 0x3F, 0x33, 0xCF, 0xC0, 0x33, 0x03, 0x03,
                                                0xFF, 0x3F, 0x33, 0xCF, 0xC0, 0x33, 0x03, 0x03, 0x03, 0x30, 0x0C, 0x30,
                                                0xF3, 0x03, 0x3F, 0x03, 0x03, 0x30, 0x0C, 0x30, 0xF3, 0x03, 0x3F, 0x03,
                                                0xF3, 0x33, 0xCF, 0xCC, 0x0C, 0xFF, 0xF3, 0x03, 0xF3, 0x33, 0xCF, 0xCC,
                                                0x0C, 0xFF, 0xF3, 0x03, 0xF3, 0x33, 0xFF, 0xF0, 0x33, 0xCC, 0x30, 0x00,
                                                0xF3, 0x33, 0xFF, 0xF0, 0x33, 0xCC, 0x30, 0x00, 0xF3, 0x33, 0x00, 0xCC,
                                                0xC0, 0x3F, 0xCC, 0x00, 0xF3, 0x33, 0x00, 0xCC, 0xC0, 0x3F, 0xCC, 0x00,
                                                0x03, 0x30, 0x0F, 0x03, 0x03, 0xC3, 0x3F, 0x00, 0x03, 0x30, 0x0F, 0x03,
                                                0x03, 0xC3, 0x3F, 0x00, 0xFF, 0x3F, 0xC0, 0xFC, 0x33, 0x00, 0xF3, 0x00,
                                                0xFF, 0x3F, 0xC0, 0xFC, 0x33, 0x00, 0xF3, 0x00};

/*
  Fontname: -Misc-Fixed-Medium-R-Normal--7-70-75-75-C-50-ISO10646-1
  Copyright: Public domain font.  Share and enjoy.
  Glyphs: 20/1848
  BBX Build Mode: 0
*/
const uint8_t u8g2_font_version_custom[201] U8G2_FONT_SECTION("u8g2_font_version_custom") = 
  "\24\0\2\2\3\3\2\4\4\4\6\0\0\6\377\6\377\0\0\0\0\0\254 \5\200\336\0.\6\322\330"
  "\214\0\60\7\363\330U\256\12\61\7\363\330%\331\32\62\12\264\330\251\230A,G\0\63\13\264\330\214\14"
  "\222F\62)\0\64\12\264\330FU\215\230A\2\65\12\264\330\14\15\66\222I\1\66\12\264\330\251\14V"
  "\224I\1\67\13\264\330\214\14b\6\61\203\10\70\12\264\330\251\230T\224I\1\71\12\264\330\251(\323\6"
  "I\1:\7\352\330\214\70\2V\10\264\330Dg\222\12e\11\244\330\251\64\62P\0i\10\363\330e$"
  "\253\1n\7\244\330\254h\6o\10\244\330\251(\223\2r\10\244\330\254\250A\6s\10\244\330\15\215\206"
  "\2\0\0\0\4\377\377\0";

/*
  Fontname: open_iconic_check_4x
  Copyright: https://github.com/iconic/open-iconic, SIL OPEN FONT LICENSE
  Glyphs: 2/5
  BBX Build Mode: 0
*/
const uint8_t custom_check_icons[170] U8G2_FONT_SECTION("custom_check_icons") =
    "\2\0\4\5\6\6\1\1\7  \0\0 \0 \0\0\0\0\0\0\215A@ \70\230\221\7\314y"
    " \244\246\326J\354\264\342\250\222b**\204\252B\246\62b*\255$\61J\42\242\246\22\252\322\13\63"
    "{b\241G\30\201\304\33\216\270\322\16Kk\251\226\36\10\347\1C\30\0BK \70\230\221\7\314y"
    " \244\246\326J\354\264\342\350\20E\216\71\10\241C\216\62\352\20\244\210BB)\241*\275\60\63\315\62"
    "\274\264*\241\224@\212(D\216\62\352\220\203\20:\346\20E\16\342J;,\255\245Zz \234\7\14"
    "a\0\0\0\0\4\377\377\0";
/*
  Fontname: open_iconic_embedded_4x
  Copyright: https://github.com/iconic/open-iconic, SIL OPEN FONT LICENSE
  Glyphs: 3/17
  BBX Build Mode: 0
*/
const uint8_t custom_embedded_icons[195] U8G2_FONT_SECTION("custom_embedded_icons") =
    "\3\0\5\5\6\6\5\2\7  \0\0 \0 \0\0\0\0\0\0\246C\62\320\207\7\33#\31\312"
    "H\206\62\222\241\214d('\70\302\11\216\62T\241\14U\254B\25\253X\204y\202\63\30\262\24\345 "
    "\350H\306\42\330\320\204\5\0G>\34\10\5\63\342\32\326\250GE(\62\25\251D\6\62\257y\216+"
    "H\321\14b\60\203\30\353 \207:\310\221\20r$\204 \10!\310\261\214U\60\202\215\205(c!\213"
    "X\310\22\30\362\201\77H\64 \10\5OB\33\370\224\215|\302h+`\210\22\30\222\6F\65\251M"
    "lb\324\242\326\304\226`\264\205\256G\7aT\201\20V \303\35\304\220\215Mh\0\0\0\0\4\377"
    "\377\0";


#define M_Font u8g2_font_7x14B_tr
#define S_Font u8g2_font_version_custom

// OLED page buffer macros to reduce boilerplate
#define OLED_BEGIN() display.firstPage(); do {
#define OLED_END() } while (display.nextPage())
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

#define LED_RED_PIN 13   // PB5
#define LED_GREEN_PIN 12 // PB4
// LED color states for bicolor LED
typedef enum
{
  LED_OFF,
  LED_RED,
  LED_GREEN,
  LED_ORANGE
} LedColor;

// Timing constants
#define BLINK_ON_MS 500       // Standard blink on duration
#define BLINK_OFF_MS 500      // Standard blink off duration
#define FAST_BLINK_MS 200     // Fast blink for config errors
#define SLOW_BLINK_MS 1000    // Slow blink for no RAM
#define INTER_BLINK_MS 300    // Pause between color sequences
#define PATTERN_PAUSE_MS 2000 // Pause before pattern repeat
#define ERROR_PAUSE_MS 1500   // Pause between error pattern repeats

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
#define T_4532 13    // TMS4532 - RAS-split half-good 4164 (7-bit RAS, A7 ignored at RAS)
#define T_3732 14    // OKI MSM3732 - CAS-split half-good 4164 (7-bit CAS, A7 ignored at CAS)

// Test Patterns
extern const uint8_t pattern[6];

// Random Table for pseudo-random testing
extern uint8_t randomTable[256];

//=======================================================================================
// STRUCTURES
//=======================================================================================

// Structure defining characteristics of different RAM types
struct RAM_Definition
{
  const char* name;    // Name for the Display (stored in PROGMEM)
  uint8_t mSRetention; // This Tester's assumptions of the Retention Time in ms
  uint8_t delayRows;   // How many rows are skipped before reading back and checking retention time
  uint16_t rows;       // How many rows does this type have
  uint16_t columns;    // How many columns does this type have
  uint8_t flags;       // Bit 0: staticColumn, Bit 1: nibbleMode, Bit 2: smallType
  uint8_t delays[6];   // Delay times in 20μs units (multiply by 20 for actual μs)
  uint8_t writeTime;   // Write time in 20μs units (multiply by 20 for actual μs)
};

// Flag bit definitions for RAM_Definition.flags
#define RAM_FLAG_STATIC_COLUMN (1 << 0)
#define RAM_FLAG_NIBBLE_MODE   (1 << 1)
#define RAM_FLAG_SMALL_TYPE    (1 << 2)

extern struct RAM_Definition ramTypes[];

//=======================================================================================
// GLOBAL VARIABLES
//=======================================================================================

extern int type;
extern const __FlashStringHelper *typeSuffix;
extern uint8_t Mode;
extern uint8_t red;
extern uint8_t green;

//=======================================================================================
// FUNCTION PROTOTYPES - LED and Error Handling
//=======================================================================================

void printQRandVersion(String s);
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

//=======================================================================================
// FUNCTION PROTOTYPES - EEPROM and Data Management
//=======================================================================================

/**
 * eeprom_counter — see implementation for details.
 * @return
 */
void invertRandomTable(void);

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
void checkGNDShort4Port(const uint8_t *portb, const uint8_t *portc, const uint8_t *portd);

// Forward declarations for port mappings (defined in respective pin modules)
extern const uint8_t CPU_16PORTB[];
extern const uint8_t CPU_16PORTC[];
extern const uint8_t CPU_16PORTD[];
extern const uint8_t CPU_18PORTB[];
extern const uint8_t CPU_18PORTC[];
extern const uint8_t CPU_18PORTD[];
extern const uint8_t CPU_20PORTB[];
extern const uint8_t CPU_20PORTC[];
extern const uint8_t CPU_20PORTD[];

//=======================================================================================
// FUNCTION PROTOTYPES - RAM Initialization
//=======================================================================================

/**
 * initRAM — see implementation for details.

 * @param RASPin
 * @param CASPin
 * @return void
 */
void initRAM(uint8_t RASPin, uint8_t CASPin);
void writeRAMType(const __FlashStringHelper* chipName);

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

// Count number of bits needed to represent a value (for address bit counting)
// Replaces repeated for loops: for (uint16_t t = val; t; t >>= 1) bits++;
static inline __attribute__((always_inline)) uint8_t countBits(uint16_t value) {
  uint8_t bits = 0;
  while (value) { bits++; value >>= 1; }
  return bits;
}

// Fast 8-bit left rotation using inline assembly
static inline uint8_t __attribute__((always_inline, hot)) rotate_left(uint8_t val)
{
  __asm__ __volatile__(
      "lsl %0 \n\t"          // Logical Shift Left (Bit 7 → Carry)
      "adc %0, __zero_reg__" // Add with Carry (Carry → Bit 0)
      : "=r"(val)            // Output: val is modified
      : "0"(val)             // Input: val as input value
  );
  return val;
}

// Randomize the access to the pseudo random table by using col & row
static inline __attribute__((always_inline)) uint8_t mix8(uint16_t col, uint16_t row)
{
  uint16_t v = col ^ (row + (row >> 4));
  return (uint8_t)(v ^ (v >> 8));
}

// Generate random table at startup to save flash memory
void generateRandomTable(void);

// Start the Health Check of the HW
void selfCheck(void);

#endif // COMMON_H