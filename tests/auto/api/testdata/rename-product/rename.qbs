import qbs

Project {
    CppApplication {
        Depends { name: "TheLib" }
        cpp.defines: "MY_EXPORT="
        files: "main.cpp"
    }

    DynamicLibrary {
        name: "TheLib"
        Depends { name: "cpp" }
        cpp.defines: "MY_EXPORT=EXPORT"
        files: "lib.cpp"
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
    }
}
