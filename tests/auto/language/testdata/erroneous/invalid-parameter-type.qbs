Product {
    Depends { name: "module_with_parameters" }
    Depends {
        name: "readonly"
        module_with_parameters.boolParameter: "This is not an error."
        module_with_parameters.stringParameter: 123
    }
}
