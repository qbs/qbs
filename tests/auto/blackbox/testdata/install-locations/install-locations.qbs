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
        if (qbs.toolchain.includes("emscripten"))
            console.info("is emscripten")
    }
    CppApplication {
        name: "theapp"
        files: "main.cpp"
        cpp.separateDebugInformation: true
        Group {
            fileTagsFilter: "application"
            fileTags: "some-tag"
        }
    }
    DynamicLibrary {
        name: "thelib"
        installImportLib: true
        Depends { name: "cpp" }
        cpp.separateDebugInformation: true
        files: "thelib.cpp"
    }
    Project {
        name: "subproject"
        LoadableModule {
            name: "theplugin"
            Depends { name: "cpp" }
            cpp.separateDebugInformation: true
            files: "theplugin.cpp"
        }
    }
}
