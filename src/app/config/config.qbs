import qbs 1.0

QbsApp {
    name: "qbs-config"
    files: [
        "../shared/qbssettings.cpp",
        "../shared/qbssettings.h",
        "configcommand.h",
        "configcommandexecutor.cpp",
        "configcommandexecutor.h",
        "configcommandlineparser.cpp",
        "configcommandlineparser.h",
        "configmain.cpp"
    ]
}

