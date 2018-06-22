Project {
    Profile {
        name: "profile1"
        qbs.architecture: "dummy"
    }
    Profile {
        name: "profile2"
        qbs.architecture: "blubb"
    }
    Product {
        name: "p1"
        qbs.profiles: ["profile1"]
        Depends { name: "p2"; profiles: ["profile1"] }
    }
    Product {
        name: "p2"
        qbs.profiles: ["profile1", "profile2"]
    }
}
