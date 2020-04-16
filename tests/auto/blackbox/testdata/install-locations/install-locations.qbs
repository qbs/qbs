Project {
    property bool dummy: {
        if (qbs.targetOS.contains("windows")) {
            console.info("is windows");
        } else if (qbs.targetOS.contains("darwin")) {
            console.info("is darwin");
            if (qbs.targetOS.contains("macos"))
                console.info("is mac");
        } else {
            console.info("is unix");
        }
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
