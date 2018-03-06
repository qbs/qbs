import qbs

CppApplication {
    name: "consumer"
    qbsSearchPaths: "default/install-root/usr/qbs"
    property string outTag: "cpp"
    Depends { name: "MyLib" }
    Depends { name: "MyTool" }
    files: ["consumer.cpp"]
    Group {
        files: ["helper.cpp.in"]
        fileTags: ["cpp.in"]
    }
}
