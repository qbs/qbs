import qbs

Project {
    DynamicLibrary {
        //Depends { name: "cpp" }
        name: "lib"
        files: "lib.cpp"
    }

    CppApplication {
        name: "app"
        files: "main.cpp"
        Depends { name: "lib" }
    }
}
