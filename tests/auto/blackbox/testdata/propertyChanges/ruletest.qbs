import qbs

Product {
    type: "test-output"
    Depends { name: "TestModule" }
    files: "test.in"
}
