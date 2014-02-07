import qbs

CppApplication {
    name: "qbs-fuzzy-test"
    destinationDirectory: "bin"
    Depends { name: "Qt.core" }
    condition: Qt.core.versionMajor >= 5 // We use QDir::removeRecursively()
    consoleApplication: true
    files: [
        "commandlineparser.cpp",
        "commandlineparser.h",
        "fuzzytester.cpp",
        "fuzzytester.h",
        "main.cpp",
    ]
}
