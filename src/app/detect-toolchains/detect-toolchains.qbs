import qbs 1.0
import "../apptemplate.qbs" as QbsApp

QbsApp {
    name: "qbs-detect-toolchains"
    files: [
        "../shared/qbssettings.h",
        "main.cpp",
        "msvcprobe.cpp",
        "msvcprobe.h",
        "osxprobe.cpp",
        "osxprobe.h",
        "probe.cpp",
        "probe.h"
    ]
}

