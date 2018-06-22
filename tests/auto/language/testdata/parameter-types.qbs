Project {
    Product {
        name: "foo"
        qbsSearchPaths: "./erroneous"
        Depends { name: "module_with_parameters" }
        Depends {
            name: "bar"
            module_with_parameters.boolParameter: true
            module_with_parameters.intParameter: 156
            module_with_parameters.stringParameter: "hello"
            module_with_parameters.stringListParameter: ["la", "le", "lu"]
        }
    }
    Product {
        name: "bar"
    }
}
