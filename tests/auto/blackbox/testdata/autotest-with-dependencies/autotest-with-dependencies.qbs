import qbs
import qbs.FileInfo

Project {
    CppApplication {
        name: "helper-app"
        type: ["application", "test-helper"]
        consoleApplication: true
        files: "helper-main.cpp"
        cpp.executableSuffix: ".exe"
        Group {
            fileTagsFilter: "application"
            fileTags: "test-helper"
            qbs.install: true
            qbs.installDir: "bin"
        }
    }
    CppApplication {
        name: "test-app"
        type: ["application", "autotest"]
        files: "test-main.cpp"
    }

    AutotestRunner {
        arguments: FileInfo.joinPaths(qbs.installRoot, qbs.installPrefix, "bin")
        auxiliaryInputs: "test-helper"
    }
}
