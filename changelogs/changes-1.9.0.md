# General
* Setting module property values from the command line can now
  be done per product.
* Introduced new properties `qbs.architectures` and `qbs.buildVariants`
  to allow product multiplexing by `qbs.architecture` and `qbs.buildVariant`,
  respectively.
* When rebuilding a project, the environment, project file and property
  values are taken from the existing build graph.

# Language
* `Depends` items can now be parameterized to set special module parameters
  for one particular product dependency. The new item type `Parameter`
  is used to declare such parameters in a module. The new item type
  `Parameters` is used to allow products to set default values for
  such parameters in their `Export` item.
* The functions `loadExtension` and `loadFile` have been deprecated and
  will be removed in a future version. Use the `require` function instead.

# Custom Rules and Commands
* Artifacts corresponding to the `explicitlyDependsOn` property
  are now available under this name in rules and commands.
* A rule's `auxiliaryInputs` and `explicitlyDependsOn` tags
  are now also matched against rules of dependencies, if these rules are
  creating target artifacts.
* Rules now have a property `requiresInputs`. If it is `false`, the rule will
  be run even if no artifacts are present that match its input tags.
* Added a new property `relevantEnvironmentVariables` to the `Command` class.
  Use it if the command runs an external tool whose behavior can be
  influenced by special environment variables.

# C/C++ Support
* Added the `cpp.link` parameter to enable library dependencies to be
  excluded from linking.
* When pulling in static library products, the new `Depends` parameter
  `cpp.linkWholeArchive` can now be specified to force all the library's
  objects into the target binary.
* When pulling in library products, the new `Depends` parameter
  `cpp.symbolLinkMode` can now be specified to control how the library
  is linked into the target binary on Apple platforms: specifically,
  whether the library is linked as weak, lazy, reexported, and/or
  upward (see the `ld64` man page for more information).
* The property `cpp.useCxxPrecompiledHeader`, as well as the variants for the
  other languages, now defaults to true.
* The property `cpp.cxxLanguageVersion` now gets mapped to MSVC's `/std` option,
  if applicable.

# Apple
* Added support for building macOS disk images.

# Android
* Product multiplexing is no longer done via profiles, but via architecture,
  employing the new `qbs.architectures` property (see above). As a result,
  the `setup-android` command now sets up only one profile, rather than
  one for each architecture.
* Added support for NDK Unified Headers.

# Documentation
* Added a "How-to" section.
