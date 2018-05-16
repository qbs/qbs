Module {
    Depends { name: "m1" }
    m1.arch: qbs.architecture
    property string arch: qbs.architecture

    validate: {
        if (qbs.architecture !== "a1" && qbs.architecture !== "a2")
            throw "Unexpected arch " + qbs.architecture;
        if (arch !== qbs.architecture)
            throw "Oops: " + arch + "/" + qbs.architecture;
        if (m1.arch !== qbs.architecture)
            throw "Oops: " + m1.arch + "/" + qbs.architecture;
    }
}
