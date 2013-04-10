import qbs 1.0

Project {
    Product {
        type: "application"
        name: "moc_hpp"

        Depends { name: "Qt.core" }

        files : [
            "object.h",
            "object.cpp"
        ]
    }
}

