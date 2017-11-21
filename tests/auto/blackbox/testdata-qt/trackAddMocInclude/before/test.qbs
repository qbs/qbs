import qbs 1.0

Application {
    Depends { name: "Qt.core" }
    cpp.cxxLanguageVersion: "c++11"
    files: ["main.cpp"]
}

