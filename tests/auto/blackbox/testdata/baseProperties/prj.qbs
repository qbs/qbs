import qbs 1.0
import "imports/Foo.qbs" as Foo

Project {
    Foo {
        cpp.defines: base.concat(["FROM_PRJ"]);
        files: "main.cpp"
    }
}

