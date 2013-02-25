import qbs.base 1.0

Project {
    CppApplication {
        name: "dep"
        files: "dep.cpp"
    }

    CppApplication {
        name: "app"
        Depends { name: "dep" }
        files: "main.cpp"
    }
}
