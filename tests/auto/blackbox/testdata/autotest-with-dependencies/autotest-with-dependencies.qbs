import qbs.FileInfo

Project {
    CppApplication {
        condition: {
            var result = qbs.targetPlatform === qbs.hostPlatform;
            if (!result)
                console.info("targetPlatform differs from hostPlatform");
            return result;
        }
        name: "helper-app"
        type: ["application", "test-helper"]
        consoleApplication: true
        install: true
        files: "helper-main.cpp"
        cpp.executableSuffix: ".exe"
        Group {
            fileTagsFilter: "application"
            fileTags: "test-helper"
        }
    }
    CppApplication {
        name: "test-app"
        type: ["application", "autotest"]
        Depends { name: "autotest" }
        files: "test-main.cpp"
    }

    AutotestRunner {
        Depends {
            name: "cpp" // Make sure build environment is set up properly.
            condition: qbs.hostOS.contains("windows") && qbs.toolchain.contains("gcc")
        }
        arguments: FileInfo.joinPaths(qbs.installRoot, qbs.installPrefix, "bin")
        auxiliaryInputs: "test-helper"
    }
}
