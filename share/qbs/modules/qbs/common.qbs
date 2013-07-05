import qbs 1.0
import qbs.FileInfo

Module {
    property string buildVariant: "debug"
    property bool enableDebugCode: buildVariant == "debug"
    property bool debugInformation: (buildVariant == "debug")
    property string optimization: (buildVariant == "debug" ? "none" : "fast")
    property stringList hostOS: getHostOS()
    property stringList targetOS: hostOS
    property string pathListSeparator: hostOS.contains("windows") ? ";" : ":"
    property string pathSeparator: hostOS.contains("windows") ? "\\" : "/"
    property string profile
    property stringList toolchain
    property string architecture

    property string endianness
    PropertyOptions {
        name: "endianness"
        allowedValues: ["big", "little", "mixed"]
        description: "endianness of the target platform"
    }

    property bool install: false
    property string installDir
    property string installPrefix: ""
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
