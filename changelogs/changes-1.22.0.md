# General
* A new Host service was introduced, providing information about the host system that
  used to be available from the qbs module, but did not really belong there.
  In addition, some more qbs module properties have moved to the FileInfo service.
* The product variable in Export items now points to the importing product, rather than
  the exporting one.
* Probes are now also available in ModuleProvider items.

# C/C++ Support
* Added support for the Open Watcom toolchain.
* Reduced unneeded re-linking on Linux by ignoring changes to weak symbols in library
  dependencies by default.

# Qt Support
* Android multi-arch packages are supported again wth Qt >= 6.3.
* We now use cpp.systemIncludePaths for Qt headers, so that building Qt applications
  no longer triggers warnings from Qt headers unrelated to the user code.

# Infrastructure
* Added coverage for Digital Mars compiler.
* Added coverage for static Qt builds.

# Contributors
* Christian Kandeler
* Denis Shienkov
* Ivan Komissarov
* Jan Blackquill
* Leena Miettinen
* Marius Gripsgard
* Mitch Curtis
* Raphael Cotty
