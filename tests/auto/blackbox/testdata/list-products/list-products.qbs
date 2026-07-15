Project {
    Product {
        name: "a"
        type: ["tag1"]
    }
    Product {
        name: "b"
        multiplexByQbsProperties: ["architectures", "buildVariants"]
        qbs.architectures: ["mips", "vax"]
        qbs.buildVariants: ["debug", "release"]
        type: ["tag2"]
    }
    Product {
        name: "c"
        type: ["tag1", "tag3"]
    }
}
