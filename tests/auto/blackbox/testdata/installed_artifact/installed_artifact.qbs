import qbs 1.0

Application {
    name: "installedApp"
    Depends { name: "cpp" }
    files: "main.cpp"
    qbs.installPrefix: "/usr"
    Group {
        fileTagsFilter: "application"
        qbs.install: true
        qbs.installDir: "bin"
    }
}
