import qbs 1.0

UnixGCC {
    condition: qbs.targetOS.contains('unix')
            && !qbs.targetOS.contains('darwin') && !qbs.targetOS.contains('linux')  // ### HACK
            && qbs.toolchain.contains('gcc')
}
