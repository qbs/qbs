import qbs 1.0
import QbsFunctions

QbsApp {
    name: "qbs_app"
    Depends { name: "qbs resources" }
    targetName: "qbs"
    cpp.defines: base.concat([
        'QBS_VERSION="' + QbsFunctions.qbsVersion() + '"',
        'QBS_RELATIVE_LIBEXEC_PATH="' + qbsbuildconfig.relativeLibexecPath + '"',
        'QBS_RELATIVE_SEARCH_PATH="' + qbsbuildconfig.relativeSearchPath + '"',
        'QBS_RELATIVE_PLUGINS_PATH="' + qbsbuildconfig.relativePluginsPath + '"',
    ])
    files: [
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

