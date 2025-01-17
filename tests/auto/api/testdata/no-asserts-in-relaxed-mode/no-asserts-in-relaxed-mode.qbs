Project {
    Product {
        name: "p"
        Depends { name: "dep" }
    }
    Product {
        name: "dep"
        property bool p: { throw "oops"; }
    }
}
