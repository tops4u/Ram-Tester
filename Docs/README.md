# RAM Tester: Operation Manual

## 1. Basic Operation
Follow these steps to operate the tester correctly.

1.  **Identify Pin Count:** Determine how many pins your RAM chip has.
2.  **Set DIP Switches:** Adjust the DIP switches to match the number of pins on your chip.
3.  **Insert the RAM:**
    * Insert the chip into the socket.
    * **Note:** You may use either the ZIF (Zero Insertion Force) or ZIP (Zig-zag) socket, but **never populate both at the same time**.
    * > **WARNING:** The **441000** RAM type is only supported in the standard **DIP** version. Do **not** test it in the ZIP socket; it uses a different pinout and may cause damage.
4.  **Power Up:** Turn on the tester power supply or press the **RESET** button if already powered.
5.  **Test Sequence:**
    * The RAM should be recognized automatically.
    * If recognition fails, the RAM is likely defective.
    * During the test, the **LED will turn orange** and may flicker.
    * Tests can take up to **40 seconds** depending on the chip size.
6.  **Completion:** The final test result is reported on the display.

---

## 2. Interpreting Error Codes
If the tester reports an error, refer to the list below to diagnose the issue.

### Hardware Errors
* **Defect or no RAM** The Ram does work to provide simple functions so it can't be detected or identified.
* **GND Short:** A pin that should not have a short-circuit to ground is shorted.
    * *Note:* Pin numbers displayed correspond to the **ZIF Socket**.
* **Addressline Error:** The tester is unable to address the memory cells.
    * *Possible causes:* Broken address line inside the chip, or a tester fault (Decoder, Amplifier, or Buffer).

### Pattern Test Errors
The tester runs various data patterns to verify memory integrity:

* **Pattern 0:** All cells are linearly filled with '0'. Cells may be stuck at '1'.
* **Pattern 1:** All cells are linearly filled with '1'. Cells may be stuck at '0'.
* **Pattern 2:** Cells are written with an alternating pattern '010101...'. This is performed row-wise (Write Row -> Check Row). Checks for Crosstalk
* **Pattern 3:** Cells are written with the inverted pattern of Pattern 2 ('101010...'). This is performed row-wise. Checks for Crosstalk
* **Pattern 4:** Rows are filled with pseudo-random data. The tester waits (refresh cycle) and re-checks the data a few rows later to verify data retention.
* **Pattern 5:** Same as Pattern 4, but using inverted data.
* **Pattern 6 (CBR Test):** Performs a "CAS before RAS" test. This checks the internal Refresh Timer logic (specific to RAMs with internal refresh logic).

---

## 3. Self-Test & Calibration
Use this mode to verify the tester hardware is functioning correctly.

**Prerequisites:**
* Ensure you are running **Firmware 4.0** or newer.
* Set **ALL DIP Switches** to the **"OFF"** position.

**Procedure:**
1.  **Start Self-Test:** Reset the tester. It will automatically detect the "All OFF" state and start the self-test.
2.  **Step 1 (Resistors):** The unit tests the Pull-Up resistors of the DIP switches.
3.  **Step 2 (Shorts):** The unit tests for short circuits between the signal lines of the socket.
4.  **Step 3 (Manual Connection Test):**
    * You will need a jumper wire.
    * Connect one end to the **Top-Right Pin** (Pin 20 of the ZIP Socket).
    * Touch the other end of the wire to **all other pins** of the ZIP socket (or the ZIF socket).
    * The tester monitors the connections in real-time.
5.  **Success:** If all contacts register correctly, the tester reports an **OK** feedback.

---

## 4. Troubleshooting Self-Test Failures
If the Self-Test reports an error, check the specific components listed below:

* **Pull-Up Resistor Error:**
    * Check the three **1M Pull-up Resistors** located under the Display.
    * If they come loose or have a bad solder joint, the status of the DIP Switches will be read incorrectly.
* **Shorts Detected:**
    * There are connections between contacts of one of the two sockets that shouldn't be there.
    * *Action:* Check for solder bridges or debris on the PCB and remove them.
* **Connection Test Failure:**
    * If the test does not end even after touching all contacts, a signal is missing.
    * *Action:* Check for bad soldering points, a broken PCB trace, or a defective socket contact.

