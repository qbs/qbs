Project {
    Profile {
        name: "theProfile"
        qbs.architecture: "dummy"
    }
    Product {
        name: "p1"
        qbs.profiles: ["theProfile"]
    }
    Product {
        name: "p2"
        qbs.profiles: ["theProfile"]
        Depends { name: "p1" }
    }
}
