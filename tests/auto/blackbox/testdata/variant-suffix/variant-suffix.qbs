StaticLibrary {
    name: "l"

    Group {
        condition: qbs.targetOS.includes("darwin")
        Depends { name: "bundle" }
        Properties { bundle.isBundle: false }
    }

    aggregate: false
    property string variantSuffix
    property bool multiplex: false
    Properties {
        condition: multiplex
        qbs.buildVariants: ["debug", "release"]
        multiplexByQbsProperties: ["buildVariants"]
    }
    Properties {
        condition: variantSuffix !== undefined
        cpp.variantSuffix: variantSuffix
    }
    cpp.staticLibraryPrefix: "lib"
    cpp.staticLibrarySuffix: ".ext"

    qbs.installPrefix: ""
    install: true

    Depends { name: "cpp" }

    files: ["lib.cpp"]

    Probe {
        id: targetOSProbe
        property stringList targetOS: qbs.targetOS
        configure: {
            console.info("is Windows: " + targetOS.includes("windows"));
            console.info("is Apple: " + targetOS.includes("darwin"));
        }
    }
}
