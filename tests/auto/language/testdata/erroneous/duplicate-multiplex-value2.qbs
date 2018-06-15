import qbs

Product {
    name: "p"
    multiplexByQbsProperties: ["architectures", "buildVariants", "architectures"]
    aggregate: false
    qbs.architectures: ["x86", "arm"]
}
