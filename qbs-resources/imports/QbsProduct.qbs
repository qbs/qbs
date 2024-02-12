Product {
    Depends { name: "qbsbuildconfig" }
    Depends { name: "qbsversion" }
    Depends { name: "Qt.core"; versionAtLeast: minimumQtVersion }
    property string minimumQtVersion: "5.15.2"
    property bool install: true
    property bool hasCMakeFile: true
    property string targetInstallDir
    cpp.defines: {
        var res = [
                    "QT_NO_CAST_FROM_ASCII",
                    "QT_NO_CAST_FROM_BYTEARRAY",
                    "QT_NO_PROCESS_COMBINED_ARGUMENT_START"
                ];
        if (qbs.toolchain.contains("msvc"))
            res.push("_SCL_SECURE_NO_WARNINGS");
        if (qbs.enableDebugCode)
            res.push("QT_STRICT_ITERATORS");
        return res;
    }
    cpp.cxxLanguageVersion: "c++20"
    cpp.enableExceptions: true
    cpp.rpaths: qbsbuildconfig.libRPaths
    cpp.minimumMacosVersion: "11.0"

    Group {
        name: "CMake"
        condition: hasCMakeFile
        prefix: product.sourceDirectory + '/'
        files: "CMakeLists.txt"
    }
}
