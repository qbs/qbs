import qbs 1.0

Project {
    Product {
        type: "application"
        name : "HelloWorldApp"
        Depends { name: 'cpp' }
        Group {
            files : [ "main.cpp" ]
        }
    }

    Product {
        type: "staticlibrary"
        name : "HelloWorldLib"
        Depends { name: 'cpp' }
        Group {
            files : [ "main.cpp" ]
        }
    }
}

