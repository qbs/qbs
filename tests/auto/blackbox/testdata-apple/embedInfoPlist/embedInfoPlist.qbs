import qbs

Project {
    CppApplication {
        Depends { name: "lib" }
        Depends { name: "mod" }
        name: "app"
        consoleApplication: true
        files: ["main.m"]
        cpp.frameworks: ["Foundation"]
        cpp.rpaths: ["@loader_path"]
        cpp.minimumMacosVersion: "10.5"
        bundle.infoPlist: ({
            "QBS": "org.qt-project.qbs.testdata.embedInfoPlist"
        })
        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }

    DynamicLibrary {
        Depends { name: "cpp" }
        name: "lib"
        bundle.isBundle: false
        bundle.embedInfoPlist: true
        files: ["main.m"]
        cpp.frameworks: ["Foundation"]
        cpp.sonamePrefix: "@rpath"
        bundle.infoPlist: ({
            "QBS": "org.qt-project.qbs.testdata.embedInfoPlist.dylib"
        })
        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }

    LoadableModule {
        Depends { name: "cpp" }
        name: "mod"
        bundle.isBundle: false
        bundle.embedInfoPlist: true
        files: ["main.m"]
        cpp.frameworks: ["Foundation"]
        bundle.infoPlist: ({
            "QBS": "org.qt-project.qbs.testdata.embedInfoPlist.bundle"
        })
        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }
}
