import qbs 1.0

Product {
    type: "application"
    name : "HelloWorld"
    files : [ "main.cpp" ]
    Depends { name: "cpp" }
    Depends { name: "lol" }
}

