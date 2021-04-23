# C/C++ Support

* qbs-setup-toolchains is now able to detect clang-cl properly
* The Library and Include probes take more paths into account on Linux to better
  support containerization systems such as Flatpak.
* Xcode autodetection now uses xcode-select to find Xcode on the system.


# Protocol Buffers Support (Protobuf Module)

* A missing nanopb generator file extension on windows has been added.
* The problem that property _libraryName was incorrect when protobuf was not
  found has been fixed.


# Android Support

* An assertion when building Android applications using additional java classes
  with native methods has been fixed (QBS-1628).


# Qt Support

* A problem related to handling Qt6EntryPoint in the Qt module provider has been
  fixed.


# Infrastructure

* A Qt4 docker image for basic testing has been added.


# Contributors

* Christian Kandeler
* Eike Ziller
* Ivan Komissarov
* Jan Blackquill
* Kai Dohmen
* Raphaël Cotty
* Richard Weickelt
