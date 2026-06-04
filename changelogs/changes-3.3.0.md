# Language
* Scanner items now participate in change tracking (QBS-1092).

# C/C++ support
* Introduced new property `cpp.libraries` (QBS-1157).
* Added support for clang's `-ftime-trace` flag.
* Static linking on Unix systems will now use response files if necessary (QBS-751).
* Added support for the `LoongArch` architecture.
* Fixed WebAssembly support for emscripten >= 3.1.68 (QBS-1876)

# Qt support
* Moc scanner now supports the Q_NAMESPACE_EXPORT macro.

# Apple
* Fixed installation of code-signed static frameworks on macOS 26.

# Contributors
* Christian Kandeler
* Ivan Komissarov
* Maximilian Hrabowski
* Pino Toscano
* Thorbjørn Lindeijer
