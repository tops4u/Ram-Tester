#!/bin/bash
# Flash Ram_Tester firmware to ATmega328P via Arduino as ISP
# Usage: ./flash.sh [port]
# Example: ./flash.sh /dev/ttyUSB0
#          ./flash.sh /dev/cu.usbmodem1101

PORT=${1:-/dev/ttyUSB0}
HEX="Ram_Tester_4_0_2.hex"
AVRDUDE=${AVRDUDE:-avrdude}

if [ ! -f "$HEX" ]; then
  echo "Error: $HEX not found in current directory."
  exit 1
fi

echo "Flashing $HEX to ATmega328P on $PORT ..."
$AVRDUDE -p atmega328p -c stk500v1 -P "$PORT" -b 19200 \
  -U lfuse:w:0xFF:m \
  -U hfuse:w:0xD9:m \
  -U efuse:w:0xFD:m \
  -U flash:w:"$HEX":i

if [ $? -eq 0 ]; then
  echo "Done. Firmware flashed successfully."
else
  echo "Error: Flashing failed. Check connection and port."
  exit 1
fi
