Project {
    Product {
        type: "application"
        consoleApplication: true
        name: "moc_cpp"
        property bool dummy: { console.info("executable suffix: " + cpp.executableSuffix); }

        Depends {
            name: "Qt.core"
        }

        files: ["bla.cpp"]
    }
}
