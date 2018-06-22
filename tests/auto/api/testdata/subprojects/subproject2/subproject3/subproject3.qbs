import LibraryType

Project {
    condition: false
    property string libNameSuffix: "blubb"
    Product {
        name: project.libNamePrefix + project.libNameSuffix
        type: LibraryType.type()
        Depends { name: "cpp" }
        Depends { name: "QtCoreDepender" }
        cpp.defines: "MY_EXPORT=Q_DECL_EXPORT"
        files: "testlib.cpp"
        Export { Depends { name: "cute.core" } }
    }
}
