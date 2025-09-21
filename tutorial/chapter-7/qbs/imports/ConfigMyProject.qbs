//! [1]
import qbs.FileInfo
//! [0]
// qbs/imports/ConfigMyProject.qbs
Module {
    property bool enableAddressSanitizer: false
    Depends { name: "Sanitizers.address"; condition: enableAddressSanitizer }
    //! [0]

    Depends { name: "cpp" }
    Depends { name: "config.install" }

    property bool enableRPath: true
    property stringList libRPaths: {
        if (enableRPath && cpp.rpathOrigin && product.installDir) {
            console.info(FileInfo.joinPaths(
                    cpp.rpathOrigin,
                    FileInfo.relativePath(
                        FileInfo.joinPaths('/', product.installDir),
                        FileInfo.joinPaths('/', config.install.dynamicLibrariesDirectory))));
            return [
                FileInfo.joinPaths(
                    cpp.rpathOrigin,
                    FileInfo.relativePath(
                        FileInfo.joinPaths('/', product.installDir),
                        FileInfo.joinPaths('/', config.install.dynamicLibrariesDirectory)))];
        }
        return [];
    }

    cpp.rpaths: libRPaths
}
//! [1]
