# C/C++ Support

* Fix system include support with MSVC >= 19.29.30037

# Qt Support

* Fix possible command line length issue with qmlimportscanner when cross-compiling (QBS-1633).

# Apple platforms

* Fix stripping debug symbols in multiplexed products when cpp.separateDebugInformation
  is false (QBS-1647)
