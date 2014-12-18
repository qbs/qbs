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
        Depends { name: "Qt.core" }
        cpp.defines: "MY_EXPORT=Q_DECL_EXPORT"
        files: "lib.cpp"
        bundle.isBundle: false
    }
}
