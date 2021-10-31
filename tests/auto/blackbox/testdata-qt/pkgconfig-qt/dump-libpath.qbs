QtApplication {
    files: "main.cpp"
    property bool test: {
       console.info("libPath="+Qt.core.libPath)
    }
}
