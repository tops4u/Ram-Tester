## Decay Test - EXPERIMENTAL 

This tests the supported RAMs (currently this is 4164 / 4464 and 514256/44256) for retention time of the weakest Cell. This is an important metric in order to assess the capability of a RAM Chip to keep its Content. The Test will start a Ramp Up Phase where the retention time gets longer and longer until the first Cells start to fail. Then it will try to pinpoint the exact time the RAM is able to keep its content. Once this is probed it will just continue to soak the RAM with this timing to see if there is any change over time.

Normally the time a RAM is able to keep its information is a function of the temperature of the Chip Die since the leaking current of the transistor is growing with higher temps. As a rough rule of thumb, the retention time halves with every 10°C increase. The refresh or retention time from datasheets is usually specified at 70°C. So at Room Temp the RAM is able to keep its content much longer - values in the range of a hundred milliseconds up to several seconds are perfectly normal and no reason for concern. So maybe you want to heat it gently up with an SMD Rework Station or put the RAM preheated to 70° to check if they can keep the specification. For those RAM it is usually between 4 and 16ms.

When heating with hot air, keep in mind that you only measure the case temperature, not the die temperature, and that it is easy to overshoot. Use a low temperature setting, keep some distance and ideally place a thermocouple on the chip package.

This is an alternative Firmware and it is only experimental. Use at your own risk. It is currently only supporting 3 RAM Types. If you want to use this SW you will temporarily lose the normal RAM Tester Firmware.

This is how the Display looks like for a 4464 Test:

<img src="https://raw.githubusercontent.com/tops4u/Ram-Tester/refs/heads/main/Media/IMG_6160.jpeg" width="400px" align="center"/><br/>
