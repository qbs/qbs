import qbs 1.0
import qbs.Utilities

QbsApp {
    name: "qbs_app"
    Depends { name: "qbs resources" }
    targetName: "qbs"
    Depends {
        condition: Qt.core.staticBuild || qbsbuildconfig.staticBuild
        productTypes: ["qbsplugin"]
    }
    cpp.defines: base.concat([
        "QBS_VERSION=" + Utilities.cStringQuote(qbsversion.version),
        "QBS_RELATIVE_LIBEXEC_PATH=" + Utilities.cStringQuote(qbsbuildconfig.relativeLibexecPath),
        "QBS_RELATIVE_SEARCH_PATH=" + Utilities.cStringQuote(qbsbuildconfig.relativeSearchPath),
        "QBS_RELATIVE_PLUGINS_PATH=" + Utilities.cStringQuote(qbsbuildconfig.relativePluginsPath),
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
        "session.cpp",
        "session.h",
        "sessionpacket.cpp",
        "sessionpacket.h",
        "sessionpacketreader.cpp",
        "sessionpacketreader.h",
        "status.cpp",
        "status.h",
        "stdinreader.cpp",
        "stdinreader.h",
    ]
    Group {
        name: "parser"
        prefix: name + '/'
        files: [
            "commandlineoption.cpp",
            "commandlineoption.h",
            "commandlineoptionpool.cpp",
            "commandlineoptionpool.h",
            "commandlineparser.cpp",
            "commandlineparser.h",
            "commandpool.cpp",
            "commandpool.h",
            "commandtype.h",
            "parsercommand.cpp",
            "parsercommand.h",
        ]
    }
}

