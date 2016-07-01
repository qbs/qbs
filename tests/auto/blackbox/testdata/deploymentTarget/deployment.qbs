import qbs

CppApplication {
    files: ["main.c"]
    cpp.minimumMacosVersion: "10.4"
    cpp.minimumIosVersion: "5.0"
    cpp.cFlags: ["-v"]
    cpp.linkerFlags: ["-v"]
}
