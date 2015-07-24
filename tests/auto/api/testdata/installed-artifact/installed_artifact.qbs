import qbs 1.0


Project {
    CppApplication {
        name: "other app"
        files: ["main.cpp"]
    }

    CppApplication {
        name: "installedApp"
        type: "application"
        consoleApplication: true
        Depends { name: "other app" }
        Group {
            files: "main.cpp"
            qbs.install: true
            qbs.installDir: "src"
        }
        qbs.installPrefix: "/usr"
        Group {
            fileTagsFilter: "application"
            qbs.install: true
            qbs.installDir: "bin"
        }
    }
}
