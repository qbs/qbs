import qbs

Project {
    CppApplication {
        Depends { name: "TheLib" }
        cpp.defines: "MY_EXPORT="
        files: "main.cpp"
    }

    DynamicLibrary {
        name: "TheLib"
        targetName: "the_lib"
        Depends { name: "cpp" }
        cpp.defines: "MY_EXPORT=EXPORT"
        files: "lib.cpp"
        bundle.isBundle: false
    }
}
