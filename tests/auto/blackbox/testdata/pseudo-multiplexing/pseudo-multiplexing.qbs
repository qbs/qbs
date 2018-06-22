Project {
    Product {
        name: "a"
        multiplexByQbsProperties: ["architectures"]
        Depends { name: "cpp" }
    }
    Product {
        name: "b"
        multiplexByQbsProperties: []
        Depends { name: "cpp" }
    }
}
