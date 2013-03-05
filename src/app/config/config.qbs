import qbs 1.0
import "../apptemplate.qbs" as QbsApp

QbsApp {
    name: "qbs-config"
    files: [
        "../shared/qbssettings.h",
        "configcommand.h",
        "configcommandexecutor.cpp",
        "configcommandexecutor.h",
        "configcommandlineparser.cpp",
        "configcommandlineparser.h",
        "configmain.cpp"
    ]
}

