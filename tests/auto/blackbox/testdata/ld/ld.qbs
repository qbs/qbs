import qbs

Project {
    Library {
        Depends { name: "cpp" }
        name: "coreutils"
        targetName: "qbs can handle any file paths, even the crazy ones! ;)"
        files: ["coreutils.cpp", "coreutils.h"]
        bundle.isBundle: false

        Properties {
            condition: qbs.targetOS.contains("darwin")
            cpp.sonamePrefix: "@rpath"
        }

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }

    CppApplication {
        Depends { name: "coreutils" }
        files: ["main.cpp"]
    }
}
