# Through-Hole (THT) Version

<img src="https://raw.githubusercontent.com/tops4u/Ram-Tester/refs/heads/main/Schematic/TH/THT_v3_Render.jpg" width="400px" align="center"/>

The through-hole version is designed for easy hand-soldering. It is functionally identical to the SMD version but with a few differences:

- No ESD/EMI protection components
- Micro-USB instead of USB-C (6-pin USB-C THT connectors are hard to source)

## Board revisions

### v3.0 
- Fixes two minor errors from v2.0
- Moves the OLED display to the centre of the board so the ICSP header remains accessible even with the display soldered in place
- Files: `Gerber_TH_v3.zip`, KiCad source files in this folder

## BOM

The bill of materials is available as `BOM_TH_RamTester.csv` in this folder.
