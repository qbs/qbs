import qbs.FileInfo

Module {
    Depends { name: "cpp" }
    property string baseDir: FileInfo.cleanPath(FileInfo.joinPaths(path, "..", ".."))
    cpp.rpaths: [product.theotherlib.baseDir]
    Group {
        name: "theotherlib dll"
        files: FileInfo.joinPaths(product.theotherlib.baseDir, cpp.dynamicLibraryPrefix
                                  + "theotherlib" + cpp.dynamicLibrarySuffix)
        fileTags: ["dynamiclibrary"]
        filesAreTargets: true
    }
    Group {
        name: "theotherlib dll import"
        condition: qbs.targetOS.contains("windows")
        files: FileInfo.joinPaths(product.theotherlib.baseDir, "theotherlib.lib")
        fileTags: ["dynamiclibrary_import"]
        filesAreTargets: true
    }
}
