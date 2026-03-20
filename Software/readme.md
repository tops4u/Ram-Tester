Current Firmwares:
- 2.1.x   Legacy Versions prior to Displays
- 4.2.x   Current Firmware. Includes verified implementation of 3732/4532 Logic. Please read below! 

## New with 4.2.x

### Short Story
- Using <code>Ram_Tester_4.2.3_32k.hex</code> or unmodified source code, the tester will be able to Test *MSM3732* and *TMS4532* at the cost that partially broken *4164* will be reported as one of those 2 types if it covers their functionality and not as bad *4164* - as long it is not completely broken. 
- Using <code>Ram_Tester_4.2.3.hex</code> or by commenting <code>#define ENABLE_32K</code> in file <code>common.h</code> the tester will not be able to test *MSM3732* or *TMS4532* they will be reported as bad *4164*. Also any defect on a *4164* will be reported as broken 4164 RAM. 

### Long Story
You may have seen that currently there are two HEX Files for the 4.2.x Release. This is due to the fact that the implementation of the MSM3732 and TMS4532 have some consequences. Those are basically 4164 RAM with some defects (ranging from simple 1bit errors to complete rows or columns that don't work). For an automatic detection of the RAM it needs to be tested. As said before it is basically a partially broken 4164 RAM, detecting a sane 4164 is no longer possible this way. So similarly broken 4164 RAM would exhibit the same pattern and are thus also identified as one of those RAM types. If you think that you will never test any of those RAMs you may use the Version without 32K in the Name, which has this logic deactivated. Defective 4164 will then be shown as faulty as any 3732 or 4532 would. 

In order to rise the users awareness that if a 4164 is inserted and then identified as 3732 and/or 4532 the Text of those RAM Types is inverted on the Display. To Check if your Firmware has 32K Support, you can check the Version of the Firmware. Firmware without 32K is writen white text on black background like <code>Ver.:4.3.2</code> while Firmware that has 32K Support enabled will be written black text on white background and has the suffix 32 like <code>Ver.:4.3.2 32</code>. 

**If you compile the Firmware yourself by uploading it via the Arduino IDE, you may decide if you want to disable the 32K Logic, by commenting out the macro <code>#define ENABLE_32K</code> in the file common.h in the first few lines. Alternatively you may choos the HEX File without 32k in the name**

The Assembly Test was used to check if the Soldering was ok, with the built in Selftest of 4.x this is more or less obsolete.

You can choose to directly use the HEX Version (i.e. for Programming with T48 over ICSP), or download the Arduino .INO File, Compile and Upload by any ICSP means Arduino IDE offers. You may of course change the Source code, but be aware that this might change Retention Timings due to compiler optimizations. 

To directly programm the HEX file use fuses:

- LowByte: 0xFF
- HighByte: 0xDF
- Extended: 0xFF
- LockBit: 0xFF
