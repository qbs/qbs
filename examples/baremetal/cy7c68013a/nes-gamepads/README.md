This example demonstrates how to build a bare-metal application using
different 8051 toolchains. It is designed for the target board
based on Cypress FX2 cy7c68013a chip.

It is possible to use the official development kit from the Cypress:

* https://www.cypress.com/documentation/development-kitsboards/cy3684-ez-usb-fx2lp-development-kit

but, a better solution is to buy the China's analogs or replacements,
e.g. on Aliexpress.

It implements a USB HID device that connects two 8-buttons NES
(Dendy) gamepads to a PC. The gamepads are connected to the
cy7c68013a chip as follows:

1. CLK - it is an output clock signal which generates by chip from
   the port A, pin 0 (PA0). This pin should be connected to the CLK
   inputs for both gamepads.

2. DATA1 - it is an input data signal which comes to chip on the
   the port A, pin 2 (PA2). This pin should be connected to the DATA
   output from the gamepad #1.

3. DATA2 - it is an input data signal which comes to chip on the
   the port A, pin 4 (PA4). This pin should be connected to the DATA
   output from the gamepad #2.

4. LATCH - it is an output clock signal which generates by chip from
   the port A, pin 6 (PA6). This pin should be connected to the LATCH
   inputs for both gamepads.

Actual schematic and pinouts depends on an used gamepads (with 7, 9
or other pins connectors) and a development boards.

Also, do not forget to connect the +3.3V and GND wires to the gamepads.
Then it is possible to play 8-bit NES games using various PC simulators.

The following toolchains are supported:

  * IAR Embedded Workbench
  * SDCC
  * KEIL C51
