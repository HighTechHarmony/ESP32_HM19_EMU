# ESP32_HM19_EMU

## Overview
This is an emulator for the DSD TECH HM18/HM19 Bluetooth module, which allows devices with a serial UART to be bluetooth enabled.

## Hardware
This was built and tested on a Seeed Studio ESP32C3.  It uses the following GPIO pins:

GPIO D7: Serial RX (connect to TX on other device)

GPIO D6: Serial TX (connect to RX on other device)

You also need to (at least) connect ground pins on the devices.

For information see [https://github.com/espressif/arduino-esp32/blob/master/variants/XIAO_ESP32C3/pins_arduino.h]


## AT commands

This attempts to support a subset of commands familiar to the HM-18/HM-19. For more information, see [https://www.whizzbizz.com/p-httpd/multimedia/HM_18_HM_19_en_V1.pdf]

###  Commands that should be working:
`AT` (disconnect all clients)

`AT+ADTY0` (start advertising)

`AT+ADTY1` (stop advertising)

`AT+NAME?` (get name)

`AT+NAMENEWNAME` (set name to NEWNAME, will automatically reboot if set is successful)

`AT+ADDR?` (get address)

`AT+VERS?` or `AT+VERR?` (get version info)


## Known issues
At this time only very basic functionality is working. Most AT commands are not yet supported. 
