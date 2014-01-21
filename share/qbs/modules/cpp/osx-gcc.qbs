import qbs 1.0
import qbs.ModUtils

DarwinGCC {
    condition: qbs.hostOS.contains('osx') && qbs.targetOS.contains('osx') && qbs.toolchain.contains('gcc')
}
