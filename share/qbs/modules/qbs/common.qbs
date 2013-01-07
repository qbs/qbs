import qbs.base 1.0
import qbs.fileinfo 1.0 as FileInfo

Module {
    property string buildVariant: "debug"
    property bool enableDebugCode: buildVariant == "debug"
    property bool debugInformation: (buildVariant == "debug")
    property string optimization: (buildVariant == "debug" ? "none" : "fast")
    property string hostOS: getHostOS()
    property string hostArchitecture: getHostDefaultArchitecture()
    property string pathListSeparator: hostOS === "windows" ? ";" : ":"
    property string pathSeparator: hostOS === "windows" ? "\\" : "/"
    property string targetOS
    property var targetPlatform: {
        if (targetOS === "linux")
            return [ "unix", "linux" ];
        else if (targetOS === "android")
            return [ "unix", "android" ];
        else if (targetOS === "mac")
            return [ "unix", "darwin", "mac" ];
        else if (targetOS === "windows")
            return [ "windows" ];
        else
            throw "Unknown targetOS: \"" + targetOS + "\""
    }
    property string profile
    property string toolchain
    property string architecture
    property string endianness
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
