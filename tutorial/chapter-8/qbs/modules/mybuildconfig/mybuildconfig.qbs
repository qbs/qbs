import qbs.FileInfo

Module {
    Depends { name: "cpp" }

    property string appInstallDir: "bin"
    property string libDirName: "lib"
    property string libInstallDir: qbs.targetOS.contains("windows") ? "bin" : libDirName
    property bool staticBuild: false
    property bool installStaticLib: true
    property bool enableRPath: true

    property stringList libRPaths: {
        if (enableRPath && cpp.rpathOrigin && product.installDir) {
            return [
                FileInfo.joinPaths(
                    cpp.rpathOrigin,
                    FileInfo.relativePath(
                        FileInfo.joinPaths('/', product.installDir),
                        FileInfo.joinPaths('/', libDirName)))
            ];
        }
        return [];
    }
}
