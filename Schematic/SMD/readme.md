***WARNING - THIS VERSION HAS ERRORS! DO NOT FORK/BUILD!***
New Version is uploaded but untested!

This is the SMD Version

<img src="https://raw.githubusercontent.com/tops4u/Ram-Tester/refs/heads/main/Schematic/SMD/SMD_PCB_Render.jpg" width="400px" align="center"/>

## Build
There are minor Footprint mistakes on this version. For the 100nF you can use 0603 or 0805 Type. The Footprint forthe LED is also 0805 instead of 0603 - still it will work with the smaller component on the larger pads as well. If you have stock of larger 100nF you may easily use those.

**BOM for the mixed SMD/TH Version**
- Y1 1x 16 MHz Quartz [AliExpress](https://aliexpress.com/item/1005006003764861.html)
- C1/C2/C7 4x 100nF 0603 Capacitor
- C2/C3 2x 15pf 0603 Capacitor [AliExpress](https://aliexpress.com/item/32966526545.html)
- C5 10µF 1206 Elko
- R3, R4, R5 3x SMD 0603 1M Resistor
- R2 1x SMD 0603 1k Resistor
- R1 1x SMD 0603 150R Resistor
- R6 1x SMD 0603 330R Resistor
- R7, R8 2x SMD 0603 47R Resistor
- RNx 4x SMD 0603 47R 4xArray
- U1 Atmega328P-AU SMD
- U2 1x ZIF Socket / DIP Socket 20 Pin [AliExpress](https://aliexpress.com/item/1005007205054381.html)
- D1 1x BiColor 0603 [AliExpress](https://aliexpress.com/item/1005006283807337.html) -> Note the seller claims 0805 but it is 0603
- SW1 1x 3 way Dip-Switch [AliExpress](https://aliexpress.com/item/4001205849246.html)
- SW2 Pushbutton [AliExpress](https://aliexpress.com/item/4000555847543.html)
- Q1, Q2 SMD BSS138 SOT-23
- F1 0.5A resetable Fuse [AliExpress](https://aliexpress.com/item/33008877817.html)
- J1 USB-C [AliExpress](https://aliexpress.com/item/1005007847045492.html)
  
If you want to use ICSP:
- ICSP 1x PinHeader 2x3 [AliExpress](https://aliexpress.com/item/4000303366348.html)
  
If you want to use the ZIP Socket
- U3 2x 10Pin Pin Header Female [AliExpress](https://aliexpress.com/item/32717301965.html)
