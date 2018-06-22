import qbs.Process 1.5

CppApplication {
    name: "theProduct"

    property bool moreFiles: false
    cpp.blubb: true

    files: ["file.cpp", "main.cpp"]
    Group {
        condition: moreFiles
        files: ["blubb.cpp"]
    }
}
