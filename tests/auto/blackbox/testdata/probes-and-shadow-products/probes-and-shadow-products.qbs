Product {
    name: "p"
    multiplexByQbsProperties: "buildVariants"
    qbs.buildVariants: ["debug", "release"]
    Export {
        Probe {
            id: dummy
            configure: { found = true; }
        }
    }
}
