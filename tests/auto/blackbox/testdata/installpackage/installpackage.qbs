Project {
    CppApplication {
        name: "public_tool"
        Properties {
            condition: qbs.targetOS.includes("darwin")
            bundle.isBundle: false
        }
        Depends { name: "mylib" }
        files: ["main.cpp"]
        installDir: "bin"
        installDebugInformation: false
    }
    CppApplication {
        name: "internal_tool"
        Depends { name: "mylib" }
        files: ["main.cpp"]
    }
    DynamicLibrary {
        Properties {
            condition: qbs.targetOS.includes("darwin")
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
        installDir: "lib"
        installDebugInformation: false
    }

    InstallPackage {
        archiver.type: "tar"
        builtByDefault: true
        name: "tar-package"
        Depends { name: "public_tool" }
        Depends { name: "mylib" }
    }
}
