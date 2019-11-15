# General
* Added a session command which offers a JSON-based API for interaction with
  other tools via stdin/ stdout. This allows for proper Qbs support in IDEs that
  do not use Qt or even C++.

# Language
* Probes are now evaluated before Profile items and can be used to create
  profiles on project level.
* AutotestRunner got a separate job pool.
* Added a timeout property to Command, JavaScriptCommand and AutotestRunner.
  This allows to identify and kill stuck commands.

# C/C++ Support
* Ensure proper support of Xcode 11.
* Linker map files can be generated with all toolchains.
* Bare metal toolchains can now generate listing files.
* Improve the command line output filtering of bare metal toolchains.
* Added support for clang in mingw mode on Windows.
* Added msp430 support to GCC and IAR.
* Added STM8 support to IAR and SDCC.
* Added IDE project generators for IAR Embedded Workbench for ARM, AVR, 8051,
  MSP430, and STM8 architectures.
* Added IDE project generators for KEIL uVision v4 for ARM and 8051
  architectures.
* Added more bare metal project examples for various target platforms.
* The IAR, KEIL and SDCC toolchains are now found automatically in various.
  locations by the setup-toolchains command and by probes if no installPath is
  set in the profile.

# Infrastructure
* Automated build and testing on Ubuntu, macOS and Windows.
* Added Ubuntu bionic Docker image which replaces Debian stretch.
* Updated Qt in the Ubuntu and Windows Docker images to 5.12.
* When building Qbs, Qt libraries can now be bundled on Linux, macOS and
  Windows.

# Contributors
* Alberto Mardegan <mardy@users.sourceforge.net>
* Christian Kandeler <christian.kandeler@qt.io>
* Denis Shienkov <denis.shienkov@gmail.com>
* Ivan Komissarov <ABBAPOH@gmail.com>
* Jochen Ulrich <jochenulrich@t-online.de>
* Joerg Bornemann <joerg.bornemann@qt.io>
* Richard Weickelt <richard@weickelt.de>
