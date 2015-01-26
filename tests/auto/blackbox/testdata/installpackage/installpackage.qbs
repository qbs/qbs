import qbs

Project {
    QtApplication {
        name: "public_tool"
        Depends { name: "mylib" }
        files: ["main.cpp"]
        Group {
            fileTagsFilter: product.type
            qbs.install: true
            qbs.installDir: "bin"
        }
    }
    QtApplication {
        name: "internal_tool"
        Depends { name: "mylib" }
        files: ["main.cpp"]
    }
    DynamicLibrary {
        name: "mylib"
        Depends { name: "Qt.core" }
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
