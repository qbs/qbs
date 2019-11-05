This example demonstrates how to build a bare-metal application using
different MSP430 toolchains. It is designed for the MSP-EXP430F5529LP
target board (based on msp430f5529 chip):

* http://www.ti.com/tool/MSP-EXP430F5529LP

It implements a USB HID device that connects two 8-buttons NES
(Dendy) gamepads to a PC. The gamepads are connected to the
msp430f5529 chip as follows:

1. CLK - it is an output clock signal which generates by chip from
   the port 6, pin 0 (P6.0). This pin should be connected to the CLK
   inputs for both gamepads.

2. DATA1 - it is an input data signal which comes to chip on the
   the port 6, pin 1 (P6.1). This pin should be connected to the DATA
   output from the gamepad #1.

3. DATA2 - it is an input data signal which comes to chip on the
   the port 6, pin 2 (P6.2). This pin should be connected to the DATA
   output from the gamepad #2.

4. LATCH - it is an output clock signal which generates by chip from
   the port 6, pin 3 (P6.3). This pin should be connected to the LATCH
   inputs for both gamepads.

Actual schematic and pinouts depends on an used gamepads (with 7, 9
or other pins connectors) and a development boards.

Also, do not forget to connect the +3.3V and GND wires to the gamepads.
Then it is possible to play 8-bit NES games using various PC simulators.

The following toolchains are supported:

  * IAR Embedded Workbench
  * GCC
