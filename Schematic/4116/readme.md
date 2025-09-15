# 4116 Adapter Documentation

This is the **4116 Adapter**, in case you want to build it yourself.  

> **Note:** You need firmware version **3.x.x or higher** to use this adapter!  

Schematics will be published soon.  

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
  - Max current: **≈80 mA** on the 12 V line  

⚠️ However, this still results in more than **1 W of power dissipation**, which will heat up the components significantly.  

---
