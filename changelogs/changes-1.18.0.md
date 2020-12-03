# General

* capnp: The outputDir property is now public and read-only.
* setup-toochains: Include the Xcode version into the profile name
  profile when auto-detection an Xcode installation.
* innosetup module: Add support for InnoSetup v6
* JSON API: Use the full display name of multiplexed products
  in the dependencies array. This allows clients to for example to
  properly update the search path for multiplexed dynamic libraries.


# Language

* Deprecate the product variable inside Export items in favor of a new
  exportingProduct variable. It will be removed in Qbs 1.20 (QBS-1576).
* Qbs now checks string and stringList values according to the
  allowedValues property in the PropertyOptions item.


# Protocol Buffers Support (Protobuf Module)

* The deprecated protocBinary property has been removed. Use compilerPath
  instead.
* A nanopb submodule has been added.
* The outputDir property is now public and read-only.

# C/C++ Support

* baremetal: cpp.generateCompilerListingFiles has been implemented for
  KEIL ARM Clang
* baremetal: cpp.enableDefinesByLanguage does now work with SDCC as well.


# Qt Support

* Moc is now disabled when building aggregate products.


# Android Support

* Support for the new directory layout of Qt6 has been added (QBS-1609).
* Input file generation for androiddeployqt has been improved (QBS-1613).
* Debugging experience of multi-architecture Android projects with Qbs and
  Qt Creator has been improved. Binaries are now generated in a directory
  layout that Qt Creator expects and debug information is no longer stripped
  away.

# Documentation

* baremetal: A new WiFi access point example for the ESP8266 MCU using the
  GCC toolchain has been added.
* baremetal: A new example for Nordic's pca10001 board has been
  added. It supports GCC, KEIL and IAR.
* baremetal: The stm32f103 example supports IAR as well.
* A howto has been added showing how to easily disable compiler
  warnings.
* Instructions for building Qbs with CMake have been added (QBS-1618).

# Important Bug Fixes

* Qt modules could not be used in Export item when building for Android
  (QBS-1576).
* Variable substitution in Info.plist files was broken for
  '@VAR@' syntax (QBS-1601).
* CppApplication failed to build for Android when using Qt > 5.14.0
  and multiplexing over multiple architectures (QBS-1608).
* Moc output was broken when including Boost project header files (QBS-1621).

# Contributors

* Alberto Mardegan
* André Pönitz
* Christian Kandeler
* Christian Stenger
* Cristian Adam
* Denis Shienkov
* Eike Ziller
* Ivan Komissarov
* Jochen Ulrich
* Kai Dohmen
* Mitch Curtis
* Orgad Shaneh
* Raphaël Cotty
* Richard Weickelt
