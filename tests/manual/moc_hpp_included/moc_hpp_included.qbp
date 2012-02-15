import qbs.base 1.0

Project {
    Product {
        type: "application"
        name: "moc_hpp_included"

        Depends { name: "qt.core" }

        files : [ "object.cpp" ]

        Group {
            files : [ "object.h" ]
            fileTags: [ "hpp" ]
        }
    }
}

