# Changelog

### v4.2.3 (2026-03-20)
- Fixed the logic for MSM3732 and TMS4532, which is now also verified — please check the Docs on this!
- Added address-line short-circuit test before actually testing RAM. This checks address lines against each other
- Added fallback retention testing for 4164 RAM. If the RAM fails with 4 ms tests, the test is repeated with a 2 ms test. The result will show which retention time is met by writing "4164 64K x 1 4ms" or suffix "2ms"

### v4.1.0 (2026-02-24)
- Fixed a bug in the handling of delays for last rows during retention testing
- Adapted 514256/-258 retention times 4 → 8 ms
- Implemented all logic and retention testing for 3732 and 4532 RAM. Yet 3732-L and 4532 are not yet tested with physical RAMs
- Some minor speed improvements
- Minor adjustments to retention test delays
- Patch for u8g2 lib for using Soft I2C — if applied you may uncomment `#define OLED_2PAGE` in `common.h`

### v4.0.6 (2026-02-22)
- Fixed and verified implementation for MSM3732-H (needs checking with real HW for MSM3732-L and TMS4532)

### v4.0.5 (2026-02-16)
- **BREAKING**: Firmware now uses the previously unused bootloader space — this changes the upgrade/install procedure! Check the Docs!
- Vast speed improvements for most RAM types, less than 45% of the previous test time!
- 41257 nibble mode RAM detection and test patterns 1–6 implemented, retention tests to be done
- Fixed false detection of 3732 RAM sub-type, yet tests 1–4 are ok, 5–6 are still buggy and no reliable retention testing

### v4.0.1 (2026-01-26)
- Re-enabled in-circuit testing of 4164/41256 when Din is hardwired to Dout. Din will enter high impedance when Dout is read back

### v4.0 (2026-01-11)
- **BREAKING**: 4164 RAMs are now tested with 4 ms retention time. There are few 2 ms 4164 around and most computers will need the 4 ms part
- Added QR code to help you find the GitHub Docs page in case DIP switches are invalid
- Added self-test/health-check mode. When all DIP switches are *OFF* it starts automatically. Check documentation on how to use
- Added CBR (CAS-before-RAS) timer test. At the end, for RAMs that have a refresh timer, it is now tested
- *Preliminary support* for 4532/3732 RAMs — **UNTESTED** and most probably still buggy. All other RAM checks should be fine

### v3.1.1 (2025-12-12)
- Added RAM presence check for 4116 adapter
- Added 4027 RAM. Needs 4116 adapter to run (requires −5 V / +12 V)
- Fixed minor typo in the display texts

### v3.0.1 (2025-09-08)
- Fixes a bug in the 20-pin code that caused pattern 0 & 1 to always result in an OK RAM due to missing data port direction change

### v3.0.0 (2025-09-03)
- Large changes in address checks
- Random data & retention tests are run twice to have all bits checked in set and reset state
- New LED status codes. The initial codes were planned when there were only a few RAMs to test
- Added tests to try and figure out if actually a RAM is inserted — if a RAM is really dead it is still not detected
- Streamlined error texts
- Added support for 4816 (16K × 1) RAM
- Added support for 4116 RAM via DC-DC board
- Code refactored in separate files to simplify maintenance
- Fixed a few bugs:
  - 41256 / 514400 patterns 0 and 1 only checked the first 256 cols due to an error. As those are checked with the following patterns this is not a major bug
  - Fixed a bug in retention testing that might lead to chips being detected as working while in fact they are broken

### v2.4.2 (2025-07-28)
- Bugfix for EEPROM wear leveling algorithm. It stopped working after the first EEPROM cell was full

### v2.4.1 (2025-06-27)
- EEPROM controlled pseudo random data flip added to have 100% cell coverage in random and retention tests if user wants it
- Added DIAG macro to enable / disable diag mode after firmware upload

### v2.4.0 (2025-06-26)
- Support for 411000 added. Test takes 28 secs
- General speed improvements
- **Important Notice:** Modified the load on the output pins of the 18-pin chips by enabling the pull-ups, so that the RAM must actively pull the outputs LOW when required. This change was necessary because parasitic capacitances were holding the input levels even after the RAM pin switched to output mode — even when the RAM was unplugged between tests. With pull-ups enabled, all I/Os are now held at 5 V unless the RAM actively drives them low.
However, older RAM modules or those produced with earlier processes often have weaker output drivers. While they may work fine in environments with minimal loading, their driver strength can be insufficient for this test setup — potentially leading to false-negative results.

### v2.3.1 (2025-06-06)
- Added some more in-depth address decoder checks
- Added a "RAM Inserted?" screen when autodetect was not able to determine the RAM

### v2.3.0 (2025-05-26)
- Added simple possibility to exclude display code if you don't use a display. Speeds up tests by 2.5 sec
- Print firmware version on DIP switch error page

### v2.2.0 (2025-05-12)
- Improves testing by adding random pattern testing in the last stage of RAM tests. This increases the total test time due to frequent changes of data patterns by about 20% (currently for the 20-pin RAM only, will be extended to all types)
- Support for static column RAM added (20-pin only). New blink pattern if SC 41258 SC RAM is checked ok (long green, yellow, red)

### v2.1.2 (2025-05-21)
- Error in mapping of internal address bits to PORTD I/O mapping leads to 2 internal bits being mapped to the same physical line. This results in this address line not being checked on the 18-pin checks

### v2.1.1 (2024-12-23)
- Bugfix for wrong test patterns
- Minor bugfix for IO config during tests for 18-pin RAM

### v2.1 (2024-12-09)
- Added test mode after PCB soldering. For details check Wiki

### v2.0 (2024-11-12)
- Bugfixes for 4464
- Timing for refresh rates of 4464 adjusted
- General code streamlining by using code replacements for shift and logic ops (`#define`)

### v1.4 (2024-11-02)
- Added support for 4416 including ROW/COL decoder check
- 4464 code is untested, since RAM test chip not yet available

### v1.3 (2024-10-16)
- Implemented the same row & column pin, line, buffer and decoder tests as for the 41xx series for the 20-pin RAM
- This is all functionality for the 16-pin and 20-pin RAM in version 1 of the software. 18-pin RAM chips not yet implemented as I don't have any of those yet to test

### v1.23 (2024-10-15)
- Added checks for ROW decoder and buffers (4164 / 41256)

### v1.22 (2024-10-13)
- Added support for 4164 (auto selection between 4164 and 41256)
- Added check for address line (pins, buffers and column decoders) — no checks for row decoders yet
- Current test performance: 4164: 1.9 s / 41256: 7.6 s / 514256: 2.1 s / 441000: 8.5 s

### v1.2 (2024-10-13)
- Added support for 256K × 1 (41256) DRAM
- Test time approx 8 s — longer due to more complex address decoding

### v1.1 (2024-10-10)
- Added test for KM44C1000AZ-70 (test time < 10 s)
- Tested automatic size detection for 256K × 4 vs 1M × 4 chips
- Improved RAS Only Refresh (ROR) init algorithm to be more compatible

### v1.0 (2024-10-10)
- TC514256-80 working (test time around 2 s)
- Only 256K × 4 DRAM are currently working correctly (faster 1M × 4 i.e. 70 ns types fail)
- Empty RAM slot fails with Error 2 Pattern 2

<details>
<summary>Pre-release versions (development snapshots)</summary>

### v2.3.0Pre2 (2025-05-24)
- Added support for 514402 static column 1M × 4 RAM
- Fine-tuning of retention test timings
- Fixed minor typos

### v2.3.0Pre (2025-05-23)
- Implemented all targeted functional features. Pending re-verification of all retention times

### v2.3.0a (2025-05-23)
- Major rework on retention testing. Introduced RAM types for timing. Added OLED support
- Speed optimization in the code to keep longer test times of pseudo random data at bay
- Minor bugfix: 16-bit had one col/row overrun — buggy but no negative side effects
- OLED tests and final implementation missing

### v2.0pre (2024-11-06)
- Added refresh testing for 20 & 16-pin devices
- Added row buffer, decoders and line checks for all devices
- Added row crosstalk check for 20 & 16-pin devices
- Re-added GND checks
- Streamlining of 20-pin code
- Fixing issues with 20-pin refresh checks
- Switch on red LED before testing — results in yellow during test

</details>
