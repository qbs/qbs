import qbs 1.0

Project {
    Product {
        type: "application"
        consoleApplication: true
        name: "moc_cpp"

        Depends {
            name: "Qt.core"
        }

        files: ["bla.cpp"]
    }
}
