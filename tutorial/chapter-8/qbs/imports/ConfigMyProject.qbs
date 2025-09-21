import qbs.FileInfo
Module {
    property bool enableAddressSanitizer: false
    Depends { name: "Sanitizers.address"; condition: enableAddressSanitizer }

    Depends { name: "cpp" }
    Depends { name: "config.install" }

    property bool enableRPath: true
    property stringList libRPaths: {
        if (enableRPath && cpp.rpathOrigin && product.installDir) {
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

