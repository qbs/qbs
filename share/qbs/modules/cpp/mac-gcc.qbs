import qbs 1.0
import '../utils.js' as ModUtils
import 'darwin-tools.js' as Tools

DarwinGCC {
    condition: qbs.hostOS === 'mac' && qbs.targetOS === 'mac' && qbs.toolchain === 'gcc'

    defaultInfoPlist: {
        var baseName = String(product.targetName).substring(product.targetName.lastIndexOf('/') + 1);
        var baseNameRfc1034 = Tools.rfc1034(baseName);
        var defaultVal = {
            CFBundleName: baseName,
            CFBundleIdentifier: "org.example." + baseNameRfc1034,
            CFBundleInfoDictionaryVersion: "6.0",
            CFBundleVersion: "1.0", // version of the app
            CFBundleShortVersionString: "1.0", // user visible version of the app
            CFBundleExecutable: baseName,
            CFBundleDisplayName: baseName,
            CFBundleIconFile: baseName + ".icns",
            CFBundlePackageType: "APPL",
            CFBundleSignature: "????", // legacy creator code in macOS Classic, can be ignored
            CFBundleDevelopmentRegion: "en" // default localization
        }
        return defaultVal
    }
}
