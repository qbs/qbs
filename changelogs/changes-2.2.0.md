# General
* Improved speed of project resolving by employing multiple CPU cores, if available.
* Improved speed of probes execution on macOS.
* Object and array (var and varList) properties are now immutable in Probe items.

# Language
* Modules can now contain `Parameters` items.
* ModuleProviders can now contain `PropertyOptions` items.

# C/C++ support
* Allow `"mold"` as value for `cpp.linkerVariant`.
* The systemIncludePaths property is now handled correctly for clang-cl.

# Apple support
* Updated dmgbuild tool. This fixes bug that additional licenses are not shown in
  the combobox in the resulting DMG image.

# Qt support
* Only create qbs modules for those Qt modules that products actually need.
* Users can now opt out of using RPATH when linking on Linux.

# Other
* Protobuf module now requires pkg-config or built-in runtime.
* Protobuf module now requires C++17 on all platforms.
* Capnproto module: the outputDir property is now mutable.
* Added support for Groups to the VisualStudio generator.
* pkgconfig module provider: mergeDependencies property is deprecated.

# Contributors
* Christian Kandeler
* Dmitrii Meshkov
* Ivan Komissarov
* Nick Karg
* Serhii Olendarenko
* Thiemo van Engelen
* Thorbjørn Lindeijer
