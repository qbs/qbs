import qbs 1.0
import "imports/Foo.qbs" as Foo

Project {
    moduleSearchPaths: "subdir"
    Foo {
        files: "main.cpp"
    }
}

