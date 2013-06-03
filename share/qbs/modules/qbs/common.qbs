import qbs 1.0
import qbs.fileinfo as FileInfo

Module {
    property string buildVariant: "debug"
    property bool enableDebugCode: buildVariant == "debug"
    property bool debugInformation: (buildVariant == "debug")
    property string optimization: (buildVariant == "debug" ? "none" : "fast")
    property string hostOS: getHostOS()
    property string pathListSeparator: hostOS === "windows" ? ";" : ":"
    property string pathSeparator: hostOS === "windows" ? "\\" : "/"
    property string targetOS: hostOS
    property var targetPlatform: {
        var platforms = [targetOS];
        if (targetOS === "linux")
            platforms.push("unix");
        else if (targetOS === "android")
            platforms.push("linux", "unix");
        else if (targetOS === "osx" || targetOS === "ios")
            platforms.push("darwin", "unix");
        return platforms
    }
    property string profile
    property string toolchain
    property string architecture

    property string endianness
    PropertyOptions {
        name: "endianness"
        allowedValues: ["big", "little", "mixed"]
        description: "endianness of the target platform"
    }

    property bool install: false
    property string installDir
    property string sysroot

    PropertyOptions {
        name: "buildVariant"
        allowedValues: ['debug', 'release']
        description: "name of the build variant"
    }

    PropertyOptions {
        name: "optimization"
        allowedValues: ['none', 'fast', 'small']
        description: "optimization level"
    }
}
