Project {
    Profile {
        name: "profile1"
        qbs.architecture: "dummy"
    }
    Profile {
        name: "profile2"
        qbs.architecture: "blubb"
    }
    Profile {
        name: "profile3"
        qbs.architecture: "hurz"
    }
    Profile {
        name: "profile4"
        qbs.architecture: "zonk"
    }
    Product {
        name: "p1"
        qbs.profiles: ["profile1", "profile2"]
        Depends { name: "p2" }
    }
    Product {
        name: "p2"
        qbs.profiles: ["profile3", "profile4"]
    }
}
