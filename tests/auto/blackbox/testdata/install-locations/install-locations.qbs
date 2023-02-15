Project {
    property bool dummy: {
        if (qbs.targetOS.includes("windows")) {
            console.info("is windows");
        } else if (qbs.targetOS.includes("darwin")) {
            console.info("is darwin");
            if (qbs.targetOS.includes("macos"))
                console.info("is mac");
        } else {
            console.info("is unix");
        }

        if (qbs.toolchain.includes("mingw"))
            console.info("is mingw");
    }
    CppApplication {
        name: "theapp"
        install: true
        installDebugInformation: true
        files: "main.cpp"
        cpp.separateDebugInformation: true
        Group {
            fileTagsFilter: "application"
            fileTags: "some-tag"
        }
    }
    DynamicLibrary {
        name: "thelib"
        install: true
        installImportLib: true
        installDebugInformation: true
        Depends { name: "cpp" }
        cpp.separateDebugInformation: true
        files: "thelib.cpp"
    }
    LoadableModule {
        name: "theplugin"
        install: true
        installDebugInformation: true
        Depends { name: "cpp" }
        cpp.separateDebugInformation: true
        files: "theplugin.cpp"
    }
}
