Project {
    Product {
        type: "application"
        consoleApplication: true
        name : "HelloWorldApp"
        property bool dummy: { console.info("executable suffix: " + cpp.executableSuffix); }
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

