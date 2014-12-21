import qbs

Project {
    LoadableModule {
        Depends { name: "cpp" }
        Depends { name: "bundle" }
        Depends { name: "Qt.core" }
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
        Depends { name: "CoolPlugIn" }
        Depends { name: "bundle" }
        Depends { name: "Qt.core" }
        bundle.isBundle: false
        name: "CoolApp"
        files: ["main.cpp"]

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }
}
