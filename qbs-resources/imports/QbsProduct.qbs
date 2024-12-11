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
                    "QT_NO_JAVA_STYLE_ITERATORS",
                    "QT_NO_PROCESS_COMBINED_ARGUMENT_START",
                    "QT_DISABLE_DEPRECATED_BEFORE=0x050f00",
                    "QT_DISABLE_DEPRECATED_UP_TO=0x050f00",
                    "QT_WARN_DEPRECATED_BEFORE=0x060700",
                    "QT_WARN_DEPRECATED_UP_TO=0x060700"
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
