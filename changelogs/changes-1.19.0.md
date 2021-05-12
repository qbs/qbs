# General

* New codesign module was added to implement code signing for Apple, Android and Windows
  platforms (QBS-899, QBS-1546).
* It is now possible to build Qbs with Qt 6.
* Project files update API depending on the Qt.gui module was removed. This allows to enable
  project files update API unconditionally when building Qbs.
* Convenience items such as Application, DynamicLibrary and StaticLibrary now properly install
  artifacts when multiplexing.

# C/C++ Support

* Qbs now supports multiple MSVC compiler versions installed in one Visual Studio installation
  (QBS-1498). Also, multiple compiler versions are properly detected by qbs setup-toolchains.
* It is now possible to specify Windows SDK version for the MSVC and clang-cl toolchains via the
  new cpp.windowsSdkVersion property.
* baremetal: Fix generation of compiler listing files with custom extension for the SDCC compiler.
* baremetal: Fix generation of compiler listing files for the ARMCC compiler.
* baremetal: Fix detection for Keil toolchains.
* baremetal: Add support for HCS08 architectures to SDCC and IAR toolchains (QBS-1631, QBS-1629).
* baremetal: Add support for HCS12 architectures to GCC and IAR toolchains (QBS-1630, QBS-1550).

# Qt Support

* Fix possible command line length issue with qmlimportscanner (QBS-1633).
* Fix accessing binaries from libexec for Qt 6.1 and above (QBS-1636).

# Android Support

* Added a workaround for the Qt.Network module dependencies for 5.15.0 < Qt < 5.15.3 (QTBUG-87288)
* Fix aapt command invocation on Windows.
* Added support for ndk 22.1.7171670.

# Documentation

* Added How-To about codesigning on Apple platforms.
* Cocoa Touch Application example is brought up-to date to use modern Apple practices.
* Added example how to use the cpp.linkerVariant property.
* Added missing documentation for the cpp.toolchainInstallPath property.
* Added missing documentation for the supported 'bare-metal' architectures.

# Important Bug Fixes

* Added support for Xcode 12.5 (QBS-1644).
* Fix support for Python 3.9 for building Apple DMG images (QBS-1642).

# Infrastructure

* Ubuntu Focal image was updated to use Qt 5.15.2 and Qbs 1.17.1.
* Added automated tests for Qt 6 for macOS, Linux and Windows.
* Added a self-hosted runner to run 'bare-metal' tests on Windows.

# Contributors

* Andrey Filipenkov
* Denis Shienkov
* Christian Kandeler
* Jan Blackquill
* Jake Petroules
* Ivan Komissarov
* Max Bespalov
* Mitch Curtis
* Orgad Shaneh
* Raphaël Cotty
* Richard Weickelt
