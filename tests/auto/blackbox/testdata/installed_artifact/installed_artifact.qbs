Application {
    name: "installedApp"
    type: "application"
    consoleApplication: true
    Depends { name: "cpp" }
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
