import qbs

CppApplication {
    name: "HelloWorld-Qt"
    Depends { name: "Qt.core" }
    files: "main.cpp"
}
