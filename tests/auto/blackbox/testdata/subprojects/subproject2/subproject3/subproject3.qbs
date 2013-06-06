import qbs

Project {
    condition: false
    property string libNameSuffix: "blubb"
    DynamicLibrary {
        name: project.libNamePrefix + project.libNameSuffix
        Depends { name: "cpp" }
        Depends { name: "Qt.core" }
        cpp.defines: "MY_EXPORT=Q_DECL_EXPORT"
        files: "testlib.cpp"
        Export { Depends { name: "Qt.core" } }
    }
}
