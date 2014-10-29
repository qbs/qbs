import qbs 1.0

QbsApp {
    name: "qbs-setup-toolchains"
    cpp.dynamicLibraries: qbs.targetOS.contains("windows") ? base.concat("shell32") : base
    files: [
        "commandlineparser.cpp",
        "commandlineparser.h",
        "compilerversion.cpp",
        "compilerversion.h",
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

