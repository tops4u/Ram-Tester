Current Firmwares:
- 2.1.x   Legacy Versions prior to Displays
- 3.x.x   Current Head Versions - if in doubt use these
- 4.1.0   Current Firmware with fixed bugs. 3732-L implemented buy untested with physical RAM. 4532 implemented only tested with 4164 and simulated detection. 

The Assembly Test was used to check if the Soldering was ok, with the built in Selftest of 4.x this is more or less obsolete.

You can choose to directly use the HEX Version (i.e. for Programming with T48 over ICSP), or download the Arduino .INO File, Compile and Upload by any ICSP means Arduino IDE offers. You may of course change the Source code, but be aware that this might change Retention Timings due to compiler optimizations. 

To directly programm the HEX file use fuses:

- LowByte: 0xFF
- HighByte: 0xDF
- Extended: 0xFF
- LockBit: 0xFF
