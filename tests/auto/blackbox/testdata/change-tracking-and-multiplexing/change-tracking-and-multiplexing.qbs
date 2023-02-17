StaticLibrary {
    name: "l"

    Depends { condition: qbs.targetOS.includes("darwin"); name: "bundle" }
    Properties { condition: qbs.targetOS.includes("darwin"); bundle.isBundle: false }

    multiplexByQbsProperties: ["buildVariants"]
    qbs.buildVariants: ["debug", "release"]
    Depends { name: "cpp" }
    files: ["lib.cpp"]
}
