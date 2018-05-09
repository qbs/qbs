import qbs

CppApplication {
    name: "the app"
    consoleApplication: true

    cpp.separateDebugInformation: false
    Properties {
        condition: qbs.targetOS.contains("macos")
        bundle.embedInfoPlist: false
    }

    files: "main.cpp"
    qbs.installPrefix: "/usr/local"
    Group {
        fileTagsFilter: "application"
        qbs.install: true
        qbs.installDir: "bin"
    }
}
