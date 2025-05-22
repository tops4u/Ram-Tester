v2.3.0a (2025-05-23)
- Major rework on Retention Testing. Introduced RAM Types for timing. Added OLED support.
- Speed optimization in the Code to keep longer test times of pseudo random data at bay.
- Minor Bugfix 16Bit had one col/row overrun - buggy but no negativ side effects
- OLED Tests and final implementation missing

v2.2.0 (2025-05-12)
Beta Version of 2.2
- Improves Testing by adding a RandomPattern Testing in the last Stage of Ram Tests. This increases the total Test-Time due to frequent changes of Data Patterns by about 20%. (currently for the 20 Pin RAM only, will be extended to all types)
- Support for Static Column Ram added (20Pin only). New Blink Pattern if SC 41258 SC Ram is checked ok (long green, yellow, red).

v2.1.2 (2025-05-21)
Bugfix Version of 2.1.1
- Error in Mapping of internal address bits to PORTD I/O Mapping leads to 2 internal Bits being mapped to the same physical line. This results in this address line not being checked on the 18 Pin Checks.
  
v2.1.1 (2024-12-23)
- Bugfix for wrong Testpatterns
- Minor Bugfix for IO Config during Tests for 18Pin RAM

v2.1 (2024-12-09)
- Added test mode, after PCB soldering. For Details check WIKI.

v2.0 (2024-11-12)
- Bugfixes for 4464
- Timing for refreshrates of 4464 adjusted
- General Code Streamlining by using Code Replacements for Shift and Logic Ops (#define)
  
v2.0pre (2024-11-06)
- Added Refresh Testing for 20 & 16 Pin Devices
- Added Row Buffer, Decoders and Line Checks for all Devices
- Added Row Crosstalk Check for 20 & 16 Pin Devices
- Re-Added GND Checks
- Streamlining of 20Pin Code
- Fixing Issues with 20Pin Refresh Checks
- Switch On Red LED before Testing - results in Yellow during Test. 

v1.4 (2024-11-02)
- Added Support for 4416 including ROW/COL Decoder check
- 4464 Code is untested, since RAM Testchip not yet available

v1.3 (2024-10-16)
- Implemented the same Row & Column pin, line, buffer and decoder tests as for the 41xx Series for the 20Pin RAM
- This is all functionality for the 16Pin and 20Pin RAM in Version 1 of the Software. 18Pin RAM Chips not yet implemented as I don't have any of those yet to test. 

v1.23 (2024-10-15)
- Added checks for ROW decoder and buffers (4164 / 41256)
  
v1.22 (2024-10-13)
- Added Support for 4164 (auto selection between 4164 and 41256)
- Added check for addressline (Pins, Buffers and Colmundecoders) - No Checks for Rowdecoders yet.
- Current Testperformance: 4164: 1.9sec / 41256: 7.6sec / 514256: 2.1sec / 441000: 8.5sec

v1.2 (2024-10-13)
- Added Support for 256x1 (41256) DRAM
- Testtime approx 8sec - longer due to more complex address Decoding
  
v1.1 (2024-10-10)
- Added Test for KM44C1000AZ-70 (Test Time < 10sec)
- Tested Automatic Size detection for 256kx4 vs 1Mx4 Chips.
- Improved RAS Only Refresh (ROR) Init Algorithm to be more compatible

v1.0 (2024-10-10)
- TC514256-80 working (Test Time around 2 sec)
- Only 256kx4 DRAM are currently working correctly (faster 1Mx4 i.e. 70ns Types fail)
- Empty RAM Slot fails with Error 2 Pattern 2. This could be nicer
