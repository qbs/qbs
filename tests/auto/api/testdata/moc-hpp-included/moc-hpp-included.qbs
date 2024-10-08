Project {
    Product {
        type: "application"
        consoleApplication: true
        name: "moc_hpp_included"

        property bool dummy: { console.info("executable suffix: " + cpp.executableSuffix); }

        Depends { name: "Qt.core" }

        cpp.cxxLanguageVersion: "c++11"

        files: ["object.cpp", "object.h"]

        Group {
            condition: qbs.targetOS.includes("darwin")
            files: ["object2.mm", "object2.h"]
        }
    }
}

