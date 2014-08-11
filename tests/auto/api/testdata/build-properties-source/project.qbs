import qbs 1.0

Project {
    Product {
        type: "application"
        name: "HelloWorld"

        Depends { name: 'cpp' }

        Group {
            cpp.defines: ['WORLD="BANANA"']
            files : [ "main.cpp" ]
        }
    }
}

