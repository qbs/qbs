# General
* Added new module `Exporter.qbs` for creating qbs modules from products.
* Added new module `Exporter.pkgconfig` for creating pkg-config metadata files.
* Introduced the concept of system-level qbs settings.
* Added a Makefile generator.
* All command descriptions now contain the product name.

# Language
* The `explicitlyDependsOn` property of the `Rule` item no longer considers
  target artifacts of product dependencies. The new property `explicitlyDependsOnFromDependencies`
  can be used for that purpose.
* The `excludedAuxiliaryInputs` property of the `Rule` item has
  been renamed to `excludedInputs`. The old name is now deprecated.
* Added a new property type `varList`.
* Added `FileInfo.suffix` and `FileInfo.completeSuffix`.
* The deprecated JS extensions `XmlDomDocument` and `XmlDomElement`
  have been removed. Use `Xml.DomDocument` and `Xml.DomDocument` instead.

# C/C++ Support
* For MSVC static libraries, compiler-generated PDB files are
  now tagged as `debuginfo_cl` to make them installable.
* The `cxxLanguageVersion` property can now be set to different values in different modules,
  and the highest value will be chosen.

# Qt Support
* Amalgamation builds work properly now in the presence of "mocable" files.
* Fixed some redundancy on the linker command line.

# Other modules
* Added support for `%option outfile` and `%output` to the `lex_yacc` module.
* The `vcs` module now creates the header file even if no repository is present.

# Autotest support
* Added an `auxiliaryInputs` property to the `AutotestRunner` item for specifying run-time
  dependencies of test executables.
* The `AutotestRunner` item now has a `workingDirectory` property.
  By default, the respective test executable's location is used.

# Important bug fixes
* Disabled products no longer cause their exported dependencies to get pulled into
  the importing product (QBS-1250).
