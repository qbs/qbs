import qbs

Module {
    Depends { name: "inner" }

    property string something: inner.something
}
