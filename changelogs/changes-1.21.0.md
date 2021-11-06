# General

* A new qbsModuleProviders property was added to Project and Product items which allows
  to specify which providers will be run (QBS-1604).
* Added a new library for reading *.pc files which allows to avoid launching multiple pkg-config
  processes and also gives QBS more information about dependencies between *.pc files (QBS-1615).
* A new qbspkgconfig provider was added which will replace the fallback provider (QBS-1614).
  This new provider uses the built-in library and is capable of setting Qt libraries as well.
* capnproto and protobuf modules can now use runtime provided by the qbspkgconfig provider.
* A new ConanfileProbe.verbose property was added which can be useful to debug problems with
  Conan.
* Qbs no longer migrates the "profiles/" dir from earlier Qbs versions (QTCREATORBUG-26475).
  Old directories might be cleaned up manually.
* FileInfo now always uses high-precision timer on all OSes.
* Fixed a problem with overriding stringList properties in ModuleProviders from command-line.

# C/C++ Support

* Added support for c++23.
* Add Elbrus E2K architecture for the GCC toolchain (QBS-1675).
* COSMIC cpp module now avoids using relative file paths as much as possible.
* Some refactoring was done in the cpp modules to share more code.

# Android Support

* Added Android.ndk.buildId property which allows to overwrite the default value (sha1) for
  the --build-id linker flag.
* Fixed reading *.prl files with Qt >= 6.0.
* Fixed rcc path with Qt >= 6.2.

# Documentation

* Added a new page with the list of ModuleProviders.
* Qt provider now has its own page.
* Clarified that application won't be runnable by default, unless env or rpaths are set correctly.

# Infrastructure

* Added standalone job for building documentation.
* Xcode version was bumped to 12.5.1.
* Added OpenSUSE Leap docker image.
* Added Android tests with different NDK versions.
* Fixed QMake build with Qt 6.

# Contributors

* Christian Kandeler
* Christian Stenger
* Davide Pesavento
* Denis Shienkov
* Ivan Komissarov
* Kai Dohmen
* Orgad Shaneh
* Raphaël Cotty
* Richard Weickelt
* Thorbjørn Lindeijer
