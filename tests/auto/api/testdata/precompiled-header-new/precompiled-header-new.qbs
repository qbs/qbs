CppApplication {
    name: "MyApp"
    consoleApplication: true
    property bool dummy: { console.info("executable suffix: " + cpp.executableSuffix); }
    Group {
        name: "precompiled headers"
        files: ["stable.h"]
        fileTags: ["cpp_pch_src"]
    }
    files: ["myobject.h", "main.cpp", "myobject.cpp"]
}
