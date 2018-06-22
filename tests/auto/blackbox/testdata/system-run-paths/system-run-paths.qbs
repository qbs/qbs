Project {
    property bool setRunPaths
    Product {
        name: "theLib"
        type: ["dynamiclibrary"]
        Depends { name: "cpp" }
        qbs.installPrefix: ""
        Group {
            fileTagsFilter: product.type
            qbs.install: true
            qbs.installDir: "lib"
        }
        files: ["lib.cpp"]
    }

    CppApplication {
        name: "app"
        Depends { name: "theLib" }
        files: ["main.cpp"]
        cpp.rpaths: qbs.installRoot + "/lib"
        cpp.systemRunPaths: project.setRunPaths ? [qbs.installRoot + "/lib"] : []
    }
}
