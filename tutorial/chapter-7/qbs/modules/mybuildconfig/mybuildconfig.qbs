import qbs.FileInfo

//! [1]
//! [0]
// qbs/modules/mybuildconfig.qbs
Module {
    property string appInstallDir: "bin"
    property string libDirName: "lib"
    property string libInstallDir: qbs.targetOS.contains("windows") ? appInstallDir : libDirName
    //! [0]

    Depends { name: "cpp" }

    property bool enableRPath: true
    property stringList libRPaths: {
        if (enableRPath && cpp.rpathOrigin && product.installDir) {
            return [FileInfo.joinPaths(cpp.rpathOrigin, FileInfo.relativePath(
                                           FileInfo.joinPaths('/', product.installDir),
                                           FileInfo.joinPaths('/', libInstallDir)))];
        }
        return [];
    }

    cpp.rpaths: libRPaths
}
//! [1]
