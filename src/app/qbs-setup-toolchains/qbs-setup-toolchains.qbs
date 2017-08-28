import qbs 1.0

QbsApp {
    name: "qbs-setup-toolchains"
    cpp.dynamicLibraries: qbs.targetOS.contains("windows") ? base.concat("shell32") : base
    files: [
        "commandlineparser.cpp",
        "commandlineparser.h",
        "main.cpp",
        "msvcprobe.cpp",
        "msvcprobe.h",
        "probe.cpp",
        "probe.h",
        "xcodeprobe.cpp",
        "xcodeprobe.h",
    ]
    Group {
        name: "MinGW specific files"
        condition: qbs.toolchain.contains("mingw")
        files: ["qbs-setup-toolchains.exe.manifest", "qbs-setup-toolchains.rc"]
    }
}
