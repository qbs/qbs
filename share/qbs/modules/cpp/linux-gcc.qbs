import qbs 1.0

UnixGCC {
    condition: qbs.targetOS === 'linux' && qbs.toolchain === 'gcc'
    rpaths: ['$ORIGIN']
}

