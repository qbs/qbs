import qbs 1.0
import "imports/Foo.qbs" as Foo

Project {
    qbsSearchPaths: "subdir"
    Foo {
        files: "main.cpp"
    }
}

