import qbs

Product {
    name: "p"
    multiplexByQbsProperties: "architectures"
    aggregate: false
    qbs.architectures: ["x86", "arm", "x86"]
}
