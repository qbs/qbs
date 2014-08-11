import qbs 1.0

Project {
    Product {
        type: "application"
        name: "moc_cpp"

        Depends {
            name: "Qt.core"
        }

        files: ["bla.cpp"]
    }
}
