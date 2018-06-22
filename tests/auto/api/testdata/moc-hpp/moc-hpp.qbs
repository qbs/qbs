Project {
    Product {
        type: "application"
        consoleApplication: true
        name: "moc_hpp"

        Depends { name: "Qt.core" }

        cpp.cxxLanguageVersion: "c++11"

        files : [
            "object.h",
            "object.cpp"
        ]
    }
}

