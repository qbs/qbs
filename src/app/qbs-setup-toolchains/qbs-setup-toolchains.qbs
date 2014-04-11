import qbs 1.0
import "../apptemplate.qbs" as QbsApp

QbsApp {
    name: "qbs-setup-toolchains"
    cpp.dynamicLibraries: qbs.targetOS.contains("windows") ? base.concat("shell32") : base
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
        "vsenvironmentdetector.cpp",
        "vsenvironmentdetector.h",
        "xcodeprobe.cpp",
        "xcodeprobe.h"
    ]
}

