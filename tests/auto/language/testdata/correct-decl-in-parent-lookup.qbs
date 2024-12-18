Product {
    name: "p"
    Depends { name: "dummy" }
    dummy.cxxFlags: base.concat(["x"])
}
