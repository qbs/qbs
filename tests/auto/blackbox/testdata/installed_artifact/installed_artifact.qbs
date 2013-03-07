import qbs 1.0

Application {
    name: "installedApp"
    Depends { name: "cpp" }
    files: "main.cpp"
    Group {
        fileTagsFilter: "application"
        qbs.install: true
        qbs.installDir: "bin"
    }
}
