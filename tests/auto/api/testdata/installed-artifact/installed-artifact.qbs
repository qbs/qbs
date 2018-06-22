
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
        install: true
        installDir: "bin"
        Group {
            fileTagsFilter: "obj"
            qbs.install: true
            qbs.installDir: "objects"
        }
    }
}
