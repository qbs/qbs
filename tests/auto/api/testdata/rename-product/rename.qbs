Project {
    CppApplication {
        Depends { name: "TheLib" }
        cpp.defines: "MY_EXPORT="
        files: "main.cpp"
    }

    DynamicLibrary {
        name: "TheLib"
        Depends { name: "cpp" }
        cpp.defines: "MY_EXPORT=DLL_EXPORT"
        files: "lib.cpp"
        Properties {
            condition: qbs.targetOS.includes("darwin")
            bundle.isBundle: false
        }
    }
}
