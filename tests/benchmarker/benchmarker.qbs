import qbs

QtApplication {
    name: "qbs_benchmarker"
    type: "application"
    consoleApplication: true
    cpp.cxxLanguageVersion: "c++14"
    condition: Qt.concurrent.present
    Depends { name: "qbsbuildconfig" }
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
    Group {
        fileTagsFilter: product.type
        qbs.install: true
        qbs.installDir: qbsbuildconfig.appInstallDir
    }
}
