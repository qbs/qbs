This example demonstrates how to build a bare-metal application using
GCC toolchain for Espressif ESP8266-WROOM02 MCU.

An application is designed to be flashed on the external SPI FLASH
with the following parameters:

 * SPI FLASH size: 16 MBit (2048 + 2048 KBytes)
 * SPI FLASH mode: DIO
 * SPI FLASH clock: 40 MHz
 * SPI FLASH map type: 5

An application is running as WiFi access point that discards all
connection attempts, and has the following WiFi parameters:
 * SSID: "HELLO FROM QBS"
 * Authentication mode: "open"
 * Beacon interval: 1000 milliseconds
 * Channel number: 3

An application requires the ESP8266 NON-OS SDK:

 * https://github.com/espressif/ESP8266_NONOS_SDK

the path to which should be set via the ESP8266_NON_OS_SDK_ROOT
environment variable.

To convert the ELF application to the final binary image (and also for
re-program the ESP8266 MCU), you can use the open source 'esptool'
utility (using the SPI FLASH parameters, provided above):

 * https://github.com/espressif/esptool

The following toolchains are supported:

  * GNU ESP8266 Toolchain
