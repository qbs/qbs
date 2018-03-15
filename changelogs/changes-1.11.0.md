# General
* Added `qbs.targetPlatform` and `qbs.hostPlatform` properties which are scalar versions of
  `qbs.targetOS` and `qbs.hostOS`. `qbs.targetPlatform` is a "write-only" property that can be used
  to set the OS/platform that is being targeted, while `qbs.targetOS` and `qbs.hostOS` should
  continue to be used to *read* the OS/platform that is being targeted.
  `qbs.targetOS` is also now read-only.
* The "run" functionality as used by the command-line command of the same name now considers
  an executable's library dependencies, that is, it adds the paths they are located in to the
  respective environment variable (e.g. PATH on Windows).

# Language
* Modules can now declare target artifacts using the new `filesAreTargets` property of the
  `Group` item.
* The Module.setupRunEnvironment script now has a new parameter `config`. Users can set it via the
  `--setup-run-env-config` option of the `run` command. The only value currently supported
  is `ignore-lib-dependencies`, which turns off the abovementioned injection of library
  dependencies' paths into the run environment.
* Module.setupBuildEnvironment and Module.setupRunEnvironment now have access to the `product`
  and `project` variables. With regards to accessing module properties, these script now behave
  like rules, rather than normal properties.
* Added the `BinaryFile` service for reading and writing binary data files.
* The `SubProject` item now has a condition property.

# C/C++ Support
* Added property `cpp.rpathOrigin` which evaluates to `@loader_path` on Darwin and `$ORIGIN`
  on other Unix-like platforms.
* Added the `qbs.toolchainType` property, which is a scalar version of the `qbs.toolchain`
  property and is used to set the current toolchain.
* Added `cpp.driverLinkerFlags` for flags to be passed to the compiler driver only when linking.
* We now properly support `"c++17"` as a possible value of `cpp.cxxLanguageVersion`.
* The auto-detection mechanism for GCC-like compilers now considers typical mingw prefixes.

# Qt Support
* Added the Qt.scxml.generateStateMethods property to back the --statemethods option.

# Command-line interface
* Configuration names are now passed as "config:<name>".
* Options do not have to precede property assignments anymore.
* Referencing a non-existing product in a property override now results in an error.

# Documentation
* Major overhaul of the module and item reference for improved readability.
* Added a how-to on the topic of pre-compiled headers.
* Added documentation for the built-in XML support.
* Added documentation for qbs.Utilities.
* Added documentation on how to target specific platforms.

# Important bug fixes
* Fixed some inconsistencies related to item ids (QBS-1016, QBS-1262).
* Fixed slow project resolving on macOS (QBS-1277).
* Fixed problems with qtquickcompiler support in Qt 5.11 (QBS-1299).
* Fixed race conditions in multi-configuration builds (QBS-1308).

# Other
* The `InnoSetup`, `nsis`, and `wix` modules' rules now have a dependency on installable artifacts
  of dependencies.
* Introduced the `ico` module for creating .ico and .cur files.
