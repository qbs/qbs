Product {
    multiplexByQbsProperties: ["architectures"]
    qbs.architectures: ["a1", "a2"]
    Depends { name: "m2" }
}
