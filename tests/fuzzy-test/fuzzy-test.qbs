import qbs

QtApplication {
    name: "qbs_fuzzy-test"
    type: "application"
    consoleApplication: true
    Depends { name: "qbsbuildconfig" }
    cpp.cxxLanguageVersion: "c++14"
    files: [
        "commandlineparser.cpp",
        "commandlineparser.h",
        "fuzzytester.cpp",
        "fuzzytester.h",
        "main.cpp",
    ]
    Group {
        fileTagsFilter: product.type
        qbs.install: true
        qbs.installDir: qbsbuildconfig.appInstallDir
    }
}
