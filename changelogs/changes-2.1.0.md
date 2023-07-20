# General
* Improved speed and correctness of project resolving.
* Fixed possible segmentation fault when quitting a session.
* Fixed regression in BinaryFile (QBS-1740).
* Added possibility to import and export Qbs settings in the JSON format (QBS-1685).

# Modules
* Dependencies are no longer merged by default in the qbspkgconfig module provider (QBS-1710).
* Protobuf modules now export the desired c++ version (c++17 on macOS, c++14 otherwise).

# Apple Support
* Updated dmgbuild to the upstream.

# Documentation
* Added documentation for the path, filePath, product and project variables.
* Added sample codesign settings to the Cocoa Touch Application example.

# Build System
* Updated Qt static Docker image to Qt 6.5.0 and Qbs 1.24.

# Contributors
* Andrey Filipenkov
* Christian Kandeler
* Denis Shienkov
* Ivan Komissarov
* Marc Mutz
* Raphael Cotty
* Thiemo van Engelen
