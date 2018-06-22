Project {
    Product {
        name: "a"
    }
    Product {
        name: "b"
        multiplexByQbsProperties: ["architectures", "buildVariants"]
        qbs.architectures: ["mips", "vax"]
        qbs.buildVariants: ["debug", "release"]
    }
    Product {
        name: "c"
    }
}
