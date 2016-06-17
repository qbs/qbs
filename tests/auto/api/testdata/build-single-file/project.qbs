import qbs

CppApplication {
    files: ["ignored1.cpp", "ignored2.cpp", "compiled.cpp"]

    Group {
        fileTagsFilter: ["application"]
        qbs.install: true
    }
}
