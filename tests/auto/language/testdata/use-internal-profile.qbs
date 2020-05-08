Project {
    name: "theproject"

    Profile {
        name: "theprofile"
        dummy.defines: name
    }

    Product {
        name: "theproduct"
        Depends { name: "dummy" }
    }
}