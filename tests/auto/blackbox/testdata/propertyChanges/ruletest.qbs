import qbs

Product {
    name: "ruletest"
    type: "test-output"
    Depends { name: "TestModule" }
    files: "test.in"
}
