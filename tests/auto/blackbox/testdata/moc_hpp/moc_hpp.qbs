import qbs 1.0

Project {
    Product {
        type: "application"
        name: "moc_hpp"

        Depends { name: "qt.core" }

        files : [
            "object.h",
            "object.cpp"
        ]
    }
}

