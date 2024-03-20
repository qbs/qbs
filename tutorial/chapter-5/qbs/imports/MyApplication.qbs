import qbs.FileInfo

CppApplication {
    version: "1.0.0"
    consoleApplication: true
    install: true
    installDebugInformation: true

    cpp.rpaths: {
        if (!cpp.rpathOrigin)
            return [];
        return [
            FileInfo.joinPaths(
                cpp.rpathOrigin,
                FileInfo.relativePath(
                    FileInfo.joinPaths("/", product.installDir),
                    FileInfo.joinPaths("/", "lib")))
        ];
    }
}
