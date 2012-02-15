import qbs.base 1.0

Project {
    Product {
        type: "application"
        name : "HelloWorld"
        Depends { name: 'cpp' }
        Group {
            files : [ "main.cpp" ]
        }
    }

    Product {
        type: "staticlibrary"
        name : "HelloWorld"
        Depends { name: 'cpp' }
        Group {
            files : [ "main.cpp" ]
        }
    }
}

