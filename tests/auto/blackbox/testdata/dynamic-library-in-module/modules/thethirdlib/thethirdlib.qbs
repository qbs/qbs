import qbs.FileInfo

Module {
    Depends { name: "cpp" }
    property string baseDir: FileInfo.cleanPath(FileInfo.joinPaths(path, "..", ".."))
    Group {
        name: "thethirdlib dll"
        condition: false
        files: FileInfo.joinPaths(product.theotherlib.baseDir, cpp.dynamicLibraryPrefix
                                  + "thethirdlib" + cpp.dynamicLibrarySuffix)
        fileTags: ["dynamiclibrary"]
        filesAreTargets: true
    }
    Group {
        name: "thethirdlib dll import"
        condition: false
        files: FileInfo.joinPaths(product.thethirdlib.baseDir, "thethirdlib.lib")
        fileTags: ["dynamiclibrary_import"]
        filesAreTargets: true
    }
}
