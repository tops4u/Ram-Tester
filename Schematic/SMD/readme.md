# SMD Version

<img src="https://raw.githubusercontent.com/tops4u/Ram-Tester/refs/heads/main/Schematic/SMD/SMD_PCB_Render.jpg" width="400px" align="center"/>

This is the mixed SMD/through-hole version of the RAM Tester. SMD components are used for the main circuit; the ZIF socket, ZIP socket, DIP switch and pin headers are through-hole.

## Files in this folder

| File | Description |
|------|-------------|
| `Schematic_SMD.pdf` | Schematic as PDF for reference |
| `Ram_Tester.kicad_sch` / `.kicad_pcb` / `.kicad_pro` | KiCad 9 source files |
| `Ram_TesterSMD_v2.1.zip` | Gerber files for PCB manufacturing |

**Notes:**
- J1 (display) and J2 (ICSP) are pin headers, not SMD — solder them as through-hole.
- R1 and R6 are LED current limitors. The values above work with the Kingbright dual LED. If you substitute a different LED (e.g. from AliExpress), R6 may need to be 500–560 Ω depending on forward voltage.
