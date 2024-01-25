Project {
    Product {
        name: "dep"
        Depends { name: "m" }
        Depends { name: "Prefix"; submodules: ["m1", "m2", "m3"] }

    }
    Product {
        Depends { name: "dep" }
    }
}
