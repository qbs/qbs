Project {
    CppApplication {
        name: "app"
    }
    DynamicLibrary {
        name: "lib"
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
    }
}
