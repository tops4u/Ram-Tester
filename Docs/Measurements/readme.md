# RAM Tester Measurements (SMD Version)

This document contains measurement results taken with the **SMD version of the RAM Tester** for documentation purposes.  

---

## Permanent Short Test  

Results from stress tests to observe how hot the fuse becomes (temperature measured directly on top of the fuse).  

- **1stGen_ShortTest** → First board revisions with green PCBs  
- **2ndGen_ShortTest** → Second revision with black PCBs, markings: none, *V2* or *V2.1*  

---

## 4116 Boards  

- **4116_-5V** → Detail of the −5 V supply line during test run.  
  - Datasheet specification: min −4.5V / max −5.5V (newer Specs up to -5.7V)
- **4116_Dataline** → Close-up of the data line during testing.  

---

## RAM Tester  

- **514400_CAS** → CAS signal detail during testing.  
  - Setup: 514400 in ZIP socket, probe in ZIF socket  
- **511400_RAND_WR** → Signals observed during a write cycle in the random pattern test (phase 4/5).  
  - Setup as above  
- **514400_RAND_RD** → Signals observed when reading back random data.  
  - Setup as above  
