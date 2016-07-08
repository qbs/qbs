import qbs

QtApplication {
    name: "qbs_fuzzy-test"
    destinationDirectory: "bin"
    type: "application"
    consoleApplication: true
    cpp.cxxLanguageVersion: "c++11"
    files: [
        "commandlineparser.cpp",
        "commandlineparser.h",
        "fuzzytester.cpp",
        "fuzzytester.h",
        "main.cpp",
    ]
}
