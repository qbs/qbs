import qbs.FileInfo

//! [0]
// qbs/modules/mybuildconfig/mybuildconfig.qbs
Module {
    Depends { name: "cpp" }

    property string productVersion: "1.0.0"
    // ...
    //! [0]
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
