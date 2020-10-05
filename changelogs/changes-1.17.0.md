# General

* The lookup order in PathProbe changed to [environmentPaths,
  searchPaths, platformEnvironmentPaths, platformSearchPaths].
* The pathPrefix and platformPaths properties have been removed from the
  PathProbe item. They were deprecated since Qbs 1.13.
* The protocBinary property in the protobuf module has been renamed to
  compilerPath.
* A new module capnp for Cap'n Proto in C++ applications has been added.
  Cap'n Proto is a serialization protocol similar to protobuf.
* The qbs-setup-android tool got a --system flag to install profiles
  system-wide similar to qbs-setup-qt and qbs-setup-toolchains.


# Language

* The product and project variables are now available on the
  right-hand-side of moduleProvider expressions and the default scope is
  product (QBS-1587).


# C/C++ Support

* Lots of improvements have been made on toolchain support for
  bare-metal devices in general. Bare-metal targets can be selected by
  setting qbs.targetPlatform to 'none'.
* KEIL: The ARMCLANG, C166 and C251 toolchains are now supported.
* IAR: National's CR16, Microchip's AVR32, NXP's M68K, Renesas'
  M8/16C/M32C/R32C/SuperH targets and RISC-V targets are now supported.
* GCC: National's CR16, NXP M68K, Renesas M32C/M32R/SuperH/V850 as well
  as RISC-V and Xtensa targets are now supported.
* MSVC: Module definition files can now be used to provide the linker
  with information about exports and attributes (QBS-571).
* MSVC: "/external:I" is now used to set system include paths (QBS-1573).
* MSVC: cpp.generateCompilerListingFiles is now supported to generate
  assembler listings.
* Xcode: macOS framework paths on the command line are now automatically
  deduplicated (QBS-1552).
* Xcode: Support for Xcode 12.0 has been added (QBS-1582).


# Qt Support

* The Qt for Android modules have been cleaned up. Support for ARMv5, MIPS and
  MIPS64 targets has been removed (QBS-1496).
* Initial support for Qt6 has been added.


# Android Support

* A packageType property has been added to the Android.sdk module which
  allows to create Android App bundles (aab) instead of apk packages
  only.
* A aaptName property has been added to the Android.sdk module which
  allows to use aapt2 (QBS-1562) since aapt has been deprecated.


# Documentation

* New bare-metal examples have been added and existing examples have
  been ported to more toolchains.
* A new how-to about cpp.rPaths has been added (QBS-1204).
* Various minor improvements have been made.


# Important Bug Fixes

* Building Qt for Android applications as static libraries has been
  fixed (QBS-1545).
* Trailing slashes are no longer removed from Visual Studio environment
  variables (QBS-1551).
* The MSVC cpp module did not use the cpp.distributionIncludePaths
  property (QBS-1572).
* The visual studio generator has been fixed to work with Visual Studio
  16.6 (QBS-1569).
* Fixed extraction of build information from CONFIG and QT_CONFIG
  variables in Qt installations (QBS-1387).
* The version number is no longer appended to .so files on Android
  (QBS-1578).
* Compiler defines are now correctly passed to moc when processing
  header files (QBS-1592).


# Contributors

* Alberto Mardegan
* Christian Gagneraud
* Christian Kandeler
* Christian Stenger
* Denis Shienkov
* Ivan Komissarov
* Jake Petroules
* Jochen Ulrich
* Mitch Curtis
* Oliver Wolff
* Raphaël Cotty
* Richard Weickelt
* Sergey Zhuravlev
