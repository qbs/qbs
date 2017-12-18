Project {
    property string targetOS
    property string toolchain
    Product {
        name: "p"
        multiplexByQbsProperties: ["profiles"]
        qbs.profiles: ["theProfile"]
        Depends { name: "multiple_backends" }
        Profile {
            name: "theProfile"
            qbs.targetOS: [project.targetOS]
            qbs.toolchain: [project.toolchain]
            multiple_backends.backend3Prop: "value"
        }
    }
}
