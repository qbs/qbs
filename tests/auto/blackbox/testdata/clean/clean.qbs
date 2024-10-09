Project {
    DynamicLibrary {
        Depends { name: "cpp" }
        version: "1.1.0"
        name: "dep"
        files: "dep.cpp"
        Properties {
            condition: qbs.targetOS.includes("darwin")
            bundle.isBundle: false
        }
    }

    CppApplication {
        consoleApplication: true
        name: "app"

        property bool dummy: {
            console.info("object suffix: " + cpp.objectSuffix);
            console.info("executable suffix: " + cpp.executableSuffix);
            console.info("is emscripten: " + qbs.toolchain.includes("emscripten"));
        }

        Depends { name: "dep" }
        files: "main.cpp"
    }
}
