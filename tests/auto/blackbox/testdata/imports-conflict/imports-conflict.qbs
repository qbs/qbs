import qbs

Project {
    Product {
        name: "Utils"
    }
    Product {
        Depends { name: "Utils" }
        Depends { name: "themodule" }
    }
}
