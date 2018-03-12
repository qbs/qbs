import qbs

QtApplication {
    name: "qbs_benchmarker"
    destinationDirectory: "bin"
    type: "application"
    consoleApplication: true
    cpp.cxxLanguageVersion: "c++11"
    condition: Qt.concurrent.present
    Depends {
        name: "Qt.concurrent"
        required: false
    }
    files: [
        "activities.h",
        "benchmarker-main.cpp",
        "benchmarker.cpp",
        "benchmarker.h",
        "commandlineparser.cpp",
        "commandlineparser.h",
        "exception.h",
        "runsupport.cpp",
        "runsupport.h",
        "valgrindrunner.cpp",
        "valgrindrunner.h",
    ]
}
