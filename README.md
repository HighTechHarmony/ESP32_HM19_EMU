# ESP32_HM19_EMU

## Overview

This is an emulator for the HM19 Bluetooth module, which allows devices with a serial UART to be bluetooth enabled.

## Hardware
This was built and tested on a WROOM32, AKA the Espressif ESP32 dev board.  It uses the following GPIO pins:
GPIO 17: Serial TX (connect to RX on enabling device)
GPIO 16: Serial RX (connect to TX on enabling device)


## Known issues
At this time only very basic functionality is working. Most AT commands are not yet supported. 


