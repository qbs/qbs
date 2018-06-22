Project {
    Product {
        name: "dep"
        multiplexByQbsProperties: ["buildVariants"]
        qbs.buildVariants: ["debug", "release"]
        property string includeDir: qbs.buildVariant === "debug" ? "/d" : "/r"
        Export {
            Depends { name: "cpp" }
            cpp.includePaths: product.includeDir
        }
    }
    Product {
        name: "p"
        Depends { name: "dep" }
        multiplexByQbsProperties: ["buildVariants"]
        qbs.buildVariants: ["debug", "release"]
    }
}
