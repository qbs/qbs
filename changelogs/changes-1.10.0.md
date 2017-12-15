# General
* Added the `vcs` module to provide VCS repository information.
  Git and Subversion are supported initially.
* Added initial support for the Universal Windows Platform.
* Improved a lot of error messages.

# Language
* Profiles can now be defined within a project using the `Profile` item.
* Groups without a prefix now inherit the one of the parent group.
* Both the `Module` and the `FileTagger` item now have a `priority` property, allowing them to
  override conflicting instances.
* It is now possible to add file tags to generated artifacts by setting the new `fileTags` property
  in a group that has a `fileTagsFilter`.
* Added new open mode `TextFile.Append`.
* Added the `filePath` function to the `TextFile` class.
* `Process` and `TextFile` objects in rules, commands and configure scripts are now
  closed automatically after script execution.

# C/C++ Support
* Added the `cpufeatures` module for abstracting compiler flags related to CPU features such as SSE.
* Added property `cpp.discardUnusedData` abstracting linker options that strip unneeded symbols
  or sections.
* Added property `cpp.variantSuffix` for making binary names unique when multiplexing products.
* Added property `cpp.compilerDefinesByLanguage` providing the compiler's pre-defined macros.

# Android
* The deprecated `apkbuilder` tool is no longer used.

# Qt
* Added support for the Qt Quick compiler.
* Added support for `qmlcachegen`.

# Command-line interface
* Removed some non-applicable options from a number of tools.
* The `run` command can now deploy and run Android apps on devices, and deploy and run iOS and
  tvOS apps on the simulator.
* Added new command `list-products`.

# Documentation
* Added porting guide for qmake projects.
* Added in-depth descriptions of all command-line tools.
* Added "How-to" for creating modules for third-party libraries.
* Added a man page.
