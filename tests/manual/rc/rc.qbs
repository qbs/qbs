import qbs 1.0

Application {
    type: "application"
    name: "rc"

    Depends { name: 'cpp' }

    files: [
        "main.cpp",
        "test.rc"
    ]
}

