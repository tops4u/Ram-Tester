# 4116 Adapter Documentation

Assembly Video for the RAM Tester SMD Kit:<br/>

[![YouTube Assembly Video](https://i.ytimg.com/vi/SPN98FYMu7Y/0.jpg)](https://youtu.be/SPN98FYMu7Y "Assembly")<br/>


This is the **4116 Adapter**, in case you want to build it yourself.  

> **Note:** You need firmware version **3.x.x or higher** to use this adapter!  

There may be a followup Version with more ESD/EMI consideration. 

![image](https://github.com/tops4u/Ram-Tester/blob/main/Media/4116.jpg?raw=true)

### Why is it so complicated and expensive ### 
The RAM Tester was never originally designed to support 4116 RAM — this feature was added later due to popular demand.
When designing the adapter, I wanted to make sure it would be both easy to use and safe. That’s why I included LEDs to indicate when all necessary voltages are present, as 4116 RAM can be very sensitive to missing or incorrect voltages.
I also added latch-up protection for the 7660 chip, which could otherwise lock up and overheat. Instead of simply using an off-the-shelf boost converter — which would have been far cheaper — I decided to design my own. This custom design limits current to 80 mA in case of a short circuit, whereas many off-the-shelf units can deliver up to 2 A, posing a risk of damage.
So yes, there are cheaper solutions out there, but they often come with trade-offs. It’s entirely up to you whether you want to reuse this design or not.

## BOM
|Designation|Pcs|Brand|Type|Form|
|-----------|---|-----|----|----|
|C1, C2, C3|	3|	Samsung EM	|CL31A475KOHNNNE	C4.7uF|	1206
|U3|	1	|Texas Instruments	|TLV61046ADBVR	|Boost Converter|	SOT-23-6
|U1	|1	|Renesas Electronics|ICL7660ACBAZA|	Negative Voltage Converter	|8-SOIC
|L1|	1	|Laird|	TYA2520104R7M-10	|4.7uH	|1008
|D2, D3	|2	|Amicc	A-SP192DGHC-C01-4T|	LED Green	|0603
|R1, R6|2	|Fojan|	FRC0603F5600TS	|R560|	0603
|R4|	1	|Fojan	|FRC0603F2003TS	|R200k	0603
|R7|	1	|Fojan|	FRC0603F4703TS	|R470k	|0603
|C4, C6|	2	|Samsung |EM	|CL31A106KOHNNNE	|C10uF	|1206
|C5, C7|	2|	Samsung |EM	|CL10B104KO8NNNC	|C100nF	|0603
|D1|	1	|Toshiba	|CES521,L3F|	Shottky Diode|	SOD-523
|R11|	1	|Fojan|	FRC0603F1202TS	|R12k|	0603
|R12	|1	|Fojan	|FRC0603F8872TS	|R88.7k	|0603
|R8|	1	|Fojan	|FRC0603F2202TS	|R22k|	0603
|R10|	1	|Fojan|	FRC0603F1003TS|	R100k|	0603
|Q1	|1	Nexperia|	BC817,215	NPN |Transistor	|SOT-23
|Q2	|1	Infineon|	BSS84PH6327XTSA2	|P-MOSFET|	SOT-23
|FB1, FB2, FB3|	3	|Murata Electronics	|BLM18PG101SN1D	|Ferrite Beads	|0603
|F1|	1	|Yageo|	SMD0603B002TF	|Fuse	|0603
|D4	|1	|Nexperia|	PDZ13B,115	|Zener Diode	|SOD-323
|D5	|1|	Nexperia|	PESD5V0S1BAF|	TVS Diode	|SOD-323

---

## ⚠️ Important Notice  

This circuit uses a **voltage inverter (MAX7660)**. As the name suggests, it inverts the supplied voltage.  

- Example: if your USB power supply provides **+6 V**, the inverter will output **−6 V**.  
- This exceeds the safe operating range of the **4116 RAM**:  
  - Most RAM chips tolerate up to **+6.5 V**.  
  - The 4116, however, only tolerates up to **−5.5 V**.  

### Measuring the Voltage  

To ensure your power supply is behaving correctly, measure the voltage at the **16-pin ZIF socket** while it is unpopulated:  

- Measure between **Pin 16 (GND)** and **Pin 1 (−5 V)**.  
- The voltage must be between **−4.5 V** and **−5.5 V**.  

---

## LED Indicators  

The onboard LEDs only indicate whether there is a short circuit on the **−5 V** or **12 V** lines.  

- If a short is detected, **remove the RAM immediately**.  
- The adapter board can tolerate a permanent short:  
  - Max current: **−30 mA** on the −5 V line  
  - Max current: **≈80 mA** on the 12 V line - Version 3.4 introduced an additional 30mA resetable Fuse. 

⚠️ However, this still results in more than **1 W of power dissipation**, which will heat up the components significantly.  

---
