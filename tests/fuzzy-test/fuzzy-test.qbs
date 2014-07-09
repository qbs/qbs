import qbs

QtApplication {
    name: "qbs_fuzzy-test"
    destinationDirectory: "bin"
    condition: Qt.core.versionMajor >= 5 // We use QDir::removeRecursively()
    type: "application"
    consoleApplication: true
    files: [
        "commandlineparser.cpp",
        "commandlineparser.h",
        "fuzzytester.cpp",
        "fuzzytester.h",
        "main.cpp",
    ]
}
