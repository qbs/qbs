import qbs 1.0

Project {
    Product {
        type: "application"
        name: "moc_hpp_included"

        Depends { name: "Qt.core" }

        files : [ "object.cpp" ]

        Group {
            files : [ "object.h" ]
            fileTags: [ "hpp" ]
        }
    }
}

