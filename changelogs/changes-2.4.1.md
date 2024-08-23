# General
* Fix LibraryProbe to take into account libraries both with and without the "lib" prefix. This
  fixes the Conan provider for MinGW.
* Conan module provider no longer sets platform to "none" for baremetal toolchains (QBS-1795).

# CI
* CI now covers Conan tests for all platforms. Conan is also included in Linux images now.
* GDB is now included in Linux images.

# Contributors
* Christian Kandeler
* Ivan Komissarov