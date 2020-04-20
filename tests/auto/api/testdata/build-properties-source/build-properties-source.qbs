Project {
    Product {
        type: "application"
        consoleApplication: true
        name: "HelloWorld"

        Depends { name: 'cpp' }

        Group {
            cpp.defines: ['WORLD="BANANA"']
            files : [ "main.cpp" ]
        }
    }
}

