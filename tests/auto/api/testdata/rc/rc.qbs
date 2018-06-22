Application {
    type: "application"
    consoleApplication: true
    name: "rctest"

    Depends { name: 'cpp' }

    files: [
        "main.cpp",
        "test.rc"
    ]
}

