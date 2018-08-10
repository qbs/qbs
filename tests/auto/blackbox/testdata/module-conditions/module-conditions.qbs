import qbs

Project {
    Product {
        name: "p1"
        qbs.architecture: "a"
        Depends { name: "m" }
    }
    Product {
        name: "p2"
        qbs.architecture: "b"
        Depends { name: "m" }
    }
    Product {
        name: "p3"
        multiplexByQbsProperties: "architectures"
        aggregate: false
        qbs.architectures: ["b", "c", "d"]
        Depends { name: "m" }
    }
}
