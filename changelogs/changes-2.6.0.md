# General
* Command descriptions are now printed with the product name as the prefix
  instead of the suffix.
* When building only specific products via the `-p` option, dependent products
  are not necessarily built in their entirety anymore. Instead, only the artifacts
  required for the requested products are built.

# Language
* Added new `Rule` property `auxiliaryInputsFromDependencies`.
* The `Rule` property `explicitlyDependsOnFromDependencies` now matches all
  artifacts from the dependencies, not just target artifacts.
* The version requirement in a `Depends` item is now checked earlier, fixing
  the problem that a non-matching version of a library could break project
  resolving if it was found before the matching version.

# API
* Added a mode that loads a build graph followed by a forced re-resolve.
* In IDE mode, messages of type "error" instead of "warning" are now emitted
  for errors that would stop the build in non-IDE mode (QBS-1818).

# Darwin support
* Added privacy manifest support for frameworks (QBS-1812).

# Qt Support
* Added `lupdate` support via the new `QtLupdateRunner` item (QBS-486).

# Contributors
* Christian Kandeler
* Ivan Komissarov
* Marcus Tillmanns
* Orgad Shaneh
* Turkaev Usman
