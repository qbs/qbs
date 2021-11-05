QbsApp {
    name: "qbs-setup-toolchains"
    cpp.dynamicLibraries: qbs.targetOS.contains("windows") ? base.concat("shell32") : base
    files: [
        "clangclprobe.cpp",
        "clangclprobe.h",
        "commandlineparser.cpp",
        "commandlineparser.h",
        "cosmicprobe.cpp",
        "cosmicprobe.h",
        "dmcprobe.cpp",
        "dmcprobe.h",
        "gccprobe.cpp",
        "gccprobe.h",
        "iarewprobe.cpp",
        "iarewprobe.h",
        "keilprobe.cpp",
        "keilprobe.h",
        "main.cpp",
        "msvcprobe.cpp",
        "msvcprobe.h",
        "probe.cpp",
        "probe.h",
        "sdccprobe.cpp",
        "sdccprobe.h",
        "watcomprobe.cpp",
        "watcomprobe.h",
        "xcodeprobe.cpp",
        "xcodeprobe.h",
    ]
    Group {
        name: "MinGW specific files"
        condition: qbs.toolchain.contains("mingw")
        files: "qbs-setup-toolchains.rc"
        Group {
            name: "qbs-setup-toolchains manifest"
            files: "qbs-setup-toolchains.exe.manifest"
            fileTags: [] // the manifest is referenced by the rc file
        }
    }
}
