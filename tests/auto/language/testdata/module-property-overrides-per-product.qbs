Project {
    Product {
        Depends { name: "dummy" }
        name: "a"
        property stringList rpaths: dummy.rpaths
    }
    Product {
        Depends { name: "dummy" }
        name: "b"
        property stringList rpaths: dummy.rpaths
    }
    Product {
        Depends { name: "dummy" }
        name: "c"
        property stringList rpaths: dummy.rpaths
    }
}
