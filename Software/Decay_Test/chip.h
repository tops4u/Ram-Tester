#line 1 "C:\\Users\\Andreas\\Documents\\DRAM_Decay_Test\\chip.h"
// chip.h - Chip-driver interface for the DRAM Decay Test
//=======================================================================================
//
// The core (DRAM_Decay_Test.ino) never touches a port itself. It talks to ONE active
// driver through g_dram: geometry + four functions + two marker masks. A driver is a
// socket/family module that knows the pinout and the access cycle; it detects the exact
// part (rows x cols) in detect() and fills in name and geometry.
//
//   chip_4164.cpp    16-pin x1  : 4164, 41256/41257, 4816            (DIP "16-pin")
//   chip_4464.cpp    18-pin x4  : 4464, 4416                          (DIP "18-pin")
//   chip_514256.cpp  20-pin x4  : 514256/514258, 514400/514402        (DIP "20-pin")
//
// Row-based API. The core hands the driver a row number and a bitmap with ONE BIT PER
// COLUMN (LSB first: bit for column c is (bits[c >> 3] >> (c & 7)) & 1). A x1 driver
// writes that bit; a xN driver replicates it on all N data lines (0x0 / 0xF). That is
// the physically right thing for retention: the N planes of a xN part are separate
// arrays, each of them then sees the same 2-D background, and every background runs
// in both polarities anyway. It also keeps the row buffer at cols/8 bytes.
//
// The driver writes or verifies the row with SINGLE-CYCLE accesses (one RAS per cell)
// -- no page/nibble/static-column mode, so it works on every generation and never runs
// into tRAS-max. The core wraps every call in cli/sei; a driver whose row takes longer
// than ~1 ms lets interrupts through for one instruction between chunks (the reference
// drivers do so every 32 cells) so the millis() timer never loses a tick.
//
// Timing contract (what the core relies on):
//   - writeRow/readRow take constant time for a given geometry (the core measures the
//     slowest row once and plans every run on it, see planPeriod() in the .ino);
//   - writeRow and readRow advance through the cells at the SAME per-cell rate (the
//     age of a cell is read time minus write time, and both passes run the cells in
//     the same order); the reference drivers do this with cycle-counted inline asm;
//   - nothing in between two calls refreshes any row (RAS stays HIGH);
//   - setupPorts() is idempotent and restores the socket state after a display update.
//
// Markers (logic analyser, docs/LA_Testkonzept.md): markRow / markBusy are PORTD bit
// masks the core toggles at every row event / holds HIGH during a run; 0 = the socket
// has no free pin for it. The per-cell error pulse lives inside the driver's own read
// loop (16-pin only, PD5).
//
// Adding a chip family:
//   1. new chipXXX.cpp with setupPorts/detect/writeRow/readRow + a bind function;
//   2. add the bind function to the socket table in DRAM_Decay_Test.ino (bindDriver);
//   3. keep the cell loops of write and read cycle-matched (avr-objdump -d, then the
//      LA check), and mind the row buffer: cols <= 8 * sizeof(g_bits).
//
//=======================================================================================

#ifndef DECAY_CHIP_H
#define DECAY_CHIP_H

#include "common.h"

#define MAX_COLS 1024     // row buffer size in bits (g_bits)

struct DramDriver {
  const char *name;        // PROGMEM "part geometry", e.g. "4464 64Kx4"; set by detect()
  uint16_t rows;           // set by detect()
  uint16_t cols;           // set by detect()  (multiple of 32, <= MAX_COLS)
  uint16_t specUs;         // datasheet refresh interval of the detected part (display only)
  uint8_t markRow;         // PORTD mask toggled at every row event (0 = none)
  uint8_t markBusy;        // PORTD mask held HIGH during a run (0 = none)
  void (*setupPorts)(void);                         // DDR/PORT for this socket; idempotent
  bool (*detect)(void);                             // presence + geometry; false = no RAM
  void (*writeRow)(uint16_t row, const uint8_t *bits);
  // Returns the number of mismatching cells in the row (saturated); *firstBadCol = first.
  uint16_t (*readRow)(uint16_t row, const uint8_t *bits, uint16_t *firstBadCol);
};

extern DramDriver g_dram;

// Socket binders: fill g_dram (name/geometry are refined by detect()).
void bind_16pin_x1(void);   // chip_4164.cpp
void bind_18pin_x4(void);   // chip_4464.cpp
void bind_20pin_x4(void);   // chip_514256.cpp

#endif // DECAY_CHIP_H
