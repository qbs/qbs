import qbs

Project {
    Product { name: "dep"; type: "x" }
    Product {
        name: "p1"
        multiplexByQbsProperties: "architectures"
        qbs.architectures: ["a", "b"]
        aggregate: true
        Depends { productTypes: "x" }
        multiplexedType: "x"
    }
    Product {
        name: "p2"
        Depends { productTypes: "x" }
    }
}

