import qbs 1.0

Project {
    Product {
        type: "application"
        consoleApplication: true
        name: "moc_hpp_included"

        Depends { name: "Qt.core" }

        files: ["object.cpp", "object.h"]

        Group {
            condition: qbs.targetOS.contains("darwin")
            files: ["object2.mm", "object2.h"]
        }
    }
}

