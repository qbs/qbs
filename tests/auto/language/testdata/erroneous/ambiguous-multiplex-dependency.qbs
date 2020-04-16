Project {
    Product {
        name: "a"
        multiplexByQbsProperties: ["architectures", "buildVariants"]
        qbs.architectures: ["x86", "arm"]
        qbs.buildVariants: ["debug", "release"]
    }
    Product {
        name: "b"
        Depends { name: "a" }
        multiplexByQbsProperties: ["architectures"]
        qbs.architectures: ["x86", "arm"]
    }
}
