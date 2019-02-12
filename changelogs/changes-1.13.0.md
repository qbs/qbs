# General
* Added a lot more documentation.
* The `--show-progress` command line option is now supported on Windows.

# Language
* Introduced module providers.
* The `Depends` item now falls back to `pkg-config` to locate dependencies whose names do not
  correspond to a qbs module.
* Added the concept of job pools for limiting concurrent execution of commands by type.
* Added support for rules without output artifacts.
* Added `atEnd` function to the `Process` service.
* Added `canonicalPath` function to the `FileInfo` service.
* Removed the need to add "import qbs" at the head of project files.
* The `Application`, `DynamicLibrary` and `StaticLibrary` items now have properties for more
  convenient installation of target binaries.

# C/C++ Support
* Added recursive dependency scanning of GNU ld linkerscripts.
* Added new `cpp` property `linkerVariant` to force use of `gold`, `bfd` or `lld`.

# Qt Support
* It is no longer required to call `setup-qt` before building Qt projects.
* Introduced the property `Qt.core.enableBigResources` for the creation of "big" Qt resources.
* Static builds now pull in the default set of plugins as specified by Qt, and the user can
  specify the set of plugins by type.
* Files can be explicitly tagged as mocable now.

# Other modules
* Added `protobuf` support for C++ and Objective-C.
* Introduced the `texttemplate` module, a facility similar to qmake's `SUBSTITUTES` feature.

# Android Support
* The `AndroidApk` item was deprecated, a normal `Application` item can be used instead.
* Building Qt apps is properly supported now, by making use of the `androiddeployqt` tool.

# Autotest support
* Introduced the `autotest` module.
