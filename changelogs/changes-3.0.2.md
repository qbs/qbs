# General
* Fixed spurious complaint about property binding loop (QBS-1845).

# CI
* Ad-hoc signed macOS artifacts are now published by release jobs.

# C/C++ support
* Made defines and include paths for windres unique.
* Fixed support for STABLE and CURRENT releases for FreeBSD.

# Qt support
* Fixed building Qt apps for iOS-simulator when qbs.architecture is undefined.
* Fixed duplicate JSON metadata files appearing on qmlregistrar command lines.

# Contributors
* Björn Schäpers
* Christian Kandeler
