Project {
    Profile {
        name: "profile1"
    }

    Product {
        name: "dep"
        qbs.profiles: ["profile1"]
    }
    Product {
        name: "main"
        Depends { name: "dep"; profiles: ["profile47"]; }
    }
}
