QbsStaticLibrary {
    Depends { name: "qbscore" }
    name: "qbsconsolelogger"
    files: [
        "coloredoutput.cpp",
        "coloredoutput.h",
        "consolelogger.cpp",
        "consolelogger.h"
    ]
    Export {
        Depends { name: "cpp" }
        cpp.includePaths: "."
    }
}
