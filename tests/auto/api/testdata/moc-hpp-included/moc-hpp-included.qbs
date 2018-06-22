Project {
    Product {
        type: "application"
        consoleApplication: true
        name: "moc_hpp_included"

        Depends { name: "Qt.core" }

        cpp.cxxLanguageVersion: "c++11"

        files: ["object.cpp", "object.h"]

        Group {
            condition: qbs.targetOS.contains("darwin")
            files: ["object2.mm", "object2.h"]
        }
    }
}

