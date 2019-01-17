Application {
    type: "application"
    consoleApplication: true
    name: "rctest"

    Depends { name: 'cpp' }

    cpp.includePaths: "subdir"

    files: [
        "main.cpp",
        "test.rc"
    ]
}

