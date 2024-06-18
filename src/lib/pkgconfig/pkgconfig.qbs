import qbs.FileInfo
import qbs.Utilities

QbsProduct {
    type: "staticlibrary"
    Depends { name: "cpp" }
    Depends { name: "qbsbuildconfig" }

    property stringList pcPaths: {
        var result = [];
        result.push(FileInfo.joinPaths(qbs.installPrefix, qbsbuildconfig.libDirName, "pkgconfig"));
        result.push(FileInfo.joinPaths(qbs.installPrefix, "share", "pkgconfig"));
        if (qbs.hostOS.contains("unix")) {
            result.push("/usr/lib/pkgconfig/")
            result.push("/usr/share/pkgconfig/")
        }
        return result
    }
    readonly property stringList pcPathsString: pcPaths.join(qbs.pathListSeparator)

    property bool withQtSupport: true

    readonly property stringList publicDefines: {
        var result = [];
        if (withQtSupport)
            result.push("QBS_PC_WITH_QT_SUPPORT=1")
        else
            result.push("QBS_PC_WITH_QT_SUPPORT=0")
        return result;
    }

    name: "qbspkgconfig"

    files: [
        "pcpackage.cpp",
        "pcpackage.h",
        "pcparser.cpp",
        "pcparser.h",
        "pkgconfig.cpp",
        "pkgconfig.h",
    ]

    cpp.defines: {
        var result = [
            "PKG_CONFIG_PC_PATH=\"" + pcPathsString + "\"",
            "PKG_CONFIG_SYSTEM_LIBRARY_PATH=\"/usr/" + qbsbuildconfig.libDirName + "\"",
        ]
        result = result.concat(publicDefines);
        return result
    }

    Export {
        Depends { name: "cpp" }
        cpp.defines: exportingProduct.publicDefines
        cpp.includePaths: [exportingProduct.sourceDirectory]
        cpp.staticLibraries: {
            if (qbs.toolchainType === "gcc" && cpp.compilerVersionMajor < 9)
                return ["stdc++fs"];
            return [];
        }
    }
}
