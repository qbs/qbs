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
        consoleApplication: true
        property bool dummy: { console.info("executable suffix: " + cpp.executableSuffix); }
        Depends { name: "dep" }
    }
}
