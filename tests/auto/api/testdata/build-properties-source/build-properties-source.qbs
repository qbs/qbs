Project {
    Product {
        type: "application"
        consoleApplication: true
        name: "HelloWorld"

        property bool dummy: { console.info("executable suffix: " + cpp.executableSuffix); }

        Depends { name: 'cpp' }

        Group {
            cpp.defines: ['WORLD="BANANA"']
            files : [ "main.cpp" ]
        }
    }
}

