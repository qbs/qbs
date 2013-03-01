import qbs.base 1.0

Project {
    // Explicit type needed to prevent bundle generation on Mac.
    CppApplication {
        type: "application"
        name: "dep"
        files: "dep.cpp"
    }

    CppApplication {
        type: "application"
        name: "app"
        Depends { name: "dep" }
        files: "main.cpp"
    }
}
