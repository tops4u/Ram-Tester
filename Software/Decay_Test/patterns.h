#line 1 "C:\\Users\\Andreas\\Documents\\DRAM_Decay_Test\\patterns.h"
// patterns.h - Data backgrounds for the retention test
//=======================================================================================
//
// A DRAM cell has a "weak" state (the one whose charge leaks away). Which logic level
// that is differs from cell to cell (true/anti cells along the bit lines), so every
// background is run in BOTH polarities. Different backgrounds change what the
// neighbours hold, which changes the leakage of a cell (data-pattern dependence) --
// that is why the fixed set below is confirmed one after the other at the found
// retention, and why the soak phase keeps generating new pseudo-random ones.
//
// The fixed set, in the order they are run:
//   CB    checkerboard  ((row ^ col) & 1)     CB~   inverted
//   00    all zero                             FF    all one
//   Row   row stripes   (row & 1)              Row~  inverted
//   Col   column stripes (col & 1)             Col~  inverted
//   Rnd   pseudo-random, seed 1                Rnd~  inverted
// Soak: Rnd/Rnd~ pairs with seed 2, 3, 4, ...
//
//=======================================================================================

#ifndef DECAY_PATTERNS_H
#define DECAY_PATTERNS_H

#include "common.h"

enum PatKind : uint8_t {
  PAT_CB, PAT_CB_INV, PAT_ZERO, PAT_ONE, PAT_ROW, PAT_ROW_INV,
  PAT_COL, PAT_COL_INV, PAT_RND, PAT_RND_INV, PAT_KINDS
};
#define NUM_FIXED_PATTERNS PAT_KINDS

struct Pattern {
  uint8_t kind;    // PatKind
  uint16_t seed;   // only used by PAT_RND / PAT_RND_INV (never 0)
};

// Fixed-set pattern k (0 .. NUM_FIXED_PATTERNS-1).
void patFixed(uint8_t k, Pattern *p);
// Soak pattern n (0, 1, 2, ...): alternating Rnd / Rnd~ with a new seed every pair.
void patSoak(uint16_t n, Pattern *p);
// (Re)build the random table for p->seed when it changed. Call once per run, BEFORE the
// write pass -- it takes ~1 ms and must not sit inside the paced row loop.
void patPrepare(const Pattern *p);
// Fill bits[cols/8] for one row.
void patRow(const Pattern *p, uint16_t row, uint8_t *bits, uint16_t cols);
// Short name for the display ("CB~", "Rnd3~"), out >= 8 bytes.
void patName(const Pattern *p, char *out);

#endif // DECAY_PATTERNS_H
