# General
* Qbs now prints information about files being installed.
* New modules were added for fine-tuning builds: `config.build`, `config.install` and
  `installpaths`.
* The `install` property in convenience items such as `CppApplication` now defaults to `true`.
* The `texttemplate` module received support for an alternative syntax (QBS-1833).
* The `Process` service now has an `errrorString` method.
* The `vcs` module now supports the `Mercurial` tool and there are new properties `repoLatestTag`,
  `repoCommitsSinceTag`, and `repoCommitSha`.
* Added a `Graphviz` generator to visualize project structures.
* The bundled QuickJS was updated to version 0.10.1.

# C/C++ support
* The `CppStd` convenience item was introduced to be able to use the `std.cppm` C++ module
  without having to build it for every product.
* The gcc module now detects the `Alpha` architecture.

# API
* The LSP support was extended to implement "go to definition" for module names, module properties,
  user-defined items and the content of the `files` and `references` properties.

# Qt Support
* Fixed iOS support with Qt >= 6.8 (QBS-1839).

# Apple support
* Added support for Xcode 26.0.

# Contributors
* Christian Kandeler
* Ivan Komissarov
* Jan Blackquill
* Pino Toscano
* Roman Telezhynskyi
