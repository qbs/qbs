import qbs

Project {
    LoadableModule {
        Depends { name: "cpp" }
        Depends { name: "bundle" }
        bundle.isBundle: false
        name: "CoolPlugIn"
        files: ["exported.cpp", "exported.h"]

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }

    CppApplication {
        Depends { name: "cpp" }
        Depends { name: "CoolPlugIn"; cpp.link: false }
        Depends { name: "bundle" }
        bundle.isBundle: false
        name: "CoolApp"
        files: ["main.cpp"]

        cpp.cxxLanguageVersion: "c++11"
        cpp.dynamicLibraries: [qbs.targetOS.contains("windows") ? "kernel32" : "dl"]

        Properties {
            condition: qbs.targetOS.contains("unix") && !qbs.targetOS.contains("darwin")
            cpp.rpaths: ["$ORIGIN"]
        }

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }
}
