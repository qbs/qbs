Project {
    CppApplication {
        Depends { name: "lib" }
        Depends { name: "mod" }
        name: "app"
        consoleApplication: true
        files: ["main.m"]
        cpp.frameworks: ["Foundation"]
        cpp.rpaths: [cpp.rpathOrigin]
        cpp.minimumMacosVersion: "10.6"
        bundle.infoPlist: ({
            "QBS": "org.qt-project.qbs.testdata.embedInfoPlist"
        })
        install: true
        installDir: ""
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
        install: true
        installDir: ""
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
