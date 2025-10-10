Project {
    name: "Install-Locations"
    property bool dummy: {
        var toolchain = "unknown";
        if (qbs.toolchain.includes("emscripten")) {
            toolchain = "emscripten";
        } else if (qbs.targetOS.includes("darwin")) {
            if (qbs.targetOS.includes("macos"))
                toolchain = "macos";
            else
                toolchain = "darwin";
        } else if (qbs.targetOS.includes("windows")) {
            if (qbs.toolchain.includes("mingw"))
                toolchain = "mingw";
            else
                toolchain = "msvc";
        } else {
            toolchain = "unix";
        }
        console.info("===toolchain:" + toolchain + "===");
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
    LoadableModule {
        name: "theloadablemodule"
        Depends { name: "cpp" }
        cpp.separateDebugInformation: true
        files: "theplugin.cpp"
    }
    Project {
        name: "subproject"
        Plugin {
            name: "theplugin"
            Depends { name: "cpp" }
            cpp.separateDebugInformation: true
            files: "theplugin.cpp"
        }
    }
}
