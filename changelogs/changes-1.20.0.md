# General

* CMake and Qbs builds now fully support building with Qt6.
* Qbs now prints the old properties set when refusing to build a project with
  changed properties.
* Added convenience command to qbs-config to add a profile in one go instead of
  setting properties separately (QTCREATORBUG-25463).
* Added profiling timer for module providers.

# C/C++ Support

* Added support for the COSMIC COLDFIRE (also known as M68K) compiler (QBS-1648).
* Added support for the COSMIC HCS08 compiler (QBS-1641).
* Added support for the COSMIC HCS12 compiler (QBS-1640).
* Added support for the COSMIC STM8 compiler (QBS-1639).
* Added support for the COSMIC STM32 compiler (QBS-1638).
* Added support for the new Digital Mars toolchain (QBS-1636).
* The new cpp.enableCxxLanguageMacro property was added for the MSVC toolchain
  that controls the /Zc:__cplusplus required for proper support of the new
  C++ standards (QBS-1655).
* Added support for the "c++20" language version for the MSVC toolchain
  which results in adding the /std:c++latest flag (QBS-1656).

# Qt Support

* Consider "external" modules
* Fix support for qml binaries that were moved to the libexec directory in Qt 6.2 (QBS-1636).

# Android Support

* Added option to use dex compiler d8 instead of dx.
* Ministro support was removed.
* Fix link with static stl on Android (QBS-1654)
* The default Android Asset Packaging Tool was change from aapt to aapt2.

# Apple Support

* Qbs now uses embedded build specs from Xcode 12.4 when bundle.useXcodeBuildSpecs is true
  instead of older specs from Xcode 9.2.

# Important Bug Fixes

* Fix handling cpp.linkerWrapper with the MSVC toolchain (QBS-1653).

# Infrastructure

* Windows-only tests are moved to a separate tst_blackbox_windows binary.
* Qbs was updated to 1.18.2 in Docker images as well as in GitHub actions jobs.
* Added Qbs build and tests with the MinGW toolchain.
* Added CMake build with Qt6 on Linux.

# Contributors

* Christian Kandeler
* Denis Shienkov
* Eike Ziller
* Ivan Komissarov
* Jan Blackquill
* Mitch Curtis
* Oswald Buddenhagen
* Raphaël Cotty
* Richard Weickelt
