Product {
    name: "ruletest"
    type: "test-output"
    Depends { name: "TestModule" }
    Group {
        files: "test.in"
        TestModule.testProperty: project.testProperty
    }
}
