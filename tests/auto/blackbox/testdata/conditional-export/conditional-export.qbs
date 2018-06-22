Project {
    property bool enableExport: false
    Product {
        name: "dep"
        Export {
            condition: project.enableExport
            Depends { name: "cpp" }
            cpp.defines: ["THE_DEFINE"]
        }
    }
    CppApplication {
        name: "theProduct"
        Depends { name: "dep" }
        files: "main.cpp"
    }
}
