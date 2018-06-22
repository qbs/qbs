Project {
    Product {
        type: "application"
        consoleApplication: true
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

