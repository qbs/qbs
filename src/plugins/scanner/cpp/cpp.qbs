import "../../qbsplugin.qbs" as QbsPlugin

QbsPlugin {
    Depends { name: "qbscore" }
    name: "qbs_cpp_scanner"
    files: [
        "../scanner.h",
        "cpp_global.h",
        "cppscannerplugin.cpp"
    ]
}
