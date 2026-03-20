# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Flash

**No Arduino CLI is available on this machine.** The pre-compiled binary `Ram_Tester_4_1_0.hex` must be used as-is; do not attempt local compilation.

Flash to ATmega328P via Arduino-as-ISP programmer:
```bash
# Linux/Mac (adjust port as needed)
avrdude -p atmega328p -c stk500v1 -P /dev/ttyUSB0 -b 19200 \
  -U lfuse:w:0xFF:m -U hfuse:w:0xD9:m -U efuse:w:0xFD:m \
  -U flash:w:Ram_Tester_4_1_0.hex:i

# Windows: edit flash.bat (default COM3)
```

**Fuse bytes:** Low=0xFF (16MHz XTAL), High=0xD9 (512B bootloader), Extended=0xFD (4.3V BOD).

**Note:** `flash.sh` currently references an outdated hex filename (`Ram_Tester_4_0_2.hex`) and should be updated to `Ram_Tester_4_1_0.hex`.

## Project Overview

DRAM tester for vintage chips on an ATmega328P @ 16MHz. Tests chips via DIP switch mode selection:

| DIP Switch | Mode | Chips Tested |
|---|---|---|
| D2 HIGH (value 2) | 16-pin | 4164, 41256, 41257, 4816, TMS4532, MSM3732 |
| D3 HIGH (value 3) | 18-pin | 4416, 4464, 411000 |
| A5 HIGH (value 5) | 20-pin | 514256, 514258, 514400, 514402, 4116, 4027 |

## Code Architecture

### Module Structure
- **`.ino`**: `setup()` reads DIP switches, routes to mode-specific test. `loop()` is a safety fallback only.
- **`common.h/cpp`**: All shared definitions — `RAM_Definition` struct (15 chip types), LED blink patterns, LFSR random table generation, `error()`, `testOK()`, `initRAM()`, `checkGNDShort()`.
- **`16Pin.h/cpp`**: Tests for 16-pin DRAM variants.
- **`18Pin.h/cpp`**: Tests for 18-pin variants (4416/4464 standard interface + 411000 alternative interface).
- **`20Pin.h/cpp`**: Tests for 20-pin variants; also handles 4116/4027 via voltage-detected adapter.

### Key Architectural Patterns

**1. Port-Ordered Iteration (critical optimization)**
All `fastPatternTest_*` functions iterate columns in port-natural order so the inner loop reduces to a single `PORTD` write + CAS toggle. Example for 20Pin (simplest): A0-A7 map directly to PD0-PD7, so `PORTD = col`. For random patterns, logical column must be reconstructed from port indices for `randomTable[]` lookup.

**2. Split Lookup Tables**
Flash memory is conserved by splitting address LUTs:
- 16Pin: 32-entry low table (A0-A4) + 16-entry high table (A5-A8) instead of 512 entries
- 18Pin alt (411000): 128+8 entries instead of 1024

**3. Port Preservation Masks**
Every port write uses `PORTB = (PORTB & mask) | newBits` to avoid disturbing control signals (RAS, WE, CAS, LED) on the same port. Masks are chip-specific — always check before modifying port writes.

**4. Direct Port Manipulation & Inline Assembly**
All timing-critical paths use `SBI`/`CBI` macros or `__asm__ volatile` — never `digitalWrite`. Functions marked `__attribute__((always_inline, hot))`.

**5. Interrupt Discipline**
`error()` calls `delay()` internally which requires interrupts enabled. Any code that disables interrupts with `cli()` must call `sei()` before invoking `error()`.

### Port Mappings (most error-prone area)

**18Pin - 411000 alternative pinout** (comment order ≠ actual order):
- A7→PB4, A8→PB2, A9→PB0 (not sequential)
- PORTD mask 0x18 preserves PD3/PD4 (DIP switch inputs)
- PORTB mask 0xEA preserves PB1(RAS), PB3(WE), PB5(LED)

**20Pin standard:**
- A0-A7 = PD0-PD7 (direct map, no LUT needed)
- A8=PB4, A9=PC4 (1Mx4 only)
- IO0-IO3 = PC0-PC3, CAS=PB0, RAS=PB1, OE=PB2, WE=PB3

### Test Sequence
For each chip type: presence detection → chip identification (address line A8/A9 capacity test) → address line verification → 6 pattern passes (0x00, 0xFF, 0xAA, 0x55, pseudo-random, pseudo-random inverted) → retention test. Patterns 0-3 use fast write-all/read-all; patterns 4-5 use per-row random data.

### OLED Display (optional)
Enabled via `#define OLED` in common.h. Uses U8g2 library with I2C bit-bang on PB5(SCL)/PB4(SDA). Requires `u8g2_i2c_speedfix.patch` for correct 400kHz timing.

### RAM_Definition struct fields
```c
struct RAM_Definition {
  const char* name;       // PROGMEM string
  uint8_t mSRetention;    // Refresh time in ms
  uint8_t delayRows;      // Rows per delay during retention test
  uint16_t rows;          // Row address count
  uint16_t columns;       // Column address count
  uint8_t flags;          // STATIC_COLUMN | NIBBLE_MODE | SMALL_TYPE
  uint8_t delays[6];      // Per-pattern retention delays (×20μs)
  uint8_t writeTime;      // Write cycle time (×20μs)
};
```
