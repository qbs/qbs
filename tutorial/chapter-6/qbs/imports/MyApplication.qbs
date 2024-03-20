import qbs.FileInfo

//! [0]
CppApplication {
    version: project.version
    consoleApplication: true
    install: true
    installDebugInformation: project.installDebugInformation
    // ...
    //! [0]

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
