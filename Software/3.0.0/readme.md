This is Version 3.0.0

It changes quiet a few things!

- Large changes in address checks
- Random Data & Retention Tests are run twice to have all Bits checked in set and reset state
- New LED Status Codes. The initial codes were planed when there were only a few RAMs to test
- Added Tests to try and figure out if actually a RAM is inserted - if a RAM is really dead it is still not detected. 
- Streamlined Error Texts
- Added Support for 4816 (16k x 1) RAM
- Added Support for 4116 RAM via DCDC Board
- Code refactored in separate files to simplify maintenance
- Fixed a few bugs:
  - 41256 / 514400 Patterns 0 and 1 only checked the first 256 Cols due to an error. As those are checked with the following Patterns so this is not a major bug
  - Fixed a Bug in Retention Testing that might lead to chips being detected as working while in fact they are broken

All this leads to better testing for the price of up to a few seconds per CHIP. 

**Current Test Times (Hand stopped):**
| RAM | Time |
|-----|------|
| 4816 | 2.9 secs |
| 4116 | 2.9 secs |
| 4164 | 4.6 secs |
| 41256 | 11.8 secs |
| 4416 | 3.9 secs |
| 4464 | 6.6 secs |
| 411000 | 37 secs |
| 41256 | 5.9 secs |
| 41258 | 6.2 secs |
| 54400 | 15.8 secs |
| 54402 | 15.9 secs |
