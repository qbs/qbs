Project {
    CppApplication {
        name: "app"
    }
    DynamicLibrary {
        name: "lib"
        Properties {
            condition: qbs.targetOS.includes("darwin")
            bundle.isBundle: false
        }
    }
}
