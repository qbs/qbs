import qbs 1.0

QtApplication {
    name: "MyApp"
    consoleApplication: true
    Group {
        name: "precompiled headers"
        files: ["stable.h"]
        fileTags: ["cpp_pch_src"]
    }
    files: ["myobject.h", "main.cpp", "myobject.cpp"]
}
