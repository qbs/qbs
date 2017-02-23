import qbs

Project {
    Product {
        name: "dep"
        Export {
            Depends { name: "cpp" }
            Group { files: ["main.cpp"] }
        }
    }

    Application {
        name: "app"
        bundle.isBundle: false
        Depends { name: "dep" }
    }
}
