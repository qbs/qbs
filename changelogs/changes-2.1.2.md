# General
* Fixed handling JS floating-point values for x86.
* Fixed scope pollution and potential crash when assigning to provider properties (QBS-1747).
* Fixed potential access to freed JSValues (QBS-1751).

# Qt
* Fixed building against Qt with "profiling" build variant (QBS-1758).

# Apple
* Fixed bundle module with Xcode-less profiles.
* Fixed ApplicationExtension with Xcode-less profiles.

# Infrastructure
* Added CI job to be able to test XCode-less profiles on macOS.

# Contributors
* Christian Kandeler
* Dmitry Shachnev
* Ivan Komissarov
