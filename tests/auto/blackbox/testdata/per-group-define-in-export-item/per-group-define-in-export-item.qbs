Project {
    Product {
        name: "dep"
        Export {
            Depends { name: "cpp" }
            Group {
                cpp.defines: ["MAIN"]
                files: ["main.cpp"]
            }
        }
    }
    Application {
        name: "app"
        Depends { name: "dep" }
    }
}
