//! [1]
// app/app.qbs
import qbs.FileInfo

CppApplication {
    Depends { name: "mylib" }
    name: "My Application"
    targetName: "myapp"
    files: "main.c"
    version: "1.0.0"

    consoleApplication: true
    install: true

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
    //! [0]
}
//! [1]
