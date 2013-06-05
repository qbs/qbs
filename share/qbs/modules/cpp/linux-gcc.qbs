import qbs 1.0

UnixGCC {
    condition: qbs.targetOS.contains('linux') && qbs.toolchain.contains('gcc')
    rpaths: ['$ORIGIN']
}

