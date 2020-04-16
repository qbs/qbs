Project {
    Profile {
        name: "profile1"
    }
    Profile {
        name: "profile2"
    }

    Product {
        name: "dep"
        qbs.profiles: ["profile1", "profile2"]
    }
    Product {
        name: "main"
        Depends { name: "dep"; profiles: ["profile47"]; }
    }
}
