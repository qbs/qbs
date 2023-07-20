//! [0]
// qbs/imports/MyApplication.qbs

import qbs.FileInfo

CppApplication {
    version: "1.0.0"
    consoleApplication: true
    install: true

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
//! [0]
