Product {
    consoleApplication: true
    type: "application"
    name: "blubb_user"

    property bool dummy: { console.info("executable suffix: " + cpp.executableSuffix); }

    files: "main.cpp"

    Depends { name: "blubb_header" }
    Depends { name: "cpp" }
}
