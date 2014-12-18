import qbs 1.0

Project {
    Product {
        name: 'someapp'
        type: 'application'
        consoleApplication: true
        Depends { name: 'cpp' }
        files: [
            "main.cpp",
            "narf.h", "narf.cpp",
            "zort.h", "zort.cpp"
        ]
    }
}

