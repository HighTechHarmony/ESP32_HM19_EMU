# ESP32_HM19_EMU

## Overview
This is an emulator for the DSD TECH HM18/HM19 Bluetooth module, which allows devices with a serial UART to be bluetooth enabled.

## Hardware
This was built and tested on a Seeed Studio ESP32C3.  It uses the following GPIO pins:

GPIO 7: Serial RX (connect to TX on other device)

GPIO 6: Serial TX (connect to RX on other device)

You also need to (at least) connect ground pins on the devices.

## Known issues
At this time only very basic functionality is working. Most AT commands are not yet supported. 

These AT commands should be working:

`AT' (disconnect)

`AT+DTY0` (start advertising)

`AT+DTY1` (stop advertising)

