import qbs.base 1.0

Project {
    Product {
        type: ["application", "installed_content"]
        name: "HelloWorld"
        destination: "bin"

        Depends { name: "Qt.core"}

        files: [ "main.cpp" ]

        Group {
            qbs.installDir: "share"
            files: ['main.cpp']
            fileTags: ["install"]
        }
    }
}

