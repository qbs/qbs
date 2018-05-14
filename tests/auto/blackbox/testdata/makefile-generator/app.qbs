import qbs

CppApplication {
    name: "the app"
    consoleApplication: true

    cpp.cxxLanguageVersion: "c++11"
    cpp.separateDebugInformation: false
    Properties {
        condition: qbs.targetOS.contains("macos")
        bundle.embedInfoPlist: false
        cpp.minimumMacosVersion: "10.7"
    }

    files: "main.cpp"
    qbs.installPrefix: "/usr/local"
    Group {
        fileTagsFilter: "application"
        qbs.install: true
        qbs.installDir: "bin"
    }
}
