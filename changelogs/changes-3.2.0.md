# General
* Fixed restoring integer configuration values (QBS-1739).
* Updated QuickJS to version 0.11.0.

# Language
* The `SubProject` item can now override properties of a loaded `Product` item (QBS-1661).
* Module properties provided via the command line are now considered when evaluating
  the respective module's condition (QBS-1841).

# C/C++ support
* Added an `ObjectLibrary` item (QBS-1834).
* Added a `CppLibrary` convenience item.
* Added support for C++26.
* Added a `cpp.rpathLinkDirs` property (QBS-1501).
* With MSVC, manifest files are now also generated for DLLs (QBS-1857).

# Contributors
* Björn Schäpers
* Christian Kandeler
* Denis Shienkov
* Ivan Komissarov
