import qbs 1.0
import "../apptemplate.qbs" as QbsApp

QbsApp {
    name: "qbs-setup-toolchains"
    files: [
        "../shared/qbssettings.h",
        "commandlineparser.cpp",
        "commandlineparser.h",
        "main.cpp",
        "msvcinfo.h",
        "msvcprobe.cpp",
        "msvcprobe.h",
        "probe.cpp",
        "probe.h",
        "xcodeprobe.cpp",
        "xcodeprobe.h"
    ]
}

