import qbs

CppApplication {
    files: ["main.c"]
    cpp.minimumOsxVersion: "10.4"
    cpp.minimumIosVersion: "5.0"
    cpp.cFlags: ["-v"]
    cpp.linkerFlags: ["-v"]
}
