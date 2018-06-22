Project {
    Product {
        name: "a"
        multiplexByQbsProperties: ["architectures"]
        qbs.architectures: ["x86", "arm"]
    }
    Product {
        name: "b"
        Depends { name: "a" }
        multiplexByQbsProperties: ["architectures"]
        qbs.architectures: ["mips", "ppc"]
    }
}
