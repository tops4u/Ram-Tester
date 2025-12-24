This is the SMD Version

<img src="https://raw.githubusercontent.com/tops4u/Ram-Tester/refs/heads/main/Schematic/SMD/SMD_PCB_Render.jpg" width="400px" align="center"/>

## Build

**BOM for the mixed SMD/TH Version**
| Designator            | Qty | Manufacturer         | Mfg Part #              | Description / Value                          | Package/Footprint                                                   |
|-----------------------|-----|----------------------|-------------------------|----------------------------------------------|----------------------------------------------------------------------|
| C3,C2                 | 2   | Yageo               | CC0603JRNPO9BN120       | 12pF                                         | 0603                                                                 |
| C5                    | 1   | FH                  | 1206B106K100NT          | 10uF / 10V                                   | 1206                                                                 |
| C7,C4,C1              | 3   | Yageo               | CC0603KRX7R9BB104       | 100nF                                        | 0603                                                                 |
| D1                    | 1   | Kingbright          | APTB1615SURKCGKC-F01    | LED_Dual_AAKK                                | 0606 Duo LED                                                         |
| F1                    | 1   | Born                | SMD1206-020/30N         | 400mA Resetable Fuse                         | 1206                                                                 |
| J1                    | 0   | -                   | -                       | Conn_01x04_Pin                               | PinHeader_1x04_P2.54mm_Vertical                                      |
| J2                    | 0   | -                   | -                       | ICSP                                         | PinHeader_2x03_P2.54mm_Vertical                                      |
| J3                    | 1   | shou han            | TYPE-C 6P(073)          | USB_C_Receptacle_PowerOnly_6P               | USB_C_Receptacle_GCT_USB4125-xx-x-0190_6P_TopMnt_Horizontal         |
| Q1,Q2                 | 2   | ElecSuper           | BSS138BK                | BSS138                                       | SOT-23                                                               |
| R1                    | 1   | Fojan               |                         | 390R                                         | 0603                                                                 |
| R10,R9                | 2   | Fojan               | FRC0603F5101TS          | 5.1k                                         | 0603                                                                 |
| R2                    | 1   | Fojan               | FRC0603J102 TS          | 1k                                           | 0603                                                                 |
| R4,R5,R3              | 3   | Fojan               | FRC0603J105 TS          | 1M                                           | 0603                                                                 |
| R6                    | 1   | Fojan               | FRC0603J151 TS          | 150R                                         | 0603                                                                 |
| R8,R7                 | 2   | Fojan               | FRC0603J470 TS          | 47R                                          | 0603                                                                 |
| RN2,RN3,RN4,RN1       | 4   | FH                  | RCML08W470JT            | 47R                                          | 4x0603                                                               |
| SW1                   | 1   | -                   | -                       | SW_DIP_x03                                   | DIP-6_W7.62mm                                                        |
| SW2                   | 1   | XKB Connection      | TS-1102S-C-G-B          | SW_Push                                      | 6x6x7mm                                                              |
| U1                    | 1   | Microchip Technology| ATMEGA328P-AU           | ATmega328-P                                  | TQFP-32                                                              |
| U2                    | 0   | -                   | -                       | RAM / ZIF                                    | DIP-20_W7.62mm_LongPads                                             |
| U3                    | 0   | -                   | -                       | ZIP Socket                                   | ZIP-20                                                               |
| D2                    | 1   | Yageo               | SBD52C05L01             | ESD Clamp Diode                              | SOD523                                                               |
| FB1                   | 1   | Murata Electronics  | BLM18PG221SN1D          | Ferrite bead                                 | 0603                                                                 |
| Y1                    | 1   | HCI                 | 0153G2-16.000F12DTLJL   | 16MHZ                                        | 5032                                                                 [1]

Regarding R1/R6 depends on the LED you use. For the linked one of Aliexpress R6 could even be 500-560 Ohms.
