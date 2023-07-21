Product {
    multiplexByQbsProperties: "architectures"
    qbs.architectures: ["x86", "x86_64", "arm"]
    property string archFromProbe: theProbe.archOut
    Probe {
        id: theProbe
        property string archIn: qbs.architecture
        property string archOut
        configure: { archOut = archIn; }
    }
    Group {
        name: "theGroup"
        qbs.sysroot: "/" + theProbe.archOut
    }
}
