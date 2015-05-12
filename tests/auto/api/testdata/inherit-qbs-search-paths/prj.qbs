import qbs 1.0
import "imports/Foo.qbs" as Foo

Project {
    qbsSearchPaths: "subdir"
    Project {
        qbsSearchPaths: "subdir2"
        Foo {
            files: "main.cpp"
        }
    }
}

