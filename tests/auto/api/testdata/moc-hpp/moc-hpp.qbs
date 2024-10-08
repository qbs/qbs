Project {
    Product {
        type: "application"
        consoleApplication: true
        name: "moc_hpp"

        property bool dummy: { console.info("executable suffix: " + cpp.executableSuffix); }

        Depends { name: "Qt.core" }

        cpp.cxxLanguageVersion: "c++11"

        files : [
            "object.h",
            "object.cpp"
        ]
    }
}

