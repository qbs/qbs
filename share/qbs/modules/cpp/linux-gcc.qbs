import qbs.base 1.0

GenericGCC {
    condition: qbs.targetOS == 'linux' && qbs.toolchain == 'gcc'
}

