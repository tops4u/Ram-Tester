# Ram-Tester
Ram Tester for vintage CBM Computer RAM Chips - **BETA** do not build/fork yet.

This is a hobbyist project, there is no warranty of any sort and usage is on your own risk. By using any information on this site you agree to this. Please read the license.

I decided to build a tester myself with the aim of being able to test some of the common DRAM chips of the CBM computers 1980-1990.

## Introduction
This project was started because I had bought a Commodore A2630 card with 2MB Ram at a flea market and wanted to upgrade it with 2MB Ram. On the internet I found either very expensive offers or cheap ones from Chinese dealers. I tried my luck, but of course the card refused to work with the additional RAM. So I wanted a tester for the required RAM. On the one hand, there were very simple projects, which probably didn't test very well, or semi-professional testers with >1000U$. Meanwhile I was able to sort the defect RAM out by have a bootable Configuration and using Amiga Test Kit (ATK v1.22) to figure out which BIT are faulty and pinpoint possible candidates by checking the schematic. 

This Project was/is inspired by:
- Project DRAM (https://github.com/ProjectDRAM/514256B)
- DRAMTESTER (https://github.com/zeus074/dramtester) 
- DRAMARDUINO (https://forum.defence-force.org/viewtopic.php?t=1699)

-> No Code or Schematic was taken from any of those Projects.

So why yet another Project? 
1. Have a cheap and simple solution. No fancy LCD that does not give you real added value. If a chip is faulty you probably don't care at which address.
2. Have a fast solution. Some other RAM Testers take more than 1 minute to just test a 64kb (8kB) Chip like the 4164. This project takes less than 3 secs for a 256x4 (128kB) Chip to test.
3. Easy Setup with few components - ZIF Sockets if you need it often, otherwise probably normal DIP Sockets will be ok.
4. Small Footprint PCB to save cost.
5. Have a Tester that also supports larger RAM used in Amiga for example. 

The project should also be able to be built by inexperienced people, which is why I decided on a solution with ATMEGA 328 processors - known from the Arduino UNO. But just having another Shield for Arduino seemed unpractical since you have some limitations by the Arduino itself.

The processor can still be programmed and taken from an [Arduino UNO](https://store.arduino.cc/products/arduino-uno-rev3), programmed with a programmer or programmed via the existing ICSP (e.g. with an AVRISP MKII). People who want to remove the processor for programming can swap it between an Arduino UNO and this Board. 

Operation is child's play. Insert the RAM, switch on the power and observe the LED. if it flashes green at the end, everything is ok, if it flashes red, something is broken. 

This Tester should work with the following DRAM (as soon as the software is able to do so): **4164** (64k x 1), **4416** (16k x 4), **4464** (64k x 4), **41256/57** (256k x 1), **514256** (256k x 4) and **514400** (1M x 4). The prerequisite is that GND is on the last pin and VCC on the diagonally opposite pin as well as the IC size of 16, 18 or 20 pins. Check the changelog File for current supported DRAMs.

*->As such, this Project will **not** be able to support the following RAM Types **2144** (Vcc on Pin 18), **6116** (24 Pin IC), **4116** (needs negative and 12V Voltage).*

In order to keep build cost low, I just designed one PCB. If you want to use the ZIP Adapter you need to cut the PCB along the thick white line. Build cost with PCB actually less than 10U$ if you have the Material ready. If you need to order everything it is more likely to be 20-30U$.

<img src="https://raw.githubusercontent.com/tops4u/Ram-Tester/refs/heads/main/Schematic/IMG_2988.jpeg" width="400px" align="center"/>
Pre v1.0 PCB build

## Operation
On Power on or after pressing the Reset Button it first checks if the DIP Switches are in a valid position. If this is not the case it will signal it with continuous red flashing.
Of a valid Config is found, the DRAM Chip is properly initialized, then Testing starts. First the address Lines are checked (Pin connectivity, Buffer function, Address-Decoders). For Configs that allow differents sizes of DRAM, the Tester will try to figure out the current Size. Then it starts Testing by using multiple patterns. 
1. Pass = All Bit set to '0'.
2. Pass = All Bit set to '1'.
3. Pass = Bits set to '01'...
4. Pass = Bits set to '10'...

**LED Function:**
- Continuous Red - Off - Red Flashing: Config Error with the DIP Switches or internal Logic Error
- Green to Yellow Fading : Testing ongoing
- 1x Red followed by n Green Flashes : Addressline Error. n-Green indicates the failing Addressline
- 2x Red followed by n Green Flashes : Error during Bit Testing. n-Green indicates the failing Pattern. (Pattern 1 - Stuck Bit in On, Pattern 2 - Stuck Bit in Off, 3 or 4 there is crosstalk between columns).
- Long Green - short black : Test successful. The Tester assumed this to be the larger Type to Test (1Mx4, 256x1 or 64x4)
- Long Green - short red  : Test successful. The Tester assumed this to be the smaller Type to Test (256x4, 64x1 or 16x4) **BUT**: If the Chip is not this type, this could mean a faulty Adressline (like A9 for 1Mx4 therefore it will be mistakenly  accounted as 256x4). 

**NOTE:** The following will not be tested:
1. Speed requirements. The tests are carried out in a way that slow and fast RAM should pass. It will not test if 70ns RAM really works flawless at 70ns.
2. The Chessboard Patterns are actually only tested Row-Wise and not interleaved between multiple Rows.
3. Pin connectivity, buffers and decoders are only simply checked if they work - no checks for internal errors or crosstalk.
4. Of course there is no 100% Warranty that the Tester will not render false positive or false negative test Results. 

## Build
**BOM**
- Y1 1x 16 MHz Quartz [AliExpress](https://aliexpress.com/item/1005006119798769.html)
- C1/C2/C6/C7 4x SMD 0805 100nF Capacitor 
- C2/C3 2x 10-20pf Disc Capacitor [AliExpress](https://aliexpress.com/item/1005003167676803.html)
- R3, R4, R5 3x SMD 0603 1M Resistor
- R2 1x SMD 0603 1k Resistor
- R1 1x SMD 0603 100R Resistor
- R6 1x SMD 0603 150R Resistor
- R7, R8 2x SMD 0603 47R Resistor
- RNx 4x SMD 0603 47R 4xArray
- Q1, Q2, SMD IRL 6401 SOJ P-FET
- U1 DIP Socket 28 Pin - Narrow
- U2 1x ZIF Socket / DIP Socket 20 Pin [AliExpress](https://aliexpress.com/item/1005007205054381.html)
- D1 1x BiColor LED 5mm (Red/Green) Common Center Cathode [AliExpress](https://aliexpress.com/item/1005006014283662.html)
- SW1 1x 3 way Dip-Switch [AliExpress](https://aliexpress.com/item/4001205849246.html)
- SW2 Pushbutton [AliExpress](https://aliexpress.com/item/4000555847543.html)
- J1 MicroUSB (or any power Connector having 2.54mm Spacing [AliExpress](https://aliexpress.com/item/1005001515820458.html)
  
If you want to use ICSP:
- ICSP 1x PinHeader 2x3 [AliExpress](https://aliexpress.com/item/4000303366348.html)
  
If you want to use the ZIP Adapter
- U3 2x 10Pin Pin Header Female [AliExpress](https://aliexpress.com/item/32717301965.html)
- U4 2x 10Pin Pin Header Male [AliExpress](https://aliexpress.com/item/1005005390193356.html)
