import qbs 1.0

Product {
    type: "application"
    name: "pch"
    files: ["stable.h",
            "myobject.h",
            "main.cpp",
            "myobject.cpp"]
    Depends { name: "cpp" }
    cpp.precompiledHeader: "stable.h"
}

