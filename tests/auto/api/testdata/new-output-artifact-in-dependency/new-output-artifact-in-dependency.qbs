import qbs

Project {
    DynamicLibrary {
        //Depends { name: "cpp" }
        name: "lib"
        files: "lib.cpp"
        bundle.isBundle: false
    }

    CppApplication {
        name: "app"
        files: "main.cpp"
        Depends { name: "lib" }
    }
}
