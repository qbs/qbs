# Language
* The `PathProbe` item was extended to support looking for multiple files and filtering candidate
  files.

# C/C++ Support
* Added support for Visual Studio 2019.
* Added support for clang-cl.
* Various improvements for bare-metal toolchains, including new example projects and support for
  the SDCC toolchain.

# Qt Support
* Added the `Qt.android_support.extraLibs` property.

# Other modules
* The `pkgconfig` module now has a `sysroot` property.
* Added gRPC support to the `protobuf.cpp` module.

# Android Support
* Removed support for NDK < r19.
* Added new `Android.sdk` properties `versionCode` and `versionName`.

# Infrastructure
* Added configuration files for Travis CI.
* Various fixes and improvements in the Debian Docker image; updated to to Qt 5.11.3.

# Contributors
* BogDan Vatra <bogdan@kde.org>
* Christian Kandeler <christian.kandeler@qt.io>
* Christian Stenger <christian.stenger@qt.io>
* Davide Pesavento <pesa@gentoo.org>
* Denis Shienkov <denis.shienkov@gmail.com>
* hjk <hjk@qt.io>
* Ivan Komissarov <ABBAPOH@gmail.com>
* Joerg Bornemann <joerg.bornemann@qt.io>
* Richard Weickelt <richard@weickelt.de>
