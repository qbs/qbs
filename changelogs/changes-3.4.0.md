# General
* Added the `qbs-init` tool to create a project from scratch.
* The `list-products` command now has a `--tags` option.
* `qbs run` now releases the build graph, allowing a concurrent re-build.
* Updated QuickJS to v0.16.2.

# Language
* Scanners can now attach properties to artifacts.
* Scanners now have a `scannerId` property.

# C/C++ support
* Modules:
  * Added support for importing internal partitions into implementation units.
  * Fixed a number of mis-parsings resulting in spurious detection of module
    and import declarations.
* Added support for the `wild` linker to `cpp.linkerVariant`.
* Added the `cpp.pchDependsOnSystemHeaders` property to prevent pre-compiled headers breaking
  the build after a toolchain update.
* `.def` files are now supported with the `MinGW` toolchain (QBS-1869).

# Qt support
* Added support for module-aware moc (>= Qt 6.13).
* Added support for QML modules via the new `QmlModule` item.

# Code signing
* Added code signing support to BareMetal.
* Added code signing support to the `wix` module.
* Allow code signing via `osslsigncode`.
* Allow signing `NSIS` and `Inno Setup` installers (QBS-1859).
* Allow signing binaries created by `MinGW`.

# Contributors
* Christian Kandeler
* Denis Shienkov
* Heiko Nardmann
* Ivan Komissarov
* Roman Telezhynskyi
