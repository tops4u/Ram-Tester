// common.h - Common definitions and functions for RAM Tester
//=======================================================================================

// ---- COMMENT THE FOLLOWING LINE TO DISABLE DISPLAY SUPPORT AND SPEED UP THE TESTER ------
#define OLED
// -----------------------------------------------------------------------------------------

#ifndef COMMON_H
#define COMMON_H

#include <Arduino.h>
#include <avr/pgmspace.h>

// Version String
#define VERSION_STR "5.0.7"

#ifdef OLED
#include "src/U8g2/U8g2lib.h"
// Always 2-page buffer mode: 256 byte RAM, 4 passes per refresh.
// (The 1-page variant is no longer carried since we ship our own stripped u8g2.)
extern U8G2_SSD1306_128X64_NONAME_2_SW_I2C display;

// QR code for the V5 documentation shortlink (https://yzjouv.s.gy/lKBul3), QR
// version 3 (29x29 modules), ECC level Q. Stored packed at 1 bit per module
// (4 bytes per row x 29 rows = 116 bytes, LSB-first within each byte); drawQR()
// expands each set module to a 2x2 box at display time. Generated AND
// roundtrip-verified (decode == URL) by tools_qrgen.py — use that tool for any
// future URL change; do not edit the bytes by hand.
const uint8_t github_QR_packed[116] U8X8_PROGMEM = {
  0x7F, 0xF8, 0xCF, 0x1F,  0x41, 0x76, 0x56, 0x10,
  0x5D, 0x51, 0x5B, 0x17,  0x5D, 0x94, 0x4D, 0x17,
  0x5D, 0x6F, 0x45, 0x17,  0x41, 0x35, 0x5C, 0x10,
  0x7F, 0x55, 0xD5, 0x1F,  0x00, 0x3A, 0x0F, 0x00,
  0x52, 0x55, 0xB5, 0x05,  0xA9, 0x5B, 0x9A, 0x1D,
  0xD6, 0x03, 0x35, 0x17,  0xBC, 0xDA, 0xAE, 0x1B,
  0xC9, 0x2B, 0x8A, 0x10,  0x17, 0x01, 0x43, 0x15,
  0xF8, 0xD0, 0x75, 0x11,  0x9F, 0x96, 0xA1, 0x0A,
  0x55, 0xE0, 0x21, 0x19,  0x0F, 0x6D, 0xF4, 0x13,
  0xFC, 0xBF, 0x86, 0x12,  0x10, 0x95, 0x17, 0x09,
  0xC3, 0x19, 0xF3, 0x13,  0x00, 0xC9, 0x11, 0x15,
  0x7F, 0x80, 0x5B, 0x19,  0x41, 0x52, 0x1E, 0x03,
  0x5D, 0xB5, 0xFC, 0x01,  0x5D, 0x52, 0x81, 0x08,
  0x5D, 0x50, 0x3D, 0x1E,  0x41, 0x05, 0xD2, 0x1A,
  0x7F, 0xD2, 0x36, 0x0B
};

/*
  Fontname: -Misc-Fixed-Medium-R-Normal--7-70-75-75-C-50-ISO10646-1
  Copyright: Public domain font.  Share and enjoy.
  Glyphs: 16/1848   (reduced set: space . 0-9 : V e r — the only chars S_Font ever
                     draws: "Ver.:" + VERSION_STR + " 32", see common.cpp setFont(S_Font).
                     Unused "Version" letters i/n/o/s stripped → -31 bytes flash.)
  BBX Build Mode: 0
*/
const uint8_t u8g2_font_version_custom[170] U8G2_FONT_SECTION("u8g2_font_version_custom") =
  "\20\0\2\2\3\3\2\4\4\4\6\0\0\6\377\6\377\0\0\0\0\0\215 \5\200\336\0.\6\322\330"
  "\214\0\60\7\363\330U\256\12\61\7\363\330%\331\32\62\12\264\330\251\230A,G\0\63\13\264\330\214\14"
  "\222F\62)\0\64\12\264\330FU\215\230A\2\65\12\264\330\14\15\66\222I\1\66\12\264\330\251\14V"
  "\224I\1\67\13\264\330\214\14b\6\61\203\10\70\12\264\330\251\230T\224I\1\71\12\264\330\251(\323\6"
  "I\1:\7\352\330\214\70\2V\10\264\330Dg\222\12e\11\244\330\251\64\62P\0"
  "r\10\244\330\254\250A\6"
  "\0\0\0\4\377\377\0";

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

// OLED page buffer macros to reduce boilerplate.
//
// OLED_PREP() restores the OPEN-DRAIN I2C idle state on SDA (PB4) and SCK
// (PB5): both released to INPUT + internal pull-up (= idle HIGH) before every
// transfer. Required because other test/config code repurposes these pins
// (DIP-switch read prep `DDRB &= 0b11100000`, or 20-pin `configureIO_20Pin()`).
// The open-drain byte driver then actively pulls them LOW as needed via DDRB
// and releases them HIGH again through the pull-up. Matches the original u8g2
// driver's INPUT_PULLUP behaviour, so no external pull-ups are required.
#define OLED_PREP() do { DDRB &= ~((1 << 4) | (1 << 5)); PORTB |= (1 << 4) | (1 << 5); } while (0)
#define OLED_BEGIN() OLED_PREP(); display.firstPage(); do {
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

// ADC threshold for 4116 adapter detection (1.6V ± 0.16V → ADC 295-360 at 5V ref)
#define ADC_4116_LOW  295
#define ADC_4116_HIGH 360

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
// TMS4532 / MSM3732: No own ramTypes[]/ledPatterns[] entries.
// Detected via quadrant error analysis during 4164 testing.
// Identified by typeSuffix (OLED name) + halfGoodBlink (LED code).
// Blink codes: 1G-5O = 4532, 1G-6O = 3732, 1G-5O+1G-6O = ambiguous.
#define T_4164_2MS 13 // 4164 variant with 2ms retention time (array index 13)
#define T_2114 14

// Random Table for pseudo-random testing (patterns 4-5). Checkerboard passes 0-1
// are generated on the fly; there is no constant pattern[] table any more.
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
extern uint8_t Mode;
extern uint8_t red;
extern uint8_t green;

//---------------------------------------------------------------------------
// Runtime configuration (EEPROM-backed, see enterConfigMode in common.cpp)
//---------------------------------------------------------------------------
// Bit layout chosen so a fresh EEPROM byte (0xFF) yields sensible defaults:
//   - bit 0 = 1  -> 32K mode ON   (positive logic)
//   - bit 1 = 1  -> Loop mode OFF (inverted logic)
//   - bit 2 = 1  -> Dim display OFF (inverted logic)
// Fresh EEPROM (0xFF) therefore equals: 32K=ON, Loop=OFF, Dim=OFF.
#define CFG_32K_ENABLED    0x01
#define CFG_LOOP_DISABLED  0x02
#define CFG_DIM_DISABLED   0x04

extern uint8_t g_config;

// Loop / stress-test run counter (1-based; testOK() increments it each loop iteration).
// Read by the pin modules to gate the ~1-minute CBR refresh-time test (loop mode only,
// every 10th run: s_runCount % 10 == 1 -> runs 1, 11, 21, ...).
extern uint32_t s_runCount;

#define CFG_32K_ACTIVE  (g_config & CFG_32K_ENABLED)
#define CFG_LOOP_ACTIVE (!(g_config & CFG_LOOP_DISABLED))
#define CFG_DIM_ACTIVE  (!(g_config & CFG_DIM_DISABLED))

extern const __FlashStringHelper *typeSuffix;
// Half-good blink code selector (set by 16Pin quadrant evaluation)
// 0=normal (use ledPatterns[type]), 1=4532 (1G-5O), 2=3732 (1G-6O), 3=ambig (1G-5O+1G-6O)
extern uint8_t halfGoodBlink;


//=======================================================================================
// FUNCTION PROTOTYPES - LED and Error Handling
//=======================================================================================

void printQRandVersion(const __FlashStringHelper *s);
void loadConfig(void);
void enterConfigMode(void) __attribute__((noreturn));
/**
 * setupLED — see implementation for details.
 * @return void
 */
void setupLED(void);
void error(uint8_t code, uint8_t error) __attribute__((noreturn));
void checkAddressShorts(uint8_t mB, uint8_t mC, uint8_t mD);
// Note: testOK CAN return when CFG_LOOP_ACTIVE is true (loop / stress-test mode).
// It only never returns when loop mode is off. Therefore no __attribute__((noreturn)).
void testOK();
// Loop-mode CBR refresh-counter phase (~60 s): the pin modules pass their per-chip
// casBeforeRasRefresh_* as the function pointer. Shows a "CBR:<sec>" OLED countdown and
// holds the LED off. cbrScreenPrep() must be called BEFORE the pass's write phase (the
// 25-60 ms full-screen render must not eat into the freshly-written array's retention).
void cbrScreenPrep(void);
void cbrRefreshPhase(void (*cbr)());
/**
 * ConfigFail — see implementation for details.
 * @return void
 */
void ConfigFail(void) __attribute__((noreturn));
void buildTest(void);

//=======================================================================================
// FUNCTION PROTOTYPES - EEPROM and Data Management
//=======================================================================================

/**
 * invertRandomTable — flips the lower nibble of every randomTable[] entry
 * (pattern 4 -> pattern 5), giving each cell both 0 and 1 in all bit positions.
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

//=======================================================================================
// FUNCTION PROTOTYPES - Ground Short Check
//=======================================================================================

/**
 * checkGNDShort — see implementation for details.
 * @return void
 */
void checkGNDShort(void);
void checkGNDShort4Port(const uint8_t *portb, const uint8_t *portc, const uint8_t *portd,
                        const uint8_t *lblb, const uint8_t *lblc, const uint8_t *lbld);

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
// FUNCTION PROTOTYPES - Half Good RAM Text Combinations
//=======================================================================================
extern const char qs_4532_4[];
extern const char qs_4532_3[];
extern const char qs_3732_H[];
extern const char qs_3732_L[];
// Ambiguous (single quadrant — could be either type):
extern const char qs_Q1[];
extern const char qs_Q2[];
extern const char qs_Q3[];
extern const char qs_Q4[];

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

// Randomize the access to the pseudo random table by using col & row
static inline __attribute__((always_inline)) uint8_t mix8(uint16_t col, uint16_t row)
{
  uint16_t v = col ^ (row + (row >> 4));
  return (uint8_t)(v ^ (v >> 8));
}

// Generate random table at startup to save flash memory
void generateRandomTable(void);

// Shared retention-aging tail for the random patterns 4-5 (called at the end of
// each writeRow_* — 16Pin, 18Pin std, 18Pin Alt, 20Pin; the 4116/4027 path keeps
// its own tail: it ages with delays[delayRows] instead of delays[5]).
// Ages the pending row(s) per ramTypes[type] (delayRows/delays/writeTime), then
// calls `check` to read them back:
//   last row:  drain x = delayRows..0 (x < delayRows adds the missing writeTime),
//              each aged by delays[5] before the check;
//   normal:    delays[5], then check(row - delayRows);
//   early row: delays[row] only (no check pending yet).
void retentionTail(uint16_t row, uint16_t last_row, uint8_t patNr,
                   void (*check)(uint16_t row, uint8_t patNr));

// Start the Health Check of the HW
void selfCheck(void);

#endif // COMMON_H