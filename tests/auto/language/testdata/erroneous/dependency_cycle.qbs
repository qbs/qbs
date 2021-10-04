Project {
    Product {
        name: "A"
        Depends { name: "B" }
        files: ["main.cpp"]
    }
    Product {
        name: "B"
        Depends { name: "C" }
        files: ["main.cpp"]
    }
    Product {
        name: "C"
        Depends { name: "A" }
        files: ["main.cpp"]
    }
}
