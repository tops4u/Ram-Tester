@echo off
REM Flash Ram_Tester firmware to ATmega328P via Arduino as ISP
REM Usage: flash.bat [COM port]
REM Example: flash.bat COM3

set PORT=%1
if "%PORT%"=="" set PORT=COM3
set HEX=Ram_Tester_4_0_2.hex

if not exist "%HEX%" (
  echo Error: %HEX% not found in current directory.
  exit /b 1
)

echo Flashing %HEX% to ATmega328P on %PORT% ...
avrdude -p atmega328p -c stk500v1 -P %PORT% -b 19200 ^
  -U lfuse:w:0xFF:m ^
  -U hfuse:w:0xD9:m ^
  -U efuse:w:0xFD:m ^
  -U flash:w:%HEX%:i

if %ERRORLEVEL%==0 (
  echo Done. Firmware flashed successfully.
) else (
  echo Error: Flashing failed. Check connection and port.
  exit /b 1
)
