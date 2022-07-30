# C/C++ Support
* Added support for c17 and c2x values in cpp.cLanguageVersion.
* Added support for cpp.cLanguageVersion for the MSVC toolchain.
* Fix passing linker scripts to iar and keil toolchains (QBS-1704).

# Qt Support
* Adapted to new location of qscxmlc in Qt 6.3.
* Adapted to new location of qhelpgenerator in Qt 6.3.
* Fixed setting up Qt 6.3 with qbspkgconfig.
* Added QtScript module to the source tarballs (QBS-1703).

# Other modules
* Fixed protobuf linking on macOS 11.
* Fixed handling empty variables in qbspkgconfig (QBS-1702)

# Contributors
* Christian Kandeler
* Ivan Komissarov
* Orgad Shaneh
