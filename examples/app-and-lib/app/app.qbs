import qbs 1.0

Product {
    type: "application"
    name : "app-and-lib-app"
    files : [ "main.cpp" ]
    Depends { name: "cpp" }
    Depends { name: "app-and-lib-lib" }
}

