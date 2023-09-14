import qbs.Process 1.5

Project {
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

    Product {
        name: "theOtherProduct"
        property bool dummy: { throw "this one comes from a thread"; }
    }
}
