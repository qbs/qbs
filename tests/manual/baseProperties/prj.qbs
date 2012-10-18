import qbs.base 1.0

Project {
    Foo {
        cpp.defines: base.concat(["FROM_PRJ"]);
        files: "main.cpp"
    }
}

