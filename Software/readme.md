Current Firmwares:
- 2.1.x   Legacy Versions prior to Displays
- 2.4.x   Current Head Versions - if in doubt use these

You can choose to directly use the HEX Version (i.e. for Programming with T48 over ICSP), or download the Arduino .INO File, Compile and Upload by any ICSP means Arduino IDE offers. You may of course change the Source code, but be aware that this might change Retention Timings due to compiler optimizations. 

To directly programm the HEX file use fuses:

- LowByte: 0xFF
- HighByte: 0xDF
- Extended: 0xFF
- LockBit: 0xFF
