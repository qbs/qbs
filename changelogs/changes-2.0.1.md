# General
* Fixed crash when importing missing JavaScript file (QBS-1730).

# C/C++ Support
* Fixed building applications with mingw toolchain and Qt6 (QBS-1724).

# Apple Support
* Added support for Xcode 14.3.
* Fixed codesigning on macOS (QBS-1722).
* Fixed detecting Xcode via xcode-select tool.

# Qt Support
* Fixed support for Qt 6.3 on iOS.
* Fixed install-qt.sh to properly support Qt for iOS.
* Do not setup Qt in qbspkgconfig when cross compiling (QBS-1717).

# Build System
* Fixed qbsbuildconfig module.
* Fixed build with Qt6.5.
* Updated CI to test via Qt 6.5 on macOS and Windows.
* Updated CI to test via Xcode 14.2 on macOS.

# Contributors
* Björn Schäpers
* Christian Kandeler
* Ivan Komissarov
* Kai Dohmen
