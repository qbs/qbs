Project {
    CppApplication {
        name: "public_tool"
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
        Depends { name: "mylib" }
        files: ["main.cpp"]
        Group {
            fileTagsFilter: product.type
            qbs.install: true
            qbs.installDir: "bin"
        }
    }
    CppApplication {
        name: "internal_tool"
        Depends { name: "mylib" }
        files: ["main.cpp"]
    }
    DynamicLibrary {
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
        Depends { name: "cpp" }
        name: "mylib"
        files: ["lib.cpp"]
        Group {
            name: "public headers"
            files: ["lib.h"]
            qbs.install: true
            qbs.installDir: "include"
        }
        Group {
            fileTagsFilter: product.type
            qbs.install: true
            qbs.installDir: "lib"
        }
    }

    InstallPackage {
        archiver.type: "tar"
        builtByDefault: true
        name: "tar-package"
        Depends { name: "public_tool" }
        Depends { name: "mylib" }
    }
}
