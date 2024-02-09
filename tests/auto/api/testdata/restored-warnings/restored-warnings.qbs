import qbs.Process 1.5

Project {
    CppApplication {
        name: "theProduct"

        cpp.blubb: true

        files: ["file.cpp", "main.cpp"]
    }

    Product {
        name: "theOtherProduct"
        property bool dummy: { throw "this one comes from a thread"; }
    }

    Product {
        name: "aThirdProduct"
        property bool moreFiles: false
        Group {
            condition: moreFiles
            files: ["blubb.txt"]
        }
    }
}
