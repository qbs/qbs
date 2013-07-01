import qbs 1.0
import "../apptemplate.qbs" as QbsApp
import "../../../version.js" as Version

QbsApp {
    name: "qbs"
    cpp.defines: ["QBS_VERSION=\"" + Version.qbsVersion() + "\""]
    files: [
        "../shared/qbssettings.h",
        "application.cpp",
        "application.h",
        "commandlinefrontend.cpp",
        "commandlinefrontend.h",
        "consoleprogressobserver.cpp",
        "consoleprogressobserver.h",
        "ctrlchandler.cpp",
        "ctrlchandler.h",
        "main.cpp",
        "qbstool.cpp",
        "qbstool.h",
        "showproperties.cpp",
        "showproperties.h",
        "status.cpp",
        "status.h",
    ]
    Group {
        name: "parser"
        prefix: name + '/'
        files: [
            "command.cpp",
            "command.h",
            "commandlineoption.cpp",
            "commandlineoption.h",
            "commandlineoptionpool.cpp",
            "commandlineoptionpool.h",
            "commandlineparser.cpp",
            "commandlineparser.h",
            "commandpool.cpp",
            "commandpool.h",
            "commandtype.h",
        ]
    }
}

