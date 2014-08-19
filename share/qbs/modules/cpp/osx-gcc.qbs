import qbs 1.0
import qbs.ModUtils

DarwinGCC {
    condition: qbs.hostOS.contains('osx') && qbs.targetOS.contains('osx') && qbs.toolchain.contains('gcc')

    minimumOsxVersion: xcodeSdkVersion || (cxxStandardLibrary === "libc++" ? "10.7" : undefined)
}
