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
    install: true
}
