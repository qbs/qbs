import qbs 1.0

Project {
    Product {
        type: ["application"]
        name: "HelloWorld"
        destination: "bin"

        Depends { name: "Qt.core"}

        Group {
            qbs.install: true
            qbs.installDir: "share"
            files: ['main.cpp']
        }
    }
}

