Application {
    Depends { name: "Qt.core" }
    files: ["main.cpp"]
    Group {
        files: ["foo.h"]
        fileTags: ["unmocable"]
        overrideTags: false
    }
}
