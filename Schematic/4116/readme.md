# 4116 Adapter

This is the adapter board for testing **4116** and **4027** RAM chips. These chips require −5 V and +12 V supply voltages that the main tester board does not provide.

> **Note:** You need firmware version **3.x.x or higher** to use this adapter!

![4116 Adapter Board](https://github.com/tops4u/Ram-Tester/blob/main/Media/4116.jpg?raw=true)

---

## Why is it so complicated and expensive?

The RAM Tester was never originally designed to support 4116 RAM — this feature was added later due to popular demand.
When designing the adapter, I wanted to make sure it would be both easy to use and safe. That's why I included LEDs to indicate when all necessary voltages are present, as 4116 RAM can be very sensitive to missing or incorrect voltages.
I also added latch-up protection for the ICL7660 chip, which could otherwise lock up and overheat. Instead of simply using an off-the-shelf boost converter — which would have been far cheaper — I decided to design my own. This custom design limits current to 80 mA in case of a short circuit, whereas many off-the-shelf units can deliver up to 2 A, posing a risk of damage.
So yes, there are cheaper solutions out there, but they often come with trade-offs. It's entirely up to you whether you want to reuse this design or not.

---

## BOM

| Designation | Qty | Brand | Type | Description | Package |
|-------------|-----|-------|------|-------------|---------|
| C1, C2, C3 | 3 | Samsung EM | CL31A475KOHNNNE | 4.7 µF | 1206 |
| C4, C6 | 2 | Samsung EM | CL31A106KOHNNNE | 10 µF | 1206 |
| C5, C7 | 2 | Samsung EM | CL10B104KO8NNNC | 100 nF | 0603 |
| U1 | 1 | Renesas Electronics | ICL7660ACBAZA | Negative voltage converter | 8-SOIC |
| U3 | 1 | Texas Instruments | TLV61046ADBVR | Boost converter | SOT-23-6 |
| L1 | 1 | Laird | TYA2520104R7M-10 | 4.7 µH inductor | 1008 |
| D1 | 1 | Toshiba | CES521,L3F | Schottky diode | SOD-523 |
| D2, D3 | 2 | Amicc | A-SP192DGHC-C01-4T | LED green | 0603 |
| D4 | 1 | Nexperia | PDZ13B,115 | Zener diode | SOD-323 |
| D5 | 1 | Nexperia | PESD5V0S1BAF | TVS diode | SOD-323 |
| R1, R6 | 2 | Fojan | FRC0603F5600TS | 560 Ω | 0603 |
| R4 | 1 | Fojan | FRC0603F2003TS | 200 kΩ | 0603 |
| R7 | 1 | Fojan | FRC0603F4703TS | 470 kΩ | 0603 |
| R8 | 1 | Fojan | FRC0603F2202TS | 22 kΩ | 0603 |
| R10 | 1 | Fojan | FRC0603F1003TS | 100 kΩ | 0603 |
| R11 | 1 | Fojan | FRC0603F1202TS | 12 kΩ | 0603 |
| R12 | 1 | Fojan | FRC0603F8872TS | 88.7 kΩ | 0603 |
| Q1 | 1 | Nexperia | BC817,215 | NPN transistor | SOT-23 |
| Q2 | 1 | Infineon | BSS84PH6327XTSA2 | P-MOSFET | SOT-23 |
| FB1, FB2, FB3 | 3 | Murata Electronics | BLM18PG101SN1D | Ferrite bead | 0603 |
| F1 | 1 | Yageo | SMD0603B002TF | Fuse | 0603 |

---

## Important Notice

This circuit uses a **voltage inverter (ICL7660)**. As the name suggests, it inverts the supplied voltage.

- Example: if your USB power supply provides **+6 V**, the inverter will output **−6 V**.
- This exceeds the safe operating range of the **4116 RAM**:
  - Most RAM chips tolerate up to **+6.5 V**.
  - The 4116, however, only tolerates up to **−5.5 V**.

### Measuring the voltage

To ensure your power supply is behaving correctly, measure the voltage at the **16-pin ZIF socket** while it is unpopulated:

- Measure between **Pin 16 (GND)** and **Pin 1 (−5 V)**.
- The voltage must be between **−4.5 V** and **−5.5 V**.

---

## LED Indicators

The onboard LEDs will go off to indicate whether there is a short circuit on the **−5 V** or **+12 V** lines.

- If a short is detected, **remove the RAM immediately**.
- The adapter board can tolerate a permanent short:
  - Max current: **≈30 mA** on the −5 V line
  - Max current: **≈80 mA** on the +12 V line (version 3.4 introduced an additional 30 mA resettable fuse)

However, this still results in more than **1 W of power dissipation**, which will heat up the components significantly.

---
