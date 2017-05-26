import qbs

Project {
    property string importSymbol: qbs.targetOS.contains("windows") ? "__declspec(dllimport)" : ""
    property string importDefine: "DLLIMPORT=" + importSymbol
    property string exportSymbol: qbs.targetOS.contains("windows") ? "__declspec(dllexport)" : ""
    property string exportDefine: "DLLEXPORT=" + exportSymbol

    StaticLibrary {
        name: "staticlib 1"
        Depends { name: "cpp" }
        cpp.defines: [project.exportDefine]
        files: ["unused1.cpp", "used.cpp"]
    }
    StaticLibrary {
        name: "staticlib2"
        Depends { name: "cpp" }
        cpp.defines: [project.exportDefine]
        files: ["unused2.cpp"]
    }
    StaticLibrary {
        name: "staticlib3"
        Depends { name: "cpp" }
        cpp.defines: [project.exportDefine]
        files: ["unused3.cpp"]
    }
    StaticLibrary {
        name: "staticlib4"
        Depends { name: "cpp" }
        cpp.defines: [project.exportDefine]
        files: ["unused4.cpp"]
    }

    DynamicLibrary {
        name: "dynamiclib"
        property string linkWholeArchive
        Depends { name: "cpp" }
        Depends { name: "staticlib 1"; cpp.linkWholeArchive: product.linkWholeArchive }
        Depends { name: "staticlib2"; cpp.linkWholeArchive: product.linkWholeArchive }
        Depends { name: "staticlib3" }
        Depends { name: "staticlib4"; cpp.linkWholeArchive: product.linkWholeArchive }
        files: ["dynamiclib.cpp"]
    }

    CppApplication {
        name: "app1"
        Depends { name: "dynamiclib" }
        cpp.defines: [project.importDefine]
        files: ["main1.cpp"]
    }

    CppApplication {
        name: "app2"
        Depends { name: "dynamiclib" }
        cpp.defines: [project.importDefine]
        files: ["main2.cpp"]
    }
    CppApplication {
        name: "app3"
        Depends { name: "dynamiclib" }
        cpp.defines: [project.importDefine]
        files: ["main3.cpp"]
    }
    CppApplication {
        name: "app4"
        Depends { name: "dynamiclib" }
        cpp.defines: [project.importDefine]
        files: ["main4.cpp"]
    }
}
