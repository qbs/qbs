import qbs.base 1.0

GenericGCC {
    condition: qbs.hostOS == 'linux' && qbs.targetOS == 'linux' && qbs.toolchain == 'gcc'
}

