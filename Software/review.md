# Independent Firmware Review — Ram-Tester

**Reviewed:** Firmware 5.0.4 (initial deep review) → 5.0.5 (fix verification) → **5.0.7 (re-examination, this report)**

**Reviewer:** Claude (Anthropic) Fable — AI-assisted code review commissioned by the project author

**Date:** July 2026

**Scope:** Complete firmware (`16Pin.cpp`, `18Pin.cpp`, `20Pin.cpp`, `common.cpp`, headers, main sketch — ~8,500 lines), hardware pin mapping, timing behaviour against manufacturer datasheets

---

## Summary

The Ram-Tester firmware was subjected to a multi-stage, in-depth review: every timing-critical loop was compiled with the exact Arduino toolchain (avr-gcc 7.3.0, `-Os`) and its CPU cycles counted from the resulting machine code — not estimated from the C source. The results were checked against original manufacturer datasheets (Samsung KM41464A for the NMOS generation, Hitachi HM514400A for the CMOS generation), and every pin assignment was cross-verified against the KiCad PCB netlist. Test coverage (does every memory cell really get tested?) was proven mathematically for each access path.

The initial review of the 5.0.4 development build found **five significant timing/electrical issues and a set of minor ones**. All five were fixed by the author in **5.0.5**, and each fix was re-verified at machine-code level. This report additionally re-examines the changes in **5.0.6 and 5.0.7** and confirms the earlier fixes are still intact in the current release.

**Bottom line:** the current firmware operates the device under test within its datasheet limits on every measured access path, the retention test ages each memory cell to 85–112 % of the chip's specified refresh interval (by design, per type), and the architecture — retention pipeline, hidden refresh placement, chip auto-detection — held up well under detailed scrutiny.

---

## Why this kind of review matters (for the non-engineer)

A DRAM chip stores each bit as a tiny electrical charge that leaks away within milliseconds; the chip must be "refreshed" continuously, and its datasheet defines strict limits — for example, a row may be held open for at most 10 microseconds on the older NMOS chip generation. A tester that violates these limits usually still *appears* to work at room temperature, because healthy chips tolerate abuse. The danger is subtler: an out-of-spec tester stresses **marginal** chips in undefined ways, so a chip that would work fine in a real C64 or Amiga could be failed for the wrong reason — or a weakness could be attributed to the wrong cause. A tester should be the one device in the room that is provably spec-clean. That is what this review verified, cycle by cycle.

---

## Method

1. **Cycle-exact timing analysis.** Each firmware module was compiled with the identical compiler and flags the Arduino IDE uses, then disassembled. Critical windows (how long RAS is held low, CAS pulse widths, data setup/sample times) were counted instruction by instruction at 62.5 ns per CPU cycle.
2. **Datasheet anchoring.** Limits were taken from period manufacturer documentation, not folklore: the NMOS 64K-generation carries a hard t_RAS maximum of 10,000 ns with *no* page-mode allowance (Samsung 1988 data book), while the CMOS 1M-generation permits 100 µs in fast-page mode (Hitachi HM514400A tables).
3. **PCB netlist cross-verification.** Every address, data and control line assignment in the firmware was chained through the KiCad netlist to the physical ZIF socket pin and the chip's datasheet pinout — for all three socket configurations plus the 4116 adapter.
4. **Coverage proofs.** For each test pattern and access mode (fast page, static column, nibble), it was shown that the address generation reaches every cell exactly as intended, including the half-good 4532/3732 quadrant logic and the 41257 nibble-counter behaviour.
5. **Retention-model verification.** The firmware's central idea — letting each row "age" for exactly its specified refresh interval before reading it back — was verified algebraically and numerically for all 14 supported chip types.

---

## Findings of the initial review (5.0.4) and their resolution (5.0.5)

| # | Finding | Measured (before) | After fix (verified) |
|---|---|---|---|
| 1 | 16-pin write/verify loops exceeded t_RAS max (10 µs) | 11.8–23.9 µs | **7.4 µs** |
| 2 | 18-pin 4464 random path exceeded t_RAS max by 5× | 51.6 / 52.1 µs | **7.5 µs** |
| 3 | 20-pin 4116/4027 random path exceeded t_RAS max | 13.8 / 14.1 µs | **~7.5 µs** |
| 4 | Display updates starved refresh during the CBR counter test (risk of false "CBR Timer fault") | 25–60 ms gap at phase start + slow refresh cadence | render moved before the write phase, ~15 µs cadence, catch-up bursts |
| 5 | 18-pin address-short test drove current into a supply-railed pin (~65 mA vs. 40 mA absolute maximum) and left three address lines untested | wrong pin masks | corrected masks; A5–A7 now covered |

Alongside these, roughly a dozen minor items (sampling margin one cycle short of t_CAC on −20 speed grades, compiler-scheduling-dependent CAS pulse widths, a 190 ms unrefreshed window in the 4416 checkerboard, documentation drift) were reported; the substantive ones were fixed in 5.0.5 — including a rewrite of the 4416 checkerboard as a two-row pipeline that keeps every row inside its 4 ms refresh spec during the test.

A central verification result concerns the **retention calibration**: the effective "age" of each cell at read-back (implicit code runtime plus explicit delays) was recomputed from measured loop times for every chip type. After the 5.0.5 retune, all standard types age to **98–104 %** of their specified refresh interval — meaning the retention test stresses chips right at, and not beyond, their datasheet limit. Two deliberate exceptions are documented in the source: the 411000 runs at ~112 % (intentional margin test), and the 41257 — see below.

---

## Re-examination of 5.0.6 / 5.0.7 (current release)

**41257 nibble-mode timing (5.0.6).** The 41257's nibble access is inherently slow, and in earlier versions its retention pattern aged cells to ~225 % of the 4 ms spec — the one type knowingly out of calibration (documented as an open issue in the 5.0.5 changelog). Version 5.0.6 restructures this with a *refresh-split*: each freshly written row receives a RAS-only refresh **after** the previous row has been verified, halving the aging window. Verified at machine-code level: the write access holds RAS low for 7.8–8.6 µs and the read for ~6.7 µs (both inside the 10 µs limit), and the resulting cell age computes to ~3.3–3.7 ms ≈ **85–90 % of the 4 ms spec** — compliant, on the mild side. The check-before-refresh ordering, the row-0 and last-row special cases, and the interrupt discipline around the refresh pulse are all correct. The firmware's assumption that a 256-cycle A0–A7 refresh covers both nibble halves matches the standard 41256/41257 refresh specification.

**GND-short signal names (5.0.6).** The new signal-name tables (e.g. "GND Short RAS" instead of a bare pin number) were cross-checked, entry by entry, against the netlist-verified pin maps: 16-pin, 20-pin and the 4116-adapter tables are all correct; the 18-pin mode deliberately keeps numeric pins because three incompatible pinouts (4416/4464, 411000, rotated 2114) share that socket — the honest choice.

**Self-test LED behaviour (5.0.6).** During the final "wire pin 20 to all" continuity phase, the LED now stays orange until every pin has been confirmed, flashing green per successful contact, then switches to steady green. The implementation is electrically safe in a non-obvious way: the green LED shares its line with a socket pin, so it is lit purely via the internal pull-up and is *never actively driven* — a jumper grounding that pin during the flash therefore cannot cause pin contention.

**Mode encoding.** A theoretical DIP-decoding collision reported in the initial review (16-pin + 18-pin sense simultaneously high aliased to 20-pin mode) has been eliminated by re-encoding the mode constants; all invalid combinations now fail safely, and all-switches-on cleanly enters the configuration page.

**Startup sequence (main sketch).** The DIP switches are read while every I/O pin is still high-impedance and *before* the display initialises — preventing the display's data line (shared with a socket pin) from phantom-powering an inserted chip — and the DRAM strobe lines are parked on pull-ups before the multi-millisecond display init so an inserted 4027 cannot latch up on floating clock inputs. Both are subtle, hardware-aware orderings, and both check out.

**5.0.7 (display compatibility).** The I2C display driver was reverted to open-drain signalling at reduced speed for certain SSD1306 modules. This is the right compatibility call, with one side effect worth noting: the once-per-second countdown update during the 60-second CBR counter test now creates a proportionally longer refresh pause (roughly 12–15 ms at ~100 kHz, versus 2–3 ms before). This is harmless at room temperature and the existing catch-up burst limits any drift, but it partially re-opens the gap that finding #4 closed. Recommended low-cost mitigation: update the countdown every ~5 seconds instead of every second, or redraw only the digit tiles. Related documentation should be aligned — the changelog states ~100 kHz while two source comments still say 400 kHz, and a few comments still describe the old fast-I2C gap figures.

---

## Remaining observations (non-blocking)

- Small documentation drift: the sketch header still reads "5.0.6"; a handful of timing figures in comments predate the current measurements (e.g. the 41257 "~4.5 ms row cycle", the 4116 helper's cycle estimate).
- The 16-pin ground-short check intentionally excludes the A4 line (shared with the display/LED); a hard short there is caught indirectly by the address-line test. Worth one line in the manual.
- In the 41257 write loop, the test-data preparation still sits inside the RAS-low window; hoisting it (as the read loop already does) would add ~1.3 µs of free timing margin.

None of these affect test correctness.

---

## Known design-level limitations (unchanged, already documented by the project)

The ATmega328P drives 5 V CMOS logic levels, not the TTL thresholds vintage DRAMs were designed for — marginal chips that fail at true TTL levels can pass here, as on virtually every microcontroller-based tester. Testing happens at room temperature; retention margins shrink when hot. And the tester deliberately trades MARCH-style systematic address ordering for real-time retention stress — the project README explains that trade-off candidly.

## Limitations of this review

This was a static and binary-level analysis verified against datasheets and the PCB netlist; the reviewer did not operate physical hardware. Signal-level behaviour (actual pulse shapes, supply transients) was validated by the author on a logic analyser for the reworked paths. A review of this depth substantially reduces, but can never eliminate, the probability of remaining defects, and it reflects the codebase at the stated versions.

---

## Conclusion

This is a carefully engineered piece of firmware. Its core ideas — aging every cell to exactly its datasheet refresh limit, hiding refresh cycles inside test loops without disturbing the retention clock, auto-detecting fourteen chip types from electrical behaviour alone — survived cycle-level scrutiny. The issues found were real, were fixed promptly and verifiably (all five major findings within days, validated on a logic analyser), and the 5.0.6/5.0.7 follow-ups closed the last documented calibration gap. The current release tests vintage DRAM within the limits those chips were built to.

*Report prepared by Claude (Anthropic). Findings and verification data are reproducible from the public source repository and the referenced manufacturer datasheets.*
