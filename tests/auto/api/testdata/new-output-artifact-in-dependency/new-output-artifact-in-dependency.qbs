Project {
    DynamicLibrary {
        //Depends { name: "cpp" }
        name: "lib"
        files: "lib.cpp"
        Properties {
            condition: qbs.targetOS.includes("darwin")
            bundle.isBundle: false
        }
    }

    CppApplication {
        name: "app"
        files: "main.cpp"
        Depends { name: "lib" }
    }
}
