import qbs 1.0

Product {
    consoleApplication: true
    type: "application"
    name: "blubb_user"

    files: "main.cpp"

    Depends { name: "blubb_header" }
    Depends { name: "cpp" }
}
