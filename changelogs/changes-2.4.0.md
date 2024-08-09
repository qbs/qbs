# General
* Added a Conan module provider (QBS-1665).
* Added a flatbuffers module (QBS-1666).
* Rules trying to create artifacts outside the build directory is now a hard error (QBS-1268).
* More details are now printed when a command times out (QBS-1750).
* Updated the bundled quickjs library.

# Language
* The pkg-config based fallback provider was removed.
* It is no longer allowed to attach a QML id to a module item.

# Apple support
* Fixed symlinks for multi-arch binaries on Apple platforms (QBS-1797).

# C/C++ Support
* Added new cpp.importPrivateLibraries property that controls whether to automatically import
  external libraries from dependencies.

# Documentation
* Added more tutorials.

# Contributors
* Christian Kandeler
* Ivan Komissarov
* Kai Dohmen
* Raphael Cotty
