import qbs

CppApplication {
    Depends { name: "Qt.core" }
    files: ["main.cpp", "object.h", "object.cpp"]
}

