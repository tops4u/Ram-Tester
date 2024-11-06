v2.0pre (2024-11-06)
- Added Refresh Testing for 20 & 16 Pin Devices
- Added Row Buffer, Decoders and Line Checks for all Devices
- Added Row Crosstalk Check for 20 & 16 Pin Devices

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
