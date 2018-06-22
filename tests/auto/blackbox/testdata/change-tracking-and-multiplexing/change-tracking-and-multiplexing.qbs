StaticLibrary {
    name: "l"

    Depends { condition: qbs.targetOS.contains("darwin"); name: "bundle" }
    Properties { condition: qbs.targetOS.contains("darwin"); bundle.isBundle: false }

    multiplexByQbsProperties: ["buildVariants"]
    qbs.buildVariants: ["debug", "release"]
    Depends { name: "cpp" }
    files: ["lib.cpp"]
}
