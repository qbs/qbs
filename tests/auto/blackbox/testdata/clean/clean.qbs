import qbs 1.0

Project {
    DynamicLibrary {
        Depends { name: "cpp" }
        Depends { name: "Qt.core" }
        version: "1.1.0"
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
