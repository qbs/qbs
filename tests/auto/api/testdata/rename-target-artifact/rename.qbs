Project {
    CppApplication {
        Depends { name: "TheLib" }
        cpp.defines: "MY_EXPORT="
        qbs.buildVariant: "release"
        files: "main.cpp"
    }

    DynamicLibrary {
        name: "TheLib"
        targetName: "the_lib"
        Depends { name: "cpp" }
        cpp.defines: "MY_EXPORT=DLL_EXPORT"
        qbs.buildVariant: "release"
        files: "lib.cpp"
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
    }
}
