import qbs 1.0

Product {
    type: "application"
    consoleApplication: true
    name: "MyApp"
    files: ["stable.h",
            "myobject.h",
            "main.cpp",
            "myobject.cpp"]
    Depends { name: "cpp" }
    cpp.precompiledHeader: "stable.h"
    Depends { name: "Qt.core" }
}

