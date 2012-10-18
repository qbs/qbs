import qbs.base 1.0

Project {
    moduleSearchPaths: "subdir"
//    moduleSearchPaths: ["subdir"]
    Foo {
        files: "main.cpp"
    }
}

