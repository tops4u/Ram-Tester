Current Firmwares:
- 2.1.x   Legacy Versions prior to Displays
- 3.x.x   Current Head Versions - if in doubt use these
- 4.0.0   Preliminary v4.0 Release. Should work, except new unverified 4532/3732 RAM implementation.- AssemblyTest is not longer needed after 4.x Firmware as this feature is built in
- 4.0.1   Minor Fix to allow In-Circuit Testing of 4164/41256 when Din is hardwired to Dout (i.e. Amiga 501 Trapdoor Memory Expansion)
- 4.0.5	  Major Speed improvement for most RAM Types. Fixing illegal access mode for 16 Pin RAMs. Adding 41257 detection and Test (no retention Testing yet). Fixing wrong 3732 Detection - but Test is still buggy. ** WARNING ** This Firmware needs a changed installation / upgrade procedure!

The Assembly Test was used to check if the Soldering was ok, with the built in Selftest of 4.x this is more or less obsolete.

You can choose to directly use the HEX Version (i.e. for Programming with T48 over ICSP), or download the Arduino .INO File, Compile and Upload by any ICSP means Arduino IDE offers. You may of course change the Source code, but be aware that this might change Retention Timings due to compiler optimizations. 

To directly programm the HEX file use fuses:

- LowByte: 0xFF
- HighByte: 0xDF
- Extended: 0xFF
- LockBit: 0xFF
