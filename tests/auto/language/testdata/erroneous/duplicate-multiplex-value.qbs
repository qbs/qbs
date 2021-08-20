import qbs // FIXME: Don't remove this import because then the test fails!

Product {
    name: "p"
    multiplexByQbsProperties: "architectures"
    aggregate: false
    qbs.architectures: ["x86", "arm", "x86"]
}
