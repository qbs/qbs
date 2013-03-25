import qbs 1.0

Project {
    Product {
        type: ["application"]
        name: "HelloWorld"
        destinationDirectory: "bin"

        Depends { name: "Qt.core"}

        Group {
            qbs.install: true
            qbs.installDir: "share"
            files: ['main.cpp']
        }
    }
}

