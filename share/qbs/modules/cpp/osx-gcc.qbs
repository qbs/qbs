import qbs 1.0
import '../utils.js' as ModUtils

DarwinGCC {
    condition: qbs.hostOS === 'osx' && qbs.targetOS === 'osx' && qbs.toolchain === 'gcc'
}
