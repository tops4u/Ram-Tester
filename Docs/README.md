# RAM Tester: Operation Manual

## Capabilities

Check whether your firmware supports the testing of half-good 4164 RAMs. Check the firmware version (select an invalid DIP switch configuration like all off or multiple on), then the tester displays the firmware version. If it is written in black on a white background and has `32` as a suffix — support is active for MSM3732 and TMS4532. If it is white text on black and does only show the version string like `Ver.:5.0.5` without `32` at the end, then the active firmware does not support those. For details on the 32K mode check the [32K-Option section](32K-Option).

**Current Firmware Release is 5.0.5**. If your firmware is below, please consider updating it.

### Further documentation in this folder

| Document | Description |
|----------|-------------|
| [Manual_4.x.pdf](Manual_4.x.pdf) | Operation manual for the RAM Tester |
| [Manual_4116.pdf](Manual_4116.pdf) / [Handbuch_4116.pdf](Handbuch_4116.pdf) | Assembly guide and operation manual for the 4116 adapter (EN / DE) |
| [Update_EN.pdf](Update_EN.pdf) / [Update_DE.pdf](Update_DE.pdf) | Step-by-step firmware update guide using an Arduino as ISP programmer (EN / DE) |
| [32K-Option/](32K-Option) | Documentation on 3732/4532 quadrant testing |
| [Measurements/](Measurements) | Oscilloscope captures from various test runs |
| [Archive/](Archive) | Documentation and manuals for older firmware |

---

## 1. Basic Operation
Follow these steps to operate the tester correctly.

1.  **Identify Pin Count:** Determine how many pins your RAM chip has.
2.  **Set DIP Switches:** Adjust the DIP switches to match the number of pins on your chip:

    | DIP Switch | RAM Pin Count | Example Types |
    |------------|---------------|---------------|
    | All ON | Setup mode | Adjust Tester behaviour |
    | 1 ON | 20-pin | 4027<sup>1)</sup>, 4116<sup>1)</sup>, 44256, 44258, 514256, 514258, 514400, 514402 |
    | 2 ON | 18-pin | 4416, 4464, 411000, **2114 (Turn 180°!)**|
    | 3 ON | 16-pin | 3732, 4532, 4816, 4164, 41256, 41257 |
    | All OFF | Self-test mode | (no RAM inserted) |

    Any other combination (multiple ON, etc.) shows the firmware version.

    <sup>1)</sup>Requires the 4116-Adapter to be in Place

4.  **Insert the RAM:**
    * Insert the chip into the socket.
    * **Note:** You may use either the ZIF (Zero Insertion Force) or ZIP (Zig-zag) socket, but **never populate both at the same time**.
    * > **WARNING:** The **411000** RAM type is only supported in the standard **DIP** socket. Do **not** test it in the ZIP socket; it uses a different pinout and may cause damage.
    * > **WARNING:** The **2114 SRAM** has a different Power Pinout, you need to rotate it 180° so that Pin 1 is in the lower righthand corner, still align it all the way up in the ZIF Socket.
5.  **Power Up:** Turn on the tester power supply or press the **RESET** button if already powered.
6.  **Test Sequence:**
    * The RAM should be recognized automatically.
    * If recognition fails, the RAM is likely defective.
    * During the test, the **LED will turn orange** and may flicker.
    * Tests can take up to **30 seconds** depending on the chip size (see the [supported DRAM types table](../README.md#supported-dram-types-speed-with-current-firmware-version)).
7.  **Completion:** The test result is reported on the display.
8.  **Loop Mode:** If you have enabled Loop Mode and your RAM supports CAS-Before-RAS (CBR) Refresh, this function will now be tested every 10th round. This Test must take 1 Minute in order to have Cells decay if the refresh counter does not work. The Countdown of the 60 Seconds is visible on the Display and the LED blinks 1/s. Once the test was successfull the next "normal" Tests are run. However if the RAM does not have a refresh counter, the Tester skipps CBR and immediately starts the next test loop. It will automatically stop on error or when reaching 1'000'000 runs.
---

## 2. Setup Mode
In order to enter the Tester Setup, you must switch on all the DIP Switches and reset the Tester. Remove all RAMs before doing so. 
A brief Display will be shown with "SETUP", the current Firmware and the QR Code to guide you to the Github Docs Page. 

To Toggle the Switches ON<->OFF simply manipulate the corresponding DIP Switch. The Screen has 3 Lines:

1. **32K Mode**: This enables or disables the 32k Test. If you are only running 4164 RAM and don't have any MSM3732 or TMS4532 you may disable the support for those, as a defect 4164 may be identified as "Half-Good" 3732/4532 RAM. 
2. **Loop Mode**: If you want RAMs to be tested continuously you may Enable the Loop Mode (see above Chapter 2.8). For fast Function Tests without CBR Mode or just a one shoot result, you may disable this feature. 
3. **Display Dimming**: Especially if you let your Tester run for a longer period of time, the display may be dimmed in order to prevent burn in or damage to the OLED Screen. 

Once you are done with the setting, simply reset the tester or unplug it. Every change of the DIP Switches is immediately synced to the EEPROM on the Controller. 

When you receive the Tester or Upgrading the Firmware the following Settings are active:

**32K Mode** : ON
**Loop Mode**: OFF
**Display Dim**: OFF

---

## 3. Interpreting Error Codes
If the tester reports an error, refer to the list below to diagnose the issue.

### Hardware Errors
* **Defect or no RAM:** The RAM does not provide enough basic function to be detected or identified.
* **GND Short:** A pin that should not have a short-circuit to ground is shorted.
    * *Note:* Pin numbers displayed correspond to the **ZIF socket**.
* **Addressline Error:** The tester is unable to address the memory cells.
    * *Possible causes:* Broken address line inside the chip, or a tester fault (decoder, amplifier, or buffer).
* **Short Addressline:** Two address lines are shorted to each other (checked before the memory test starts).
    * *Possible causes:* A solder bridge between two address pins, or a chip that internally shorts two address lines.

### Pattern Test Errors
The tester runs various data patterns to verify memory integrity:

* **Error Checkerboard** Cells are filles with a checkerboard pattern. This is done twice once by writing in one direction and reading in the opposite, then the other way around. For RAMs that have a refresh counter it will fill the whole RAM and then read it back. For RAMs without it is using 2 subsequent Rows. This checks column and row interaction between cells and Stuck Cells as well. 
* **Error RandomData** A pseudorandom pattern is used row-wise to be written and then read back after the refresh time has expired. This checks propper adressing, refresh and Crosstalk. This Test is run a second time with inverted pattern. 
* **CBR Timer fault** Performs a "CAS before RAS" test. This checks the internal refresh timer logic (specific to RAMs with internal refresh logic). This Test is only run in Loop Mode and takes 60 seconds to complete.
* **SRAM Error** A detected 2114 SRAM failed its functional test — a control-signal or memory-cell fault found by the March C- algorithm. The chip is present but defective. (The exact sub-test is encoded on the LED as the orange-blink count.) 

---

## 4. Self-Test & Calibration
Use this mode to verify the tester hardware is functioning correctly.

**Prerequisites:**
* Ensure you are running **Firmware 4.0** or newer.
* Set **ALL DIP Switches** to the **"OFF"** position.

**Procedure:**
1.  **Start Self-Test:** Reset the tester. It will automatically detect the "All OFF" state and start the self-test.
2.  **Step 1 (Resistors):** The unit tests the pull-up resistors of the DIP switches.
3.  **Step 2 (Shorts):** The unit tests for short circuits between the signal lines of the socket.
4.  **Step 3 (Manual Connection Test):**
    * You will need a jumper wire.
    * Connect one end to the **top-right pin** (Pin 20 of the ZIP socket).
    * Touch the other end of the wire to **all other pins** of the ZIP socket (or the ZIF socket).
    * The tester monitors the connections in real-time.
5.  **Success:** If all contacts register correctly, the tester reports an **OK** feedback.

 The following Video shows the function of the Self-Test Mode:<br>
 [![Self-Test Demo](https://img.youtube.com/vi/qTgzcuxilXE/0.jpg)](https://youtu.be/qTgzcuxilXE)

---

## 5. Troubleshooting Self-Test Failures
If the self-test reports an error, check the specific components listed below:

* **Pull-Up Resistor Error:**
    * Check the three **1 MΩ pull-up resistors** located under the display.
    * If they come loose or have a bad solder joint, the status of the DIP switches will be read incorrectly.
* **Shorts Detected:**
    * There are connections between contacts of one of the two sockets that shouldn't be there.
    * *Action:* Check for solder bridges or debris on the PCB and remove them.
* **Connection Test Failure:**
    * If the test does not end even after touching all contacts, a signal is missing.
    * *Action:* Check for bad soldering points, a broken PCB trace, or a defective socket contact.

## 6. Assembly of the SMD Kit
 For Assembly please check the following Video<br>
[![YouTube Assembly Video](https://i.ytimg.com/vi/SPN98FYMu7Y/0.jpg)](https://youtu.be/SPN98FYMu7Y "Assembly")<br/>
