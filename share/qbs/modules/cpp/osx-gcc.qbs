import qbs 1.0

DarwinGCC {
    condition: qbs.hostOS === 'osx' && qbs.targetOS === 'osx' && qbs.toolchain === 'gcc'
}
