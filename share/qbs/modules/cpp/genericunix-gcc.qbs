import qbs 1.0

UnixGCC {
    condition: qbs.targetOS.contains('unix') && qbs.toolchain.contains('gcc')
}
