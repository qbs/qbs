# General
* Improved speed of project resolving by employing multiple CPU cores, if available.
* Improved speed of probes execution on macOS.

# Language
* Modules can now contain `Parameters` items.
* ModuleProviders can now contain `PropertyOptions` items.

# C/C++ support
* Allow `"mold"` as value for `cpp.linkerVariant`.

# Qt support
* Only create qbs modules for those Qt modules that products actually need.
* Users can now opt out of using RPATH when linking on Linux.

# Protocol Buffers Support (Protobuf Module)
* This module now requires pkg-config.

# Contributors
* Christian Kandeler
* Dmitrii Meshkov
* Ivan Komissarov
* Nick Karg
* Serhii Olendarenko
* Thiemo van Engelen
* Thorbjørn Lindeijer
