import qbs 1.0

Application {
    type: "application"
    consoleApplication: true
    name: "rc"

    Depends { name: 'cpp' }

    files: [
        "main.cpp",
        "test.rc"
    ]
}

