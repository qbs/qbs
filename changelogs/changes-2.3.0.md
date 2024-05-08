# General
* Added an LSP language server that provides support for following symbols and completion
  in IDEs (QBS-395).
* Module properties are now directly available within groups in modules (QBS-1770).
* Added possibility to export products to CMake via the new Exporter.cmake module.
* Deprecated the pkgconfig-based fallback module provider.
* If a project needs to be re-resolved, we now print the reason.
* Added some tutorials.
* Wildards handling was rewritten to track changes more accurate.
* Module 'validate' scripts are no longer run for erroneous product in IDE mode.
* Add example how to use Exporters.

# C/C++ Support
* Private dependencies of products are not traversed more than once anymore (QBS-1714).

# Language
* Module properties are now accessible for groups in modules (QBS-1770).
* Fixed pathList properties in Probes (QBS-1785).
* The qbspkgconfig.mergeDependencies property was removed.
* ModuleProviders now support the 'allowedValues' property of the PropertyOptions item
  (QBS-1748).

# Apple
* Adapted darwin support to Xcode 15.3.

# CI
* Changed Linux Docker images from Focal to Jammy.
* Updated compilers and linters to recent versions.
* Added clang-format job to check code style.
* The project.withExamples property was removed.

# Contributors
* Christian Kandeler
* Dmitrii Meshkov
* Ivan Komissarov
* Raphael Cotty
* Richard Weickelt
